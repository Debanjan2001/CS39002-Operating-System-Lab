#include <bits/stdc++.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <sys/ipc.h>

#include <sys/shm.h>
using namespace std;

#define Q_SIZE 10
#define N 1000

/**
 * In the main program malloc the shared memory, assign it to the shared memory struct 
 * Initialize the shared memory structure appropriately
 * Fork NP Producer processes and code the producer behavior
 * Fork NW Worker processes and code the worker behavior
 * 
 * Two challenges:  the updation of status fields appropriately to denote if all blocks of the given job are to be done or not
 *                  second, the mutex locking mechanism or whatever the hell that is
 * 
 * The last position of the queue should be kept reserved for computations,
 * so that should be coded in the producer behavior to wait while there is less than or equal to 1 position on the queue.
 * 
 * 
 */

typedef struct compjob {
    int producerId;
    int status;
    int matrix[N][N];
    int matrixId;
} Job;

typedef struct shm {
    int jobsCreated;
    Job jobQueue[Q_SIZE];
    int inPointer;
    int outPointer;
    int count;  
    // Mutex for shared_mem variables
    sem_t mutex;
    // Mutex for updation of matrix blocks
    sem_t cmatmutexes[4];
    // Semaphore for Full
	sem_t full; 
    // Semaphore for Empty
	sem_t empty;  
} SharedMem;

SharedMem* initializeSHM(SharedMem* shmem) {
    shmem->jobsCreated = 0;
    shmem->inPointer = 0;
    shmem->outPointer = 0;
    shmem->count = 0;

    int sema = sem_init(&(shmem->mutex), 1, 1);
    
    for(int i = 0; i < 4; i++) {
        sema += sem_init(&(shmem->cmatmutexes[i]), 1, 1);
    }
    sema += sem_init(&(shmem->full), 1, 0);
    sema += sem_init(&(shmem->empty), 1, Q_SIZE);
    if(sema < 0) {
        cerr<<"ERROR:: [ shm_init() ] failed to initialize semaphores/mutexes.\n";
        exit(EXIT_FAILURE);
    }
    return shmem;
}



int main() {

    // Input the number of producers and workers...
    int NP, NW;
    cin>>NP>>NW;
    vector<pid_t> producers;
    vector<pid_t> workers;

    int key = 132;
    int shmid = shmget(key, sizeof(SharedMem), IPC_CREAT | 0666);
    if(shmid < 0) {
        cerr<<"ERROR:: [ shmget() ] failed to get a shared memory space.\n";
        exit(EXIT_FAILURE);
    }   
    cout<<">>> Shared Memory created.\n";

    SharedMem* shmem = (SharedMem *)shmat(shmid, NULL, 0);
    cout<<">>> Shared Memory attached to SharedMem struct.\n";

    shmem = initializeSHM(shmem);
    cout<<">>> Initialized the Shared Memory.\n";

    for(int i = 0; i < NP; i++) {
        pid_t pid = fork();
        cout<<">>> FORKING ("<<i+1<<") Producer...\n";
        if(pid < 0) {
            cerr<<"ERROR:: [ fork() ] failed to spawn a child process.\n";
            exit(EXIT_FAILURE);
        }
        else  if(pid == 0) {
            // handle Producer
            
            sleep(rand()%4);
        }
        else {
            producers.push_back(pid);
        }
    }
    cout<<">>> All producers spawned.\n";

    for(int i = 0; i < NW; i++) {
        pid_t pid = fork();
        cout<<">>> FORKING ("<<i+1<<") Worker...\n";
        if(pid < 0) {
            cerr<<"ERROR:: [ fork() ] failed to spawn a child process.\n";
            exit(EXIT_FAILURE);
        }
        else  if(pid == 0) {
            // handle Worker

        }
        else {
            workers.push_back(pid);
        }
    }
    cout<<">>> All workers spawned.\n";









    
}   