#include <cstdlib>
#include <iostream>
#include <signal.h>
#include <pthread.h>
#include <api/api.hpp>
#include <common/platform.hpp>
#include <common/locks.hpp>
#include "bmconfig.hpp"
#include "../include/api/api.hpp"
#include <vector>
#include <iostream>
#include <atomic>
#include <pthread.h>
#include <cmath>
#include <chrono>
using namespace std;

const int THREAD_COUNT = 8;
const int NUM_TRANSACTIONS = 500000;
const int SPLIT_RATIO = 0.5;
// shared variable that will be incremented by transactions
int x = 0;

Config::Config() :
    bmname(""),
    duration(1),
    execute(0),
    threads(THREAD_COUNT),
    nops_after_tx(0),
    elements(256),
    lookpct(34),
    inspct(66),
    sets(1),
    ops(1),
    time(0),
    running(true),
    txcount(0)
{
}

Config CFG TM_ALIGN(64);

/*************************************************************************************/

class Vector {
    // Variable to specify the initial array size
    int first_bucket_size;
    // 2-level array to store the data
    int **memory = new int*[32];
    // Variable to track the size of the vector
    int size;
public:
    // Public constructor
    Vector() {
        size = 0;
        first_bucket_size = 2;
        memory[0] = new int[first_bucket_size];
    }

    // Function to get the highest set bit in a given integer
    int highest_bit(int num) {
        int result;
        TM_THREAD_INIT();
        TM_BEGIN(atomic) {
            // __builtin_clz() gives the count of leading zeros, which is used to indirectly get the number of the highest set bit
            // It'll not work with 0. Hence, 0 must be checked separately.
            if(!num) 
                result = 0;
            else
                result = 31 - __builtin_clz(num);
        } TM_END;
        TM_THREAD_SHUTDOWN();
        return result;
    }

    // Function to push an element to the tail of the vector
    // The biggest difference with the lock-free version is the lack of a descriptor object
    void push_back(int data) {
        TM_THREAD_INIT();
        TM_BEGIN(atomic) {
            int local_size = TM_READ(size);
            int local_first_bucket_size = TM_READ(first_bucket_size);
            int bucket = highest_bit(local_size + local_first_bucket_size) - highest_bit(local_first_bucket_size);
            if(TM_READ(memory[bucket]) == NULL) {
                int new_bucket_size = std::pow(local_first_bucket_size,bucket+1);
                memory[bucket] = new int[new_bucket_size];
            }
            int pos = local_size + local_first_bucket_size;
            int hibit = highest_bit(pos);
            int index = pos ^ (int)std::pow(2, hibit);
            TM_WRITE(memory[hibit - highest_bit(local_first_bucket_size)][index], data);
            TM_WRITE(size, local_size+1);
        } TM_END;
        TM_THREAD_SHUTDOWN();
    }

    // Function to remove an element from the tail of the vector
    int pop_back() {
        TM_THREAD_INIT();
        int data;
        TM_BEGIN(atomic) {
            int local_size = TM_READ(size);
            if(local_size == 0) return -1;
            int pos = local_size-1 + first_bucket_size;
            int hibit = highest_bit(pos);
            int index = pos ^ (int)std::pow(2, hibit);
            data = TM_READ(memory[hibit - highest_bit(first_bucket_size)][index]);
            TM_WRITE(size, local_size-1);
        } TM_END;
        TM_THREAD_SHUTDOWN();
        return data;
    }

    // Function to write a given element to a given position
    void write(int i, int data) {
        TM_THREAD_INIT();
        TM_BEGIN(atomic) {
            int local_first_bucket_size = TM_READ(first_bucket_size);
            int pos = i + first_bucket_size;
            int hibit = highest_bit(pos);
            int index = pos ^ (int)std::pow(2, hibit);
            TM_WRITE(memory[hibit - highest_bit(local_first_bucket_size)][index], data);
        } TM_END;
        TM_THREAD_SHUTDOWN();
    }

