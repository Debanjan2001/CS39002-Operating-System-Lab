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
#define N 100
#define MAXID 100000
#define RANGE 9
#define MAXJOBS 4

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
 * --------------------------------------------------------------------------
 *          Status           |       Block         |          Next          
 *                           |                     |         Status 
 * --------------------------|      Assigned       |--------------------------
 *      A           B        |                     |      A          B
 * ---------------------------------------------------------------------------
 *      0           0               A[0],B[0]             0          1
 *      0           1               A[0],B[1]             1          2
 *      1           2               A[1],B[2]             1          3
 *      1           3               A[1],B[3]             2          0
 *      2           0               A[2],B[0]             2          1
 *      2           1               A[2],B[1]             3          2
 *      3           2               A[3],B[2]             3          3
 *      3           3               A[3],B[3]             4          4 (4 == remove from queue)
 * ---------------------------------------------------------------------------
 *      
 * A = [ A[0]  A[1] ]
 *     [ A[2]  A[3] ]
 * 
 */



typedef struct compjob {
    int producerId;
    int status;
    int matrix[N][N];
    int matrixId;
    int resultIndex;
} Job;

typedef struct sham {
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

void PrintJob(Job* j) {
    cout<<"MatrixId : "<<j->matrixId<<"\n";
    cout<<"Status   : "<<j->status<<"\n";
    cout<<"Producer : "<<j->producerId<<"\n\n";
    // if required print the matrix, avoiding right now.
}

Job* CreateJob(int producenum) {
    Job* j = new Job();
    j->producerId = producenum;
    j->status = 0;
    j->resultIndex = -1;
    j->matrixId = (rand()%MAXID) + 1;
    for(int i = 0; i < N; i ++) {
        for(int k = 0; k < N; k ++) {
            j->matrix[i][k] = (rand()%(2*RANGE+1)) - RANGE;
        }
    }
    return j;
}

// insert a normal segment from producers
void InsertJob(SharedMem* shm, Job* newjob) {
    if(shm->count == Q_SIZE) {
        cerr<<"ERROR:: shared memory queue is full. overflow detected.\n";
        return;
    }
    shm->count++;

    shm->jobQueue[shm->inPointer].producerId = newjob->producerId;
    shm->jobQueue[shm->inPointer].matrixId = newjob->matrixId;
    shm->jobQueue[shm->inPointer].status = newjob->status;
    shm->jobQueue[shm->inPointer].resultIndex = newjob->resultIndex;
    for(int i = 0; i < N; i++) {
        for(int j = 0; j < N; j++) {
            shm->jobQueue[shm->inPointer].matrix[i][j] = newjob->matrix[i][j];
        }
    }

    shm->inPointer++;
    shm->inPointer %= Q_SIZE;
}

// insert a result segment at the end 
int InsertResultJob(SharedMem* shm) {
    if(shm->count == Q_SIZE) {
        cerr<<"ERROR:: queue is full. result segment cannot be inserted. !!FATAL!!\n";
        exit(EXIT_FAILURE);
    }
    shm->count++;

    shm->jobQueue[shm->inPointer].producerId = -1;
    shm->jobQueue[shm->inPointer].matrixId = (rand()%MAXID) + 1;
    shm->jobQueue[shm->inPointer].status = 0;
    shm->jobQueue[shm->inPointer].resultIndex = -1;
    for(int i = 0; i < N; i++) {
        for(int j = 0; j < N; j++) {
            shm->jobQueue[shm->inPointer].matrix[i][j] = 0;
        }
    }
    int retval = shm->inPointer;
    shm->inPointer++;
    shm->inPointer %= Q_SIZE;
    return retval;
}

void UpdateStatus(int* statusA, int* statusB) {
    int oldA = *statusA, oldB = *statusB;
    *statusB = (oldA < 2) ? (oldB+1)%4 : oldB;
    *statusA = (oldB % 2) ? (oldA+1)   : oldA;
    return;
}

void ComputeBlockProduct(int D[][N/2], SharedMem* shm) {
    int stat1 = shm->jobQueue[shm->outPointer].status;
    int stat2 = shm->jobQueue[shm->outPointer + 1].status;
    int ROffset1 = (stat1>1), COffset1 = (stat1%2);
    int ROffset2 = (stat2>1), COffset2 = (stat2%2);
    for(int i = 0; i < N/2; i++ ) {
        for(int j = 0; j < N/2; j++ ) {
            int ans = 0;
            for(int k = 0; k < N/2; k++ ) {
                // compute and assign vector mult
                ans += (( shm->jobQueue[shm->outPointer].matrix[i+ROffset1][j+COffset1] )*( shm->jobQueue[(shm->outPointer+1)%Q_SIZE].matrix[i+ROffset2][j+COffset2] ));
            }
            D[i][j] = ans;
        }
    }
    return;
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

    clock_t start = clock();

    for(int i = 0; i < NP; i++) {
        pid_t pid = fork();
        cout<<">>> FORKING ("<<i+1<<") Producer...\n";
        if(pid < 0) {
            cerr<<"ERROR:: [ fork() ] failed to spawn a child process.\n";
            exit(EXIT_FAILURE);
        }
        else if(pid == 0) {
            // handle Producer

            int chshmid = shmget(key, sizeof(SharedMem), IPC_CREAT | 0666);
            if(chshmid < 0) {
                cerr<<"ERROR:: [ shmget() ] failed to get a shared memory space.\n";
                exit(EXIT_FAILURE);
            }   
            // cout<<">>> Shared Memory created.\n";

            SharedMem* shm = (SharedMem *)shmat(chshmid, NULL, 0);
            
            for(;;) {
                // wait for a random time from 0-3s
                sleep(rand()%4);

                // create a new computing job
                Job* newjob = CreateJob(i);

                // if the queue is not empty wait
                sem_wait(&(shm->empty));

                // wait to gain access to the shared memory control variables
                sem_wait(&(shm->mutex));

                // if already required jobs are created, stop generating
                if(shm->jobsCreated == MAXJOBS) {
                    sem_post(&(shm->mutex));
                    break;
                }
                else if(shm->count < Q_SIZE-1){
                    // else insert the generated job into the queue if the count < 0
                    InsertJob(shm, newjob);

                    // print the newjob
                    cout<<"\n>>> Job Inserted : producer[PID:"<<getpid()<<"] : \n";
                    PrintJob(newjob);

                    // increment the job counter
                    shm->jobsCreated++;

                    // increment the full counter
                    sem_post(&(shm->full)); 
                }

                // surrender access to shared memory control variables
                sem_post(&(shm->mutex));
            }

            shmdt(shm);
            exit(EXIT_SUCCESS);
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

            int chshmid = shmget(key, sizeof(SharedMem), IPC_CREAT | 0666);
            if(chshmid < 0) {
                cerr<<"ERROR:: [ shmget() ] failed to get a shared memory space.\n";
                exit(EXIT_FAILURE);
            }   
            // cout<<">>> Shared Memory created.\n";

            SharedMem* shm = (SharedMem *)shmat(chshmid, NULL, 0);
            

            while(1) {
                // wait for random time between 0-3s
                sleep(rand()%4);

                // check if there are atleast 2 matrices
                sem_wait(&(shm->mutex));
                if(shm->count < 2) {
                    sem_post(&(shm->mutex));
                    continue;
                }
                else {
                    // check if any block is left to compute
                    if(shm->jobQueue[shm->outPointer].status == 4 &&
                       shm->jobQueue[(shm->outPointer+1)%Q_SIZE].status == 4) {
                        sem_post(&(shm->mutex));
                        continue;
                    }
                    
                    // check if this is the first block
                    int status1 = shm->jobQueue[shm->outPointer].status, status2 = shm->jobQueue[(shm->outPointer+1)%Q_SIZE].status;
                    if(shm->jobQueue[shm->outPointer].status == 0 &&
                       shm->jobQueue[(shm->outPointer+1)%Q_SIZE].status == 0) {
                        // if(shm->count <= Q_SIZE-1) {
                            cout<<">>> First Access for Jobs : ("<<shm->jobQueue[shm->outPointer].matrixId<<", "<<shm->jobQueue[(shm->outPointer+1)%Q_SIZE].matrixId<<") \n";
                            int resPointer = InsertResultJob(shm);
                            shm->jobQueue[shm->outPointer].resultIndex = resPointer;
                            shm->jobQueue[(shm->outPointer+1)%Q_SIZE].resultIndex = resPointer;
                            cout<<">>> Result Segment created for Jobs : ("<<shm->jobQueue[shm->outPointer].matrixId<<", "<<shm->jobQueue[(shm->outPointer+1)%Q_SIZE].matrixId<<") : "<<shm->jobQueue[resPointer].matrixId<<" \n";
                        // }
                    }
                    
                    // store the pointer to the jobQ 
                    int resPointer = shm->jobQueue[shm->outPointer].resultIndex;
                    int D[N/2][N/2];
                    cout<<">>> Reading and Computing for : ("<<shm->jobQueue[shm->outPointer].matrixId<<", "<<shm->jobQueue[(shm->outPointer+1)%Q_SIZE].matrixId<<") Blocks : ("<<status1<<", "<<status2<<") \n";
                    ComputeBlockProduct(D, shm);
                    UpdateStatus(&(shm->jobQueue[shm->outPointer].status), &(shm->jobQueue[(shm->outPointer+1)%Q_SIZE].status));
                    
                    // if this is the last worker to access then remove the current job matrixes
                    if(shm->jobQueue[shm->outPointer].status == 4 &&
                       shm->jobQueue[(shm->outPointer+1)%Q_SIZE].status == 4) {
                           cout<<">>> Removing Jobs : ("<<shm->jobQueue[shm->outPointer].matrixId<<", "<<shm->jobQueue[(shm->outPointer+1)%Q_SIZE].matrixId<<") \n";
                        shm->outPointer += 2;
                        shm->outPointer %= Q_SIZE;
                        shm->count --;

                        sem_post(&(shm->empty));
                        sem_post(&(shm->empty));
                    }
                    
                    // surrender the access to shared memory control variables
                    sem_post(&(shm->mutex));

                    // wait for access to the block of the result array that this worker should add to
                    int cblockindex = (status1>>1)*2 + (status2&1);
                    sem_wait(&(shm->cmatmutexes[cblockindex]));
                    cout<<">>> Updating C Block for : ("<<shm->jobQueue[resPointer].matrixId<<") \n";

                    // copy the result of this worker into the corresponding block
                    int roffset = (cblockindex>1), coffset = (cblockindex%2);
                    for(int i = 0; i < N/2; i++) {
                        for(int j = 0; j < N/2; j++) {
                            shm->jobQueue[resPointer].matrix[i+roffset][j+coffset] += D[i][j];
                        }
                    }

                    // unlock access to the result array block
                    sem_post(&(shm->cmatmutexes[cblockindex]));                    
                }
            }
            shmdt(shm);
            exit(EXIT_SUCCESS);
        }
        else {
            workers.push_back(pid);
        }
    }
    cout<<">>> All workers spawned.\n";

    while(1) {
        sem_wait(&(shmem->mutex));
        if((shmem->jobsCreated == MAXJOBS) && (shmem->count == 1)) {
            for(int i = 0 ; i < NP; i++) {
                kill(producers[i], SIGTERM);
            }
            for(int i = 0; i < NW; i++) {
                kill(workers[i], SIGTERM);
            }
            break;
        }
        sem_post(&(shmem->mutex));
    }
    
    clock_t end = clock();

    double elapsed_time = (end-start)/CLOCKS_PER_SEC;
    int trace = 0;
    for(int i = 0; i < N; i++) {
        trace += shmem->jobQueue[shmem->outPointer].matrix[i][i];
    }
    sem_destroy(&(shmem->mutex));

    cout<<">>>Finished executing : "<<MAXJOBS<<" jobs. \n";
    cout<<">>>Total Time Elapsed : "<<elapsed_time<<" seconds.\n";
    cout<<">>>Final Trace        : "<<trace<<" .\n";

    shmdt(shmem);

    cout<<"\n>>> Detached Main process from the Shared Memory.\n";
    shmctl(shmid, IPC_RMID, NULL);

    cout<<"\n>>> Shared Memory freed.\n";
    // if(kill(-1, SIGKILL) < 0) {
    //     cerr<<"ERROR:: [ kill() ] failed to send kill signal.\n";
    //     exit(EXIT_FAILURE);
    // }
    return 0;
}   