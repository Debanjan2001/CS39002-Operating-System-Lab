#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
using namespace std;

typedef size_t addrs;
const bool GC_ENABLE = false;
const size_t word_size = 4; 
const u_int32_t slab_full_indicator = static_cast<u_int32_t>(0xFFFF);

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

struct gc_stack_node {
    segment* seg;
    gc_stack_node* next;
};

struct gc_stack_t {
    gc_stack_node* top;
    gc_stack_node* rbp;
};

gc_stack_t* gc_stack = NULL;

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

mmu_t* create_mem (mmu_t* mmu, size_t size) {
    mmu = (mmu_t*) malloc(sizeof(mmu_t));
    mmu->baseptr = malloc(size);

    // book-keeping space allocation
    // size_t book_size = size/2; // Revision 1: compute more carefully
    // size_t free_list_size = size/4; // Revision 1: compute more carefully
    mmu->vmm = (books*) malloc(sizeof(books));
    mmu->vmm->seg_counter = 0;

    /**
     * @a Review_Required : How to do this ? Brain not working.
     * 
     */
    // mmu->vmm->free_list = (free_list_t*) mmu->vmm;
    // mmu->vmm->segment_list = (segment_list_t*) mmu->vmm + free_list_size;

    /**
     * @brief INIT : free_list with one big hole and other counter if required etc.
     * 
     */

    return mmu;
} 


addrs create_var (mmu_t* mmu, var_type type) {
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
    segment* s = mmu->vmm->segment_list->var_head;
    addrs var = 0;
    while(s != mmu->vmm->segment_list->arr_head) {
        if (s->info>>3 == type) { 
            if (s->bitmap != slab_full_indicator) {
                int i = 0;
                while((s->bitmap & 1<<i) == 0) {
                    i++;
                    if(i == 32) break;
                }
                s->bitmap = (s->bitmap | (1<<i));
                var = ((s->seg_num<<5 )+ i);

                gc_push(s);
                return var;
            }
        }
        s = s->next;
    }

    /** find a free hole enough for 32*4 bytes, if not: ( @a Review_Required : if not what to do? )
     */
    hole* h = mmu->vmm->free_list->head;
    while(h != NULL) {
        if(h->size >= 32*word_size){
            break;
        } else {
            h = h->next;
        }
    }

    if(h == NULL){
        cerr<<"Fatal Error :: assign_var() : Out of memory"<<endl;
        exit(EXIT_FAILURE);
    }
    segment* newseg = (segment*) malloc(sizeof(segment));
    newseg->info = static_cast<u_int32_t>(0);
    newseg->info |= 1;
    newseg->info |= type<<3;
    newseg->size = 32 ;
    newseg->seg_num = mmu->vmm->seg_counter ++;
    newseg->baseptr = h->baseptr;

    insert_after_segment(newseg, mmu);

    h->baseptr += 32;
    h->size -= 32;
    if(h->size == 0) {
        delete_hole(mmu, h);
    }

    newseg->bitmap |= 1;
    var = newseg->seg_num<<5;
    return var;
}

void assign_var (mmu_t* mmu, addrs var, var_type type, int value) {
    /**
     * @brief use var to find segment index and variable index, check for type mismatch, use memcpy and variable size
     * 
     */

    segment *s = mmu->vmm->segment_list->var_head;
    addrs var_actual_addr_offset = 0; 

    while(s!=NULL){
        addrs lookup_range_min = (s->seg_num << 5);
        addrs lookup_range_max = lookup_range_min + 31;
        if(var >= lookup_range_min && var <= lookup_range_max){
            // Found the segment where it was declared;
            // Set the offset, var = i + seg_num * 32, where 'i' in [0,31]
            var_actual_addr_offset = var - lookup_range_min;
            break;
        }
        s = s->next;
    }


    if(s==NULL){
        cerr<<"Fatal Error:: assign_var() : Invalid Memory Assignment"<<endl;
        exit(EXIT_FAILURE);
    }

    u_int8_t cur_var_type = (s->info >> 3);
    if( cur_var_type != type ){
        cerr << "Fatal Error:: assign_arr() : Data type mismatch during array assignment" << endl;
        exit(EXIT_FAILURE);
    }

    void *var_mem_baseptr = (void * )(s->baseptr + var_actual_addr_offset * word_size);
    memcpy( var_mem_baseptr , (void *)&value, sizeof(int));
}


addrs create_arr (mmu_t* mmu, var_type type, int arr_size) {
    /**
     * @brief similar to create_var, but we dont have to wait check first, we just create after finding a space.
     *
     */

    size_t required_arr_size = (size_t)(arr_size) * word_size;
    hole* h = mmu->vmm->free_list->head;

    while(h!=NULL){
        if(h->size >= required_arr_size){
            break;
        }
        h=h->next;
    }

    if(h==NULL){
        /**
         * @brief 
         * No free space found?
         */

        cerr<<"Fatal Error :: assign_arr() : No free space available"<<endl;
        exit(EXIT_FAILURE);
    }


    segment *arr_seg = (segment *)malloc(sizeof(segment));
    arr_seg->baseptr = h->baseptr;
    arr_seg->info = static_cast<u_int32_t>(0);
    arr_seg->info |= (type<<3);
    arr_seg->bitmap = 0;
    arr_seg->size = required_arr_size;
    arr_seg->seg_num = mmu->vmm->seg_counter++;

    /**
     * @brief 
     * Return seg_num as the token for this array. It should be enough for searching
     */
    addrs var = arr_seg->seg_num;

    insert_after_segment(arr_seg, mmu);


    h->baseptr += required_arr_size;
    h->size -= required_arr_size;

    if(h->size == 0){
        delete_hole(mmu, h);
    }   

    return var;
}


