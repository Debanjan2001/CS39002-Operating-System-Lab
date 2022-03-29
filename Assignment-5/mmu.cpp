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
enum var_type {
    INT, 
    MED_INT,
    CHAR,
    BOOL
};

size_t var_size(var_type v) {
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
};

// struct for memory management unit
struct mmu_t {
    void* baseptr; // actual physical address
    books* vmm; // pointer to book-keeping space
};

mmu_t* create_mem (mmu_t* mmu, size_t size) {
    mmu = (mmu_t*) malloc(sizeof(mmu_t));
    mmu->baseptr = malloc(size);

    // book-keeping space allocation
    size_t book_size = size/2; // Revision 1: compute more carefully
    size_t free_list_size = size/4; // Revision 1: compute more carefully
    mmu->vmm = (books*) malloc(book_size);

    /**
     * @a Review_Required : How to do this ? Brain not working.
     * 
     */
    mmu->vmm->free_list = (free_list_t*) mmu->vmm;
    mmu->vmm->segment_list = (segment_list_t*) mmu->vmm + free_list_size;

    /**
     * @brief INIT : free_list with one big hole and other counter if required etc.
     * 
     */

    return mmu;
} 


addrs create_var (mmu_t* mmu, var_type t) {
    /**
     * @brief search the variable part of the segment list;
     *          if appropriate slot found; 
     *              done;
     *          else;
     *              create a new segment;
     *                  iterate through free list and check for enough sized holes;
     *                  if found; done;
     *                  else; ( @a Review_Required : make a space for a single variable, since giving segments is no longer possible. )
     *              add this variable;
     *              done;
     * 
     */
}


void assign_var (mmu_t* mmu, addrs var, var_type t, int value) {
    /**
     * @brief use var to find segment index and variable index, check for type mismatch, use memcpy and variable size
     * 
     */
}


addrs create_arr (mmu_t* mmu, var_type t) {
    /**
     * @brief similar to create_var, but we dont have to wait check first, we just create after finding a space.
     * 
     */
}


void assign_arr (mmu_t* mmu, addrs arr, var_type t, int value) {
    /**
     * @brief similar to assign_arr
     * 
     */
}


void free_elem (mmu_t* mmu, addrs var) {
    /**
     * @brief if var is a variable then just mark it (set bitmap to 0) and move ahead, 
     *        if arr is an array, then just remove it add to holes
     * @a Review_Required : should we just delete var segment if the bitmap is all 0
     * 
     */
}

void gc_compact();
void gc_run();
void gc_init();
void gc_push();