    // Function to read an element at a given position
    int read(int i) {
        int data;
        TM_THREAD_INIT();
        TM_BEGIN(atomic) {
            int pos = i + first_bucket_size;
            int hibit = highest_bit(pos);
            int index = pos ^ (int)std::pow(2, hibit);
            data = TM_READ(memory[hibit - highest_bit(first_bucket_size)][index]);
        } TM_END;
        TM_THREAD_SHUTDOWN();
        return data;
    }

    // Function to get the current size of the vector
    int get_size() {
        int local_size;
        TM_THREAD_INIT();
        TM_BEGIN(atomic) {
            local_size = TM_READ(size);
        } TM_END;
        TM_THREAD_SHUTDOWN();
        return local_size;
    }

    // Function to display the contents of the vector
    void display() {
        for(int i = 0; i < size; i++) {
            int pos = i + first_bucket_size;
            int hibit = highest_bit(pos);
            int index = pos ^ (int)std::pow(2, hibit);
            std::cout << memory[hibit - highest_bit(first_bucket_size)][index] << " ";
        }
        std::cout << std::endl;
    }
};

/*************************************************************************************/

// Structure to pass on multiple parameters to the threaded function
struct MyArguments {
    int data;
    int operation;
};

// Global Vector object
Vector v;

void* run_thread(void* i) {
    // each thread must be initialized before running transactions
    TM_THREAD_INIT();
    // Read the arguments passed to each thread
    struct MyArguments *args = (MyArguments *)i;
    int data = args->data;
    int operation = args->operation;
    for(int i=0; i<NUM_TRANSACTIONS; i++) {
        // mark the beginning of a transaction
        TM_BEGIN(atomic)
        {
            // Some threads are tasked with pushing to the vector while others are tasked with popping
            if(operation == 0)
                v.push_back(data);
            else
                v.pop_back();
        }
        TM_END; // mark the end of the transaction
    }

    TM_THREAD_SHUTDOWN();
}

int main(int argc, char** argv) {

    // Define the arguments for the run() method
    int thread_count = THREAD_COUNT;
    float split_ratio = SPLIT_RATIO; // Used to split the threads into pushers and poppers
    int split_count = thread_count * split_ratio;

    struct MyArguments args1[thread_count+1];

    TM_SYS_INIT();

    // original thread must be initalized also
    TM_THREAD_INIT();

    void* args[256];
    pthread_t tid[256];

    // set up configuration structs for the threads we'll create
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    for (uint32_t i = 0; i < CFG.threads; i++)
        args[i] = (void*)i;

    // Start timing the execution runtime
    auto t1 = std::chrono::steady_clock::now();
    // actually create the threads
    for (uint32_t j = 1; j < CFG.threads; j++) {
        args1[j].data = j;
        if((int)j < split_count)
            args1[j].operation = 0;
        else
            args1[j].operation = 1;
        pthread_create(&tid[j],&attr,run_thread,(void *)&args1[j]);
    }
    

    // all of the other threads should be queued up, waiting to run the
    // benchmark, but they can't until this thread starts the benchmark
    // too...
    run_thread((void *)&args1[0]);

    // everyone should be done.  Join all threads so we don't leave anything
    // hanging around
    for (uint32_t k = 1; k < CFG.threads; k++)
        pthread_join(tid[k], NULL);
    auto t2 = std::chrono::steady_clock::now();

    // And call sys shutdown stuff
    TM_SYS_SHUTDOWN();

    // Print the final metrics
    printf("x = %d\n", x); // x should equal (THREAD_COUNT * NUM_TRANSACTIONS)
    std::cout << "vector size = " << v.get_size() << std::endl;
    auto elapsed = std::chrono::duration_cast< std::chrono::milliseconds>(t2 - t1); 
    std::cout << "time elapsed = " << elapsed.count() << std::endl;

    return 0;
}