void assign_arr (mmu_t* mmu, addrs arr, var_type type, int value) {
    /**
     * @brief similar to assign_arr
     * 
     */

    segment *s = mmu->vmm->segment_list->arr_head;
    
    while(s!=NULL){
        if(s->seg_num == arr){
            break;
        }
        s=s->next;
    }

    if(s==NULL){
        cerr << "Fatal Error:: assign_arr() : Invalid Array Assignment" << endl;
        exit(EXIT_FAILURE);
    }

    u_int8_t arr_type = ((s->info) >> 3); 
    if(type != arr_type){
        cerr << "Fatal Error:: assign_arr() : Data type mismatch during array assignment" << endl;
        exit(EXIT_FAILURE);
    }

    int arr_size = (int)(s->size / word_size);
    for(int i=0;i<arr_size;i++){
        void *memptr = (void *)(s->baseptr + i*word_size);
        memcpy(memptr, (void *)&value, sizeof(int));
    }
}


void free_elem (mmu_t* mmu, addrs var) {
    /**
     * @brief if var is a variable then just mark it (set bitmap to 0) and move ahead, 
     *        if arr is an array, then just remove it add to holes
     * @a Review_Required : should we just delete var segment if the bitmap is all 0
     * 
     */
    segment* s = mmu->vmm->segment_list->var_head;
    bool isVar = 0;
    while(s != NULL) {
        if((s->info & 1) && (s->seg_num<<5 <= var && var < (s->seg_num+1)<<5)) {
            isVar = 1;
            break;
        } else if(!(s->info & 1) && s->seg_num == var) {
            isVar = 0;
            break;
        }
        s = s->next;
    }

    if(s == NULL) {
        cerr << "ERROR :: illegal memory delete request. " << endl ;
        return;
    } else if (isVar) {
        int offset = var - s->seg_num<<5;
        s->bitmap = s->bitmap & ~(1<<offset);
        if(s->bitmap == 0) {
            // delete segment and add to hole if !GC_ENABLE otherwise just MARK ;
            if(GC_ENABLE) {
                // mark the segment
                s->info = s->info | (1<<1);
            } else {
                // function will handle deleting the segment node and adding to the free_list
                free_segment(mmu, s);
            }
        } else {
            // done;
        }
    } else {
        // delete segment and add to hole if !GC_ENABLE otherwise just MARK ;
        if(GC_ENABLE) {
            // mark the segment
            s->info = s->info | (1<<1);
        } else {
            // function will handle deleting the segment node and adding to the free_list
            free_segment(mmu, s);
        }
    }
    return;
}

void push_rbp() {
    gc_stack_node* newnode = (gc_stack_node*) malloc(sizeof(gc_stack_node));
    newnode->seg = (segment*)(gc_stack->rbp);
    newnode->next = gc_stack->top;

    gc_stack->top = newnode;
    gc_stack->rbp = gc_stack->rbp;
}

void gc_mark() {
    // mark all segments currently in the stack as alive.S
    gc_stack_node* h = gc_stack->top;
    gc_stack_node* rbp = gc_stack->rbp;
    while(h != NULL) {
        if(h == rbp) {
            rbp = (gc_stack_node*)((rbp)->seg);
        } else {
            h->seg->info = h->seg->info | (1<<1);
        }
        h = h->next;
    }
} 

void gc_pop_frame() {
    gc_stack_node* h = gc_stack->top;
    gc_stack_node* rbp = gc_stack->rbp;

    // delete all nodes part of the current frame, insert after the current base pointer
    while(h != rbp) {
        gc_stack->top = h->next;
        h->seg->info = h->seg->info ^ (1<<2); // denote it absence from the stack
        free(h);
        h = gc_stack->top;
    }

    if(h == rbp) {
        // repostion the base pointer to the prev frame base pointer and delete the rbp node.
        gc_stack->rbp = (gc_stack_node*)(gc_stack->rbp->seg);
        gc_stack->top = h->next;
        free(h);
        h = gc_stack->top;
    }
}

void gc_sweep() {
    segment* s = segment_list->var_head;
    while(s != NULL) {
        if(s->info & 1<<1) {
            // live segment, do not touch
            // reset the bit
            s->info = s->info ^ (1<<1);
        } else {
            // delete segment; add to hole list; and all;
            free_segment(s);
        }
    }
    return;
}

void gc_compact(mmu_t* mmu) {
    // compute new baseptr locations for all segments
    // memcpy to these locations
    // update free list 
    size_t baseptr = 0;
    segment* s = mmu->vmm->segment_list->var_head;
    while(s != NULL) {
        memcpy(mmu->baseptr + baseptr, mmu->baseptr+s->baseptr, s->size);
        s->baseptr = baseptr;
        baseptr = baseptr + s->size;
    }

    hole* h = mmu->vmm->free_list->head;
    while(h->next != NULL) {
        delete_hole(h);
    }
    if(h->next == NULL) {
        h->size += h->baseptr - baseptr;
        h->baseptr = baseptr;
    }
}

void gc_run() {
    gc_mark();
    gc_sweep();
    // wait for compaction criteria to satisfy.

}

void gc_init() {
    // Insert a sentinel node at the bottom of the stack
    gc_stack->top = (gc_stack_node*) malloc(sizeof(gc_stack_node));
    gc_stack->top->seg = 0;
    gc_stack->top->next = NULL;
    gc_stack->rbp = gc_stack->top;

    // Spawn a thread and call gc_run(); ??
    
}

void gc_push(segment* s) {
    if(s->info & (1<<2)) {
        gc_stack_node* newnode = (gc_stack_node*) malloc(sizeof(gc_stack_node));
        newnode->seg = s;
        newnode->next = gc_stack->top;
        gc_stack->top = newnode;
        s->info = s->info | (1<<2);
    }
    return;
}