
/*
 * Program to simulate the Dining Philosophers problem.
 * It consists of 5 threads (philosophers) and 5 shared variables (chopsticks).
 * This is a deadlock-free and starvation-free solution implemented using the Bakery algorithm. This solution takes a value, N for no. of philosophers,
 * from the command line.
 * @author: ArvindRS
 * @date: 09/06/2017
 */

#include <iostream>
#include <vector>
#include <ctime>
#include <pthread.h>
#include <sstream>
#include <numeric>
#include <algorithm>
#include <fstream>
#include <random>
#include <unistd.h>
#include <mutex>

using namespace std;


// Structure to pass on multiple parameters to the threaded function
struct MyArguments {
	int N;
	int philosopher_no;
	int term_signal;
	volatile bool *chopsticks;
	int chopsticks_size;
	volatile bool *entering;
	int entering_size;
	volatile int *numbering;
	int numbering_size;
	int eating_count;
};

// Function to get the max value in an array of numbers
int get_max_value(volatile int *arr, int size) {

	int max = -1;
	for(int i = 0; i < size; i++) {
		if(arr[i] > max)
			max = arr[i];
	}

	return max;
}

// Function that implements the special less than comparison defined by the Bakery algorithm
bool less_than(int numbering_a,int a,int numbering_b,int b) {
	if(numbering_a < numbering_b)
		return true;
	else if((numbering_a == numbering_b) && (a < b))
		return true;
	else
		return false;
}

// Function to simulate a philosopher
void *philosopher(void *ptr) {

	struct MyArguments *args = (MyArguments *) ptr;

	// Initialize some local variables
	int N = args->N;
	int number = args->philosopher_no;
	cout << "philosopher: " << number << endl;

	int state = 0;
	while(args->term_signal == 0) {

		if(state == 0) {
			printf("%d is now thinking.\n",number);
			state = 1;
		}
		if(state == 1) {

			printf("%d is now hungry.\n",number);


			// Acquire the lock
			args->entering[number] = true;
			args->numbering[number] = 1 + get_max_value(args->numbering,args->numbering_size);
			args->entering[number] = false;
			for(int j = 0; j < N; j++) {
				// Wait until thread j receives its number:
          		while (args->entering[j]) { /* nothing */ }
          		// Wait until all threads with smaller numbers or with the same number, but with higher priority, finish their work:
          		while ((args->numbering[j] != 0) && less_than(args->numbering[j], j, args->numbering[number], number)) { /* nothing */ }
			}
			// Access the chopsticks only if they are available
			printf("%d now is waiting for chopstick no: %d.\n",number,number);
			while(!args->chopsticks[number]) {};/* Wait for the left chopstick. */  
			args->chopsticks[number] = false;
			printf("%d now has chopstick no: %d.\n",number,number);
			printf("%d now is waiting for chopstick no: %d.\n",number,(number+1)%N);
			while(!args->chopsticks[(number+1)%N]) {} /* Wait for the right chopstick. */
			args->chopsticks[(number+1)%N] = false;
			printf("%d now has chopstick no: %d.\n",number,(number+1)%N);
			// Release the lock once the chopsticks have been acquired
			args->numbering[number] = 0;
					
			// Once the chopsticks have been acquired, time to dig in!
			printf("%d is now eating.\n",number);
			args->eating_count++;
			printf("%d has finished eating.\n",number);

			// Release the chopsticks
			args->chopsticks[number] = true;
			args->chopsticks[(number+1)%N] = true;
			state = 0;
		}
	}

	printf("%d is now leaving.\n",number);

}

// Main function
int main(int argc, char **argv) {

	// Get the limit from the command line
	cout << "No. of philosophers: " << argv[1] << endl;
	istringstream ss(argv[1]);
	int N;
	if(!(ss >> N)) {
		cout << "Invalid argument: " << argv[1] << endl;
		return 0;
	}

	// Create the threads
	pthread_t t[N];

	// Create the shared objects (chopsticks)
	volatile bool chopsticks[N];
	for(int i = 0; i < N; i++)
		chopsticks[i] = true;
	volatile bool entering[N];
	for(int i = 0; i < N; i++)
		entering[i] = false;
	volatile int numbering[N];
	for(int i = 0; i < N; i++)
		numbering[i] = 0;

	// Create the structure to pass arguments to the threads
	struct MyArguments args[N];
	for(int i = 0; i < N; i++) {
		args[i].N = N;
		args[i].philosopher_no = i;
		args[i].term_signal = 0;
		args[i].chopsticks = chopsticks;
		args[i].chopsticks_size = N;
		args[i].entering = entering;
		args[i].entering_size = N;
		args[i].numbering = numbering;
		args[i].numbering_size = N;
		args[i].eating_count = 0;
		pthread_create(&t[i],NULL,philosopher,(void*)&args[i]);
	}

	// Go into a loop until the user enters 'n'
	// Pressing 'n' will initiate graceful termination of the threads.
	char input = 'y';
	while(input != 'n') {
		cin >> input;
	}

	// Signal the threads to terminate
	for(int i = 0; i < N; i++) {
		args[i].term_signal = 1;
	}

	// Wait for the threads to terminate
	for(int i = 0; i < N; i++) {
		pthread_join(t[i],NULL);
	}

	// Print how many times each philosopher ate
	for(int i = 0; i < N; i++) {
		printf("Philosopher %d ate %d times!\n",args[i].philosopher_no,args[i].eating_count);
	}

	return 0;
}