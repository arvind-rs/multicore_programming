
/*
 * Lock-free concurrent vector (dynamically resizable array) implementation in C++.
 * @author: ArvindRS
 * @date: 11/21/2017
 */

#include <iostream>
#include <atomic>
#include <pthread.h>
#include <cmath>
#include <chrono>

// Object to specify the write operation details for the helping thread
class WriteDesc {
public:
	bool pending;
	int old_value;
	int position;
	int new_value;
	WriteDesc(int nv, int ov, int pos) {
		new_value = nv;
		old_value = ov;
		position = pos;
		pending = true;
	}
};

// Object to specify the pending write operation
class Descriptor {
public:
	std::atomic<int> size;
	WriteDesc *write_op;
	Descriptor(int s, WriteDesc *w){size = s,write_op = w;}
};

// Lock-free vector class implementation
class Vector {

	// 2-level array of atomic variables
	typedef std::atomic<int> location;
	std::atomic<location *> memory[32];
	// Pointer to a descriptor object
	std::atomic<Descriptor *> descriptor;
	// Variable to specify the initial size of the array
	int first_bucket_size;

public:
	// Public constructor
	Vector() {
		Descriptor *temp = new Descriptor(0, NULL);
		descriptor = temp;
		first_bucket_size = 2;
		std::atomic<int> *array = new std::atomic<int>[first_bucket_size];
		memory[0] = array;
	}
	
	// Function to return the current size of arraylist
	int size() {
		Descriptor *current_descriptor = descriptor.load();
		if(current_descriptor->write_op != NULL && current_descriptor->write_op->pending)
			return current_descriptor->size - 1;
		return current_descriptor->size;
	}

	// Function to get the position at a given index in the arraylist
	std::atomic<int>* at(int i) {
		int pos = i + first_bucket_size;
		int hibit = highest_bit(pos);
		int index = pos ^ (int)std::pow(2, hibit);
		return &memory[hibit - highest_bit(first_bucket_size)][index];
	}

	// Function to get the highest set bit in a given integer
	int highest_bit(int num) {
		if(!num) return 0;
		return 31 - __builtin_clz(num);
	}


	// Function to push an element to the back of the vector
	void push_back(int data) {
		Descriptor *local_descriptor = descriptor.load();
		WriteDesc *write_op = new WriteDesc(data, 0, local_descriptor->size);
		Descriptor *new_descriptor = new Descriptor(local_descriptor->size + 1, write_op);
		do {
			local_descriptor = descriptor.load();
			// Take a local copy of the vector's descriptor and attempt to finish any pending writes before continuing with current
			// write operation.
			complete_write(local_descriptor->write_op);
			int bucket = highest_bit(local_descriptor->size + first_bucket_size) - highest_bit(first_bucket_size);
			// If the current bucket is full, allocate a new bucket twice the current size
			if(memory[bucket] == NULL)
				alloc_bucket(bucket);
			// Create a new write operation object that describes the details of the write operation to be performed and a new 
			// descriptor object which holds a reference to the write operation object and the new size of the vector
			write_op = new WriteDesc(data, 0, local_descriptor->size);
			new_descriptor = new Descriptor(local_descriptor->size + 1, write_op);
			// The following compare_exchange_weak() is the linearization point for the push_back() operation, with respect to the other 
			// push and pop operations.
			// If the thread fails to atomically swap the vector object's descriptor with it's descriptor object, then another thread has 
			// changed the state of the vector since the current thread obtained a local copy of the vector' descriptor and thus, has
			// to redo the entire process again.
		} while(!descriptor.compare_exchange_weak(local_descriptor,new_descriptor));
		complete_write(new_descriptor->write_op);
	}

	// Function to complete any pending writes
	void complete_write(WriteDesc *writeop) {
		// Skip if the pending is false for the write operation or it's NULL
		if(writeop != NULL && writeop->pending) {
			int pos = writeop->position + first_bucket_size;
			int hibit = highest_bit(pos);
			int index = pos ^ (int)std::pow(2, hibit);
			int old_value = memory[hibit - highest_bit(first_bucket_size)][index];
			// We atomically swap the old value, which is usually NULL, with the new value to be written and then mark the write operation
			// as complete by setting the pending flag to false
			memory[hibit - highest_bit(first_bucket_size)][index].compare_exchange_strong(old_value, writeop->new_value);
			writeop->pending = false;
		}
	}

