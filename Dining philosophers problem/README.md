# Dining philosopher's problem solutions:

The dining_philosophers program is a simulation of the Dining Philosophers problem. It consists of 5 threads (philosophers) and 5 shared variables (chopsticks). Each version is built on top of the previous version.

The first version is a naive solution which can potentially have deadlocks and starvation. In this, each thread attempts to acquire the left chopstick first and then the right chopstick. This can lead to deadlock if each thread acquires their left chopstick and are waiting for their right chopstick.

The second version solves the deadlock issue by setting the task of checking for and acquiring the chopsticks in a critical section region guarded by a global mutex. Each thread acquires the lock, one at a time, and checks if the left and right chopsticks are available and takes them if they are. Else if it acquired one but couldn’t acquire the other, it’ll release the acquired chopstick and unlocks the mutex, to try again. This solution prevents deadlock but can lead to starvation as the CPU scheduler might pick the same thread again and again to execute, leading other threads to slowly starve.

The third and fourth versions resolve this issue by implementing the Bakery algorithm. The Bakery algorithm works by giving each thread a token (numbering) and allowing all threads with higher priority (i.e. smaller token) to execute first. This ensures fairness as each thread after executing must get a new token, which will be 1 + max of the token of other waiting threads. The third version is basically the same algorithm as the fourth version with N=5.

**Command-line execution:**
```
cd version-1
g++ -std=c++0x -pthread dining_philosophers.cpp
./a.out
```
```
cd version-4
g++ -std=c++0x -pthread dining_philosophers.cpp
./a.out 100
```
