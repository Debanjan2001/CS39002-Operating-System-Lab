#include <bits/stdc++.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
 
using namespace std;

#define MAX_CHILD_JOBS 100
#define MAX_JOBQ_SIZE 1000
#define JOB_ID 100'000'000
#define JOB_DURATION 250

typedef struct job_ {
    /**
     * @brief 
     * STATUS Codes : -1 Empty Job.
     *                 0 Created.
     *                 1 Running.
     *                 2 Completed.
     * 
     */
    int job_id;
    int duration;
    int num_child_jobs;
    struct job_ * child_jobs[MAX_CHILD_JOBS];
    pthread_mutex_t mutex;
    int status; 
    job_() {
        job_id = -1;
        duration = -1;
        num_child_jobs = 0;
        memset(child_jobs, NULL, sizeof(child_jobs));
        status = -1;
    }
} job;

typedef struct shmem_ {
    int count_jobs;
    job job_queue[MAX_JOBQ_SIZE];
} shmem_t;

void init(shmem_t* shmem) {
    shmem->count_jobs = 0;
}

job* insert_new_job(shmem_t* shmem) {
    if(shmem->count_jobs >= MAX_JOBQ_SIZE) {
        cerr<<"ERROR :: memory full. cannot insert new jobs"<<endl;
        return NULL;
    }
    int i = shmem->count_jobs;
    shmem->job_queue[i].job_id = (rand()*rand())%JOB_ID + 1;
    shmem->job_queue[i].duration = (rand())%JOB_DURATION + 1;
    pthread_mutex_init(&shmem->job_queue[i].mutex, NULL);
    shmem->job_queue[i].status = 0;

    shmem->count_jobs += 1;

    return &(shmem->job_queue[i]);
}

void * producer_thread_handler(void* param) {

}

void * consumer_thread_handler(void* param) {

}

int main() {
    cout<<"Start"<<endl;

    int key = 234;
    int shmid = shmget(key, sizeof(shmem_t), IPC_CREAT | 0666);
    if(shmid < 0) {
        cerr<<"ERROR :: shmget()"<<endl;
        exit(EXIT_FAILURE);
    }

    shmem_t* shmem = (shmem_t*)shmat(shmid, NULL, 0);
    init(shmem);

    // Spawn Processor Threads
    // Create an Init Array of Root Jobs
    // Fork B
    // In B, Spawn Consumer Threads
}