	// Function to allocate a new bucket
	void alloc_bucket(int bucket) {
		long long new_bucket_size = std::pow(first_bucket_size,bucket+1);
		// For some strange reason, we need to copy the atomic object to a local variable before applying CAS
		// Else, you get this error;
		// error: invalid initialization of non-const reference of type ‘std::__atomic_base<int>::__int_type& {aka int&}’ 
		// from an rvalue of type ‘std::__atomic_base<int>::__int_type {aka int}’
		std::atomic<int> *temp_memory = memory[bucket];
		std::atomic<int> *array = new std::atomic<int>[new_bucket_size];
		if(!memory[bucket].compare_exchange_strong(temp_memory, array)) {
			delete array;
		}
	}

	// Function to display the contents of the vector
	void display() {
		Descriptor *local_descriptor = descriptor;
		for(int i = 0; i < local_descriptor->size; i++) {
			int pos = i + first_bucket_size;
			int hibit = highest_bit(pos);
			int index = pos ^ (int)std::pow(2, hibit);
			std::cout << memory[hibit - highest_bit(first_bucket_size)][index] << " ";
		}
		std::cout << std::endl;
	}

	// Function to pop an element from the back of the arraylist
	// The pop_back() operation uses a similar helping approach as the push_back() operation by completing any pending writes before 
	// popping the element and swapping in a new descriptor with size-1. 
	int pop_back() {
		Descriptor *local_descriptor = descriptor;
		if(local_descriptor->size == 0) return -1;
		Descriptor *new_descriptor = new Descriptor(local_descriptor->size-1,NULL);
		int data = *at(local_descriptor->size-1);
		do {
			local_descriptor = descriptor;
			complete_write(local_descriptor->write_op);
			data = *at(local_descriptor->size-1);
			new_descriptor = new Descriptor(local_descriptor->size-1,NULL);
			// The following compare_exchange_weak() is the linearization point for the pop_back() operation with respect to the other
			// push and pop operations. If this atomic swap failed, then that means another another had changed the state of the vector
			// since the time the current thread obtained a local copy of the vector's descriptor.
		} while(!descriptor.compare_exchange_weak(local_descriptor, new_descriptor));
		return data;
	}

	// Function to check if the arraylist is empty
	/* This function is wait-free as we can define the linearization point as the point at which the local copy of the descriptor 
	 * is successfully compared to the value 0.
	 */
	bool isEmpty() {
		Descriptor *local_descriptor = descriptor;
		return local_descriptor->size == 0;
	}
};


// Structure to pass on multiple parameters to the threaded function
struct MyArguments {
	int data;
	int execution_limit;
	int operation;
};

// Global Vector object
Vector v;

// Function to run the thread code
void *run(void *ptr) {
	
	// Get the arguments
	struct MyArguments *args = (MyArguments *)ptr;
	int data = args->data;
	long int execution_limit = args->execution_limit;
	int operation = args->operation;

	// Perform the operations for the given number of times
	for(int i = 1; i <= execution_limit; i++) {
		if(operation == 0)
			v.push_back(data);
		else
			v.pop_back();
	}
}


// Function implementing the test framework
void run_test(int thread_count, int limit, float split_r) {

	// Initialize variables
	int execution_limit = limit;

	// Used to split the threads into pushers and poppers
	float split_ratio = split_r;
	int split_count = thread_count * split_ratio;
	
	// Create the threads
	pthread_t thread[thread_count];

	// Create an arguments structure to pass on the arguments to the threads
	struct MyArguments args[thread_count];

	// Start the threads and start timing the execution time
	auto t1 = std::chrono::steady_clock::now();
	for(int i = 0; i < thread_count; i++) {
		args[i].data = i;
		args[i].execution_limit = execution_limit;		
		if(i < split_count)
			args[i].operation = 0;
		else
			args[i].operation = 1;
		pthread_create(&thread[i],NULL,run,(void *)&args[i]);
	}
	
	// Wait for the threads to complete
	for(int i = 0; i < thread_count; i++) {
		pthread_join(thread[i],NULL);
	}
	auto t2 = std::chrono::steady_clock::now();
   	auto elapsed = std::chrono::duration_cast< std::chrono::milliseconds>(t2 - t1);

   	// Display some metrics
   	std::cout << "no. of threads = " << thread_count << std::endl;
   	std::cout << "split ratio = " << split_ratio << std::endl;
   	std::cout << "execution count per thread = " << execution_limit << std::endl;
   	std::cout << "time elapsed = " << elapsed.count() << " milliseconds!" << std::endl;
}


// Main function
int main() {
	
	// Test cases
	for(int i = 8; i <= 8; i++) {
		std::cout << i << std::endl;
		run_test(i, 500000, 1);
		run_test(i, 500000, 0.5);
		run_test(i, 500000, 0.75);
	}
}
