#ifndef __MMU_H
#define __MMU_H

#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fstream>
#include <time.h>
using namespace std;

typedef size_t addrs;
bool GC_ENABLE = false;
const size_t word_size = 4; 
const u_int32_t slab_full_indicator = static_cast<u_int32_t>(0xFFFF);

const unsigned int max_holes = 1000;
bool ANALYSE = false;
unsigned long long _bookkeeping = 0ULL, _alloted = 0ULL, _requested = 0ULL;
ofstream logfile;
/**
 * @brief enum for var_type + function for var_size(var_type)
 * 
 */
enum var_type {
    INT, 
    MED_INT,
    CHAR,
    BOOL
};

size_t var_size(u_int v) {
    if(v == INT) return 4;
    else if(v == MED_INT) return 3;
    else if(v == CHAR) return 1;
    else if(v == BOOL) return 1;
    else return 0;
}

struct hole {
    hole* prev;
    
    // baseptr of this hole
    size_t baseptr; // offset into the memory

    // size of this hole
    size_t size;

    hole* next;
};

struct free_list_t {
    // linked list of all holes, will remain sorted on hole.baseptr's by implementation
    hole* head;
    hole* tail;
    size_t size;
};

struct segment {
    segment* prev;

    // token value; for variable segments, ith individual variables' token = i + seg_num*32
    size_t seg_num;

    // denotes the baseptr of this segment
    size_t baseptr;

    // size is fixed for variable segments, 32
    size_t size;

    // only used for variable segments; map which position are free and what are occupied
    u_int32_t bitmap;

    // info : 00000000
    //               ^last bit of info denotes if this is an array segment or a variable segment
    //              ^reserved for mark and sweep
    //             ^shows if the segment is present in gc_stack
    //           ^^the two bits before that specify the var_type of this array or variable segment   
    //                     
    u_int8_t info;

    segment* next;
};

struct segment_list_t {
    segment* var_head;
    segment* var_tail;
    // Revision 1: maybe arr_head is redundant, since it is var_tail->next but still it is kept. 
    segment* arr_head;
    segment* arr_tail;
};

// struct for book-keeping structures;
struct books {
    free_list_t* free_list;
    segment_list_t* segment_list;
    size_t seg_counter;
};

// struct for memory management unit
struct mmu_t {
    void* baseptr; // actual physical address
    books* vmm; // pointer to book-keeping space
};

/**
 * @brief GC Structures 
 * 
 */
struct gc_stack_node {
    segment* seg;
    gc_stack_node* next;
};

struct gc_stack_t {
    gc_stack_node* top;
    gc_stack_node* rbp;
};

struct thread_data {
    mmu_t* mmu;
};

gc_stack_t* gc_stack = NULL;
pthread_mutex_t book_lock;

segment* get_segment_head(mmu_t* mmu);
void insert_after_segment(mmu_t* mmu, segment* n) ;
void delete_hole (mmu_t* mmu, hole* h);

void free_segment (mmu_t* mmu, segment* seg);

void push_rbp();

void gc_mark();

void gc_compact(mmu_t* mmu);

void gc_sweep(mmu_t* mmu);

void gc_run(mmu_t* mmu, bool compact);

void gc_pop_frame(mmu_t* mmu);

void* gc_thread_handler(void* data);

void gc_init(mmu_t* mmu) ;

void gc_push(segment* s);

mmu_t* create_mem (mmu_t* mmu, size_t size);

addrs create_var (mmu_t* mmu, var_type type) ;

void assign_var (mmu_t* mmu, addrs var, var_type type, int value);

addrs create_arr (mmu_t* mmu, var_type type, int arr_size);

int getval(mmu_t* mmu, addrs var);

int getval(mmu_t* mmu, addrs arr, unsigned int index);

void assign_arr (mmu_t* mmu, addrs arr, var_type type, int value);

void assign_arr (mmu_t* mmu, addrs arr, var_type type, int value, int index) ;

void free_elem (mmu_t* mmu, addrs var);

#endif