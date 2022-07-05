#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>

#include <sys/shm.h>

typedef struct subJob {
    double* A;
    double* B;
    double* C;
    int len1;
    int len2;
    int r;
    int c;
}subJob;

void scanMatrix(double* mat, int r, int c) {
    for(int i = 0; i < r; i++) {
        for(int j = 0; j < c; j++) {
            scanf("%lf", (mat + i*c + j));
        }
    }
}

void printMatrix(double* mat, int r, int c) {
    for(int i = 0; i < r; i++) {
        for(int j = 0; j < c; j++) {
            printf("%lf ", *(mat + i*c + j));
        }
        printf("\n");
    }
}

void multVec(subJob* subjob) {
    double ans = 0.0;
    int col1 = subjob->len1, col2 = subjob->len2;
    for(int i = 0; i < subjob->len1; i++) {
        ans += (*((subjob->A + (subjob->r)*col1 + i)))*(*((subjob->B + i*col2 + subjob->c))) ;
    }
    *((subjob->C + subjob->r*col2 + subjob->c)) = ans;
    // printf("\t>>> ans(%d,%d) = %lf\n", subjob->r, subjob->c, ans);
    return ;
} 

int main() {
    int r1,c1,r2,c2;
    scanf("%d%d%d%d",&r1,&c1,&r2,&c2);
    if(c1 != r2) {
        printf("ERROR::[  ] entered dimensions of the matrices are not compatible.\n");
        exit(EXIT_FAILURE);
    }  
    int requestedSize = sizeof(double) * (r1*c1 + r2*c2 + r1*c2);

    int key = 132;
    int shmid = shmget(key, requestedSize, IPC_CREAT | 0666);
    if(shmid < 0) {
        printf("ERROR:: [ shmget() ] failed to get a shared memory space.\n");
        exit(EXIT_FAILURE);
    }
    
    printf(">>> Shared Memory created.\n");

    double* A = shmat(shmid, NULL, 0);
    double* B = A + (r1 * c1);
    double* C = B + (r2 * c2);

    printf(">>> Matrix pointers init.\n");

    scanMatrix(A, r1, c1);
    scanMatrix(B, r2, c2);

    printf(">>> Matrixes input.\n");

    printf("\n>>> Matrix A : \n");
    printMatrix(A, r1, c1);
    
    printf("\n>>> Matrix B : \n");
    printMatrix(B, r2, c2);

    printf("\n");


    for(int i = 0; i < r1; i ++) {
        for(int j = 0; j < c2; j++) {
            printf(">> FORKING (%d, %d) \n", i, j);
            pid_t pid = fork();
            if(pid == 0) {
                // int child_shmid = shmid;
                int child_shmid = shmget(key, requestedSize, IPC_CREAT | 0666);
                if(child_shmid < 0) {
                    printf("ERROR:: [ shmget() ] failed to get a shared memory space.\n");
                    exit(EXIT_FAILURE);
                }
                double* childA = shmat(child_shmid, NULL, 0);
                double* childB = childA + (r1 * c1);
                double* childC = childB + (r2 * c2);

                subJob eleij = {childA, childB, childC, c1, c2, i, j};
                multVec(&eleij);

                shmdt(childA);

                // shmctl(child_shmid, IPC_RMID, NULL);
                exit(EXIT_SUCCESS);
            }
            else if(pid < 0) {
                printf("ERROR:: [ fork() ] failed to spawn a child process.\n");
                exit(EXIT_FAILURE);
            }
            else {
                continue;
            }
        }
    }

    printf(">>> All forked. Waiting...\n");

    while(wait(NULL) > 0);

    printf("\n>>> Result Matrix ready: \n");

    printMatrix(C, r1, c2);

    shmdt(A);

    printf("\n>>> Detach shared memory segment.\n");

    shmctl(shmid, IPC_RMID, NULL);

    printf(">>> Shared Memory freed. \n");

    return 0;
}