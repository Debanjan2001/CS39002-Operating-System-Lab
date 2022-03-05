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
    // int num_completed_child;
    pthread_mutex_t mutex;
    int status; 
    job_() {
        job_id = -1;
        duration = -1;
        num_child_jobs = 0;
        status = -1;
    }
} job;

typedef struct shmem_ {
    int count_jobs;
    job job_queue[MAX_JOBQ_SIZE];
    pthread_mutex_t in_mutex;
    pthread_mutex_t rw_mutex;
    pthread_mutex_t mutex;
    int counter;
} shmem_t;

struct thread_data {
    int num;
    shmem_t* shmem;
};

void init(shmem_t* shmem) {
    shmem->count_jobs = 0;
    shmem->counter = 0;
    pthread_mutex_init(&shmem->in_mutex, NULL);
    pthread_mutex_init(&shmem->rw_mutex, NULL);
    pthread_mutex_init(&shmem->mutex, NULL);

}

job* insert_new_job(shmem_t* shmem) {
    if(shmem->count_jobs >= MAX_JOBQ_SIZE) {
        cerr<<"ERROR :: memory full. cannot insert new jobs"<<endl;
        return NULL;
    }
    int i = shmem->count_jobs;
    shmem->job_queue[i].job_id = (rand())%JOB_ID + 1;

    /** */
    shmem->job_queue[i].duration = (rand())%20 + 1;
    /** */

    pthread_mutex_init(&shmem->job_queue[i].mutex, NULL);
    shmem->job_queue[i].status = 0;
    shmem->count_jobs += 1;

    return &(shmem->job_queue[i]);
}

void* producer_thread_handler(void* param) {
    int thisthread = ((struct thread_data *)param)->num;

    /** */
    int time_to_run = rand()%2 + 1;
    /** */


    time_t start_time, end_time;
    start_time = time(NULL);
    shmem_t * shmem = ((struct thread_data *)param)->shmem;
    while(1) {

        end_time = time(NULL);
        if(end_time-start_time >= time_to_run) break;
        int j = -1;

        // This section may be put before the for(;;). Try once evrything works.

        for(;;) {
            pthread_mutex_lock(&shmem->in_mutex);
            pthread_mutex_lock(&shmem->mutex);
            shmem->counter ++;
            if(shmem->counter == 1) pthread_mutex_lock(&shmem->rw_mutex);
            pthread_mutex_unlock(&shmem->mutex);
            pthread_mutex_unlock(&shmem->in_mutex);
            j = rand()%shmem->count_jobs;
            pthread_mutex_lock(&shmem->mutex);
            shmem->counter--;
            if(shmem->counter == 0) pthread_mutex_unlock(&shmem->rw_mutex);
            pthread_mutex_unlock(&shmem->mutex);

            int x = pthread_mutex_trylock(&shmem->job_queue[j].mutex);
            if(x!=0) continue;
            if(shmem->job_queue[j].status == 0) {
                break;
            }
            pthread_mutex_unlock(&shmem->job_queue[j].mutex);
        }

        pthread_mutex_lock(&shmem->in_mutex);
        pthread_mutex_lock(&shmem->rw_mutex);
        job* newjob = insert_new_job(shmem);
        pthread_mutex_unlock(&shmem->rw_mutex);
        pthread_mutex_unlock(&shmem->in_mutex);

        cout<<"Job Inserted by [prod_thread : "<<thisthread<<", job_id : "<<newjob->job_id<<" ] "<<endl;
        if(newjob != NULL) {    
            shmem->job_queue[j].child_jobs[shmem->job_queue[j].num_child_jobs] = newjob;
            shmem->job_queue[j].num_child_jobs += 1;
        }

        pthread_mutex_unlock(&shmem->job_queue[j].mutex);

        /** */
        int sleeptime = rand()%301 + 200;
        /** */

        struct timespec sleep_time = {0, 1'000'000L * sleeptime};
        struct timespec rem_time;
        nanosleep(&sleep_time, &rem_time);
    }  
    cout<<"[prod_thread : "<<thisthread<<"] ran for "<<end_time-start_time<<" seconds."<<endl;
    pthread_exit(NULL);  
}

void* consumer_thread_handler(void* param) {
    int thisthread = ((struct thread_data *)param)->num;
    shmem_t * shmem = ((struct thread_data *)param)->shmem;
   
    while(1) {
        int count_jobs;
        pthread_mutex_lock(&shmem->in_mutex);
        pthread_mutex_lock(&shmem->mutex);
        shmem->counter ++;
        if(shmem->counter == 1) pthread_mutex_lock(&shmem->rw_mutex);
        pthread_mutex_unlock(&shmem->mutex);
        pthread_mutex_unlock(&shmem->in_mutex);
        count_jobs = shmem->count_jobs;
        pthread_mutex_lock(&shmem->mutex);
        shmem->counter--;
        if(shmem->counter == 0) pthread_mutex_unlock(&shmem->rw_mutex);
        pthread_mutex_unlock(&shmem->mutex);
    
        // int all_done = 1;
        for(int i = count_jobs-1; i >= 0; i--) {
            int x = pthread_mutex_trylock(&shmem->job_queue[i].mutex);
            if(x != 0) continue;
            if(shmem->job_queue[i].status == 0) {
                // all_done = 0;
                
                //check if all children are completed;
                int flag = 1;
                for(int k = 0; k < shmem->job_queue[i].num_child_jobs; k++ ){
                    
                    pthread_mutex_lock(&shmem->job_queue[i].child_jobs[k]->mutex);
                    int status = shmem->job_queue[i].child_jobs[k]->status;
                    pthread_mutex_unlock(&shmem->job_queue[i].child_jobs[k]->mutex);
                    
                    if(status != 2) {
                        flag = 0;
                        break;
                    } 
                }

                if(flag == 1) {
                    
                    cout<<"Job Started by [cons_thread : "<<thisthread<<", job_id : "<<shmem->job_queue[i].job_id<<" ] "<<endl;
                    shmem->job_queue[i].status = 1; // running
                    struct timespec proc_time = {0, 1'000'000L * shmem->job_queue[i].duration}, rem_time;
                    pthread_mutex_unlock(&shmem->job_queue[i].mutex);
                    
                    nanosleep(&proc_time, &rem_time);
                    
                    pthread_mutex_lock(&shmem->job_queue[i].mutex);
                    cout<<"Job Finished by [cons_thread : "<<thisthread<<", job_id : "<<shmem->job_queue[i].job_id<<" ] "<<endl;
                    shmem->job_queue[i].status = 2; // done
                    pthread_mutex_unlock(&shmem->job_queue[i].mutex);
                
                }
                else {
                    
                    pthread_mutex_unlock(&shmem->job_queue[i].mutex);
                
                }
            }
            else {
               
                pthread_mutex_unlock(&shmem->job_queue[i].mutex);
            
            }
        }

        // if(all_done) break;
    }
    
    cout<<"[cons_thread : "<<thisthread<<"] cannot find unfinished jobs."<<endl;
    pthread_exit(NULL);
}


