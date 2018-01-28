
/*
 * Program to simulate the Dining Philosophers problem.
 * It consists of 5 threads (philosophers) and 5 shared variables (chopsticks).
 * This is a naive solution which can potentially have deadlocks and starvation. 
 * @author: ArvindRS
 * @date: 09/05/2017
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

// Global lock
std::mutex m;

// Structure to pass on multiple parameters to the threaded function
struct MyArguments {
	int N;
	int philosopher_no;
	int term_signal;
	volatile bool *chopsticks;
	int chopsticks_size;
	int eating_count;
};

// Function to simulate a philosopher
void *philosopher(void *ptr) {

	struct MyArguments *args = (MyArguments *) ptr;

	int N = args->N;
	int number = args->philosopher_no;
	cout << "philosopher: " << number << endl;

	int state = 0;
	while(args->term_signal == 0) {
		//int state = rand() % states;
		if(state == 0) {
			printf("%d is now thinking.\n",number);
			//sleep(1);
			state = 1;
		}
		if(state == 1) {

			printf("%d is now hungry.\n",number);
			bool hungry = true;
			while(hungry) {
				// Access the chopsticks only if they are available
				m.lock();
				printf("%d now is waiting for chopstick no: %d.\n",number,number);
				if(args->chopsticks[number]) {
					args->chopsticks[number] = false;
					printf("%d now has chopstick no: %d.\n",number,number);
				}
				m.unlock();
				m.lock();
				printf("%d now is waiting for chopstick no: %d.\n",number,(number+1)%N);
				if(args->chopsticks[(number+1)%N]) {
					args->chopsticks[(number+1)%N] = false;
					printf("%d now has chopstick no: %d.\n",number,(number+1)%N);
				}
				m.unlock();

				// Once the chopsticks have been acquired, time to dig in!	
				printf("%d is now eating.\n",number);
				args->eating_count++;
				printf("%d has finished eating.\n",number);

				// Release the chopsticks
				args->chopsticks[number] = true;
				args->chopsticks[(number+1)%N] = true;
				hungry = false;
				state = 0;
			}
			
		}
		//int sleep_duration = 1 + rand() % N;
		//sleep(sleep_duration);
	}

	cout << number << " is now leaving!" << endl;

}

// Main function
int main() {

	// Initialize 5 philosophers
	int N = 5;

	// Create the threads
	pthread_t t[N];

	// Create the shared objects (chopsticks)
	volatile bool chopsticks[N];
	for(int i = 0; i < N; i++)
		chopsticks[i] = true;

	// Create the structure to pass arguments to the threads
	struct MyArguments args[N];
	for(int i = 0; i < N; i++) {
		args[i].N = N;
		args[i].philosopher_no = i;
		args[i].term_signal = 0;
		args[i].chopsticks = chopsticks;
		args[i].chopsticks_size = N;
		args[i].eating_count = 0;
		pthread_create(&t[i],NULL,philosopher,(void*)&args[i]);
	}

	// Go into a loop until the user enters 'n'
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