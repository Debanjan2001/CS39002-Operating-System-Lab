#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
using namespace std;

typedef size_t addrs;

/**
 * @brief enum for var_type + function for var_size(var_type)
 * 
 */

struct hole {
    hole* prev;
    
    // baseptr of this hole
    size_t baseptr; // offset into the memory

    // size of this hole
    size_t size;

    hole* next;
};

struct free_list {
    // linked list of all holes, will remain sorted on hole.baseptr's by implementation
    hole* head;
    hole* tail;
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
    unsigned int bitmap;

    // info : 8'b0 8'b0 8'b0 00000000
    //                              ^last bit of info denotes if this is an array segment or a variable segment
    //                            ^^the two bits before that specify the var_type of this array or variable segment
    unsigned int info;

    segment* next;
};


struct segment_table {
    segment* var_head;
    segment* var_tail;
    // Revision 1: maybe arr_head is redundant, since it is var_tail->next but still it is kept. 
    segment* arr_head;
    segment* arr_tail;
};

struct mmu_t {
    void* baseptr; // actual physical address
};

mmu_t* create_mem (mmu_t* mmu, size_t size) {

} 