void printNTree(job* x, vector<bool> flag, map<job*,int>& visited, int depth = 0, bool isLast = false);
void print_job_tree(shmem_t * shmem);

int main() {
    cout<<"Process A starting..."<<endl;

    /** */
    int num_prod_threads = 10, num_consumer_threads = 20;
    /** */

    pthread_t producers[num_prod_threads];
    
    int key = 2;
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


    /** */
    int init_num_jobs = rand()%2 + 2;
    /** */


    for(int i = 0; i < init_num_jobs; i++) {
        job* j = insert_new_job(shmem);
    }

    struct thread_data args[num_prod_threads];
    for(int i = 0; i < num_prod_threads; i++ ){ 
        args[i].shmem = shmem;
        args[i].num = i+1;
        pthread_create(&producers[i], NULL, producer_thread_handler, (void *) &args[i]);
    }

    pid_t proc_B;
    if((proc_B = fork()) == 0) {

        cout<<"Process B starting... "<<endl;
        // shmem_t* cshmem = (shmem_t*)shmat(shmid, NULL, 0);
        // Process-B
        pthread_t consumers[num_consumer_threads];
        struct thread_data cargs[num_consumer_threads];
        for(int i = 0; i < num_consumer_threads; i++ ) {
            cargs[i].shmem = shmem;
            cargs[i].num = i+1;
            pthread_create(&consumers[i], NULL, consumer_thread_handler, (void *) &cargs[i]);
        } 
        
        // wait for Consumer Threads
        for(int i = 0; i < num_consumer_threads; i++) {
            pthread_join(consumers[i], NULL);
        }

        shmdt(shmem);
        cout<<"Process B exiting..."<<endl;
        exit(EXIT_SUCCESS);
        
    } else {
        // wait for Producers Threads
        for(int i = 0; i < num_prod_threads; i++) {
            pthread_join(producers[i], NULL);
        }

        while(waitpid(proc_B, NULL, 0) < 0);
        // wait for Proc B
        print_job_tree(shmem);
        // continue Proc A
        for(int i = 0; i < shmem->count_jobs; i++) {
            if(shmem->job_queue[i].status != 2) {
                cout<<"DEBUG :: missed this one. [job_id : "<<shmem->job_queue[i].job_id<<" ]"<<endl;
            }
        }

        shmdt(shmem);
        shmctl(shmid, IPC_RMID, NULL);
        cout<<"Process A exiting..."<<endl;
        exit(EXIT_SUCCESS);
    }

}

void printNTree(job* x, vector<bool> flag, map<job*,int>& visited, int depth, bool isLast) {
    if (x == NULL)
        return;
    visited[x] = 1;
    for (int i = 1; i < depth; ++i) {
        if (flag[i] == true) {
            cout << "| "
                << " "
                << " "
                << " ";
        }
        else {
            cout << " "
                << " "
                << " "
                << " ";
        }
    }

    if (depth == 0)
        cout << x->job_id << '\n';
    else if (isLast) {
        cout << "+--- " << x->job_id << '\n';
        flag[depth] = false;
    }
    else {
        cout << "+--- " << x->job_id << '\n';
    }
    // cout<<x->num_child_jobs<<endl;
    // for (int i = 0; i < x->num_child_jobs; i++){ 
    //     if(x->child_jobs[i] == NULL) {
    //         cout<<i<<" is NULL"<<endl;
    //     }
    // }
    // return;
    for (int i = 0; i < x->num_child_jobs; i++){ 
        printNTree(x->child_jobs[i], flag, visited, depth + 1, i == ((x->num_child_jobs)-1));
    }
    flag[depth] = true;
}
 
void print_job_tree(shmem_t * shmem){
    int nv = shmem->count_jobs;
    cout<<"Total jobs : "<<nv <<endl;
    map<job*, int> visited;
    for(int i = 0; i < nv; i++) {
        if(visited[&shmem->job_queue[i]] == 0) {
            // cout<<"Trying for root job "<<shmem->job_queue[i].job_id<<endl;
            vector<bool> flag(nv, true);
            printNTree(&shmem->job_queue[i], flag, visited);
            // cout<<"Done"<<endl;
        }
    }
}