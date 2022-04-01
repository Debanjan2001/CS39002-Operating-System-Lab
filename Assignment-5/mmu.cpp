#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fstream>
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
/**
 * 
 */

segment* get_segment_head(mmu_t* mmu) {
    if(mmu->vmm->segment_list->var_head == NULL) {
        return mmu->vmm->segment_list->arr_head;
    }
    return mmu->vmm->segment_list->var_head;
}

void insert_after_segment(mmu_t* mmu, segment* n) {
    if(n->info & 1) {
        // variable segment insert at var_tail
        if(mmu->vmm->segment_list->var_head == NULL) {
            mmu->vmm->segment_list->var_head = n;
            mmu->vmm->segment_list->var_tail = n;
            mmu->vmm->segment_list->var_tail->next = NULL;
            mmu->vmm->segment_list->var_tail->prev = NULL;
            if(mmu->vmm->segment_list->arr_head != NULL) {
                mmu->vmm->segment_list->arr_head->prev = mmu->vmm->segment_list->var_tail;
                mmu->vmm->segment_list->var_tail =  mmu->vmm->segment_list->arr_head;
            }
        } else {
            n->next = mmu->vmm->segment_list->var_tail->next;
            n->prev = mmu->vmm->segment_list->var_tail;
            mmu->vmm->segment_list->var_tail = n;

            if(mmu->vmm->segment_list->var_tail->next != NULL) {
                mmu->vmm->segment_list->var_tail->next->prev = mmu->vmm->segment_list->var_tail;
            }
        }
    } else {
        // array segment insert at arr_tail
        if(mmu->vmm->segment_list->arr_head == NULL) {
            mmu->vmm->segment_list->arr_head = n;
            mmu->vmm->segment_list->arr_tail = n;
            mmu->vmm->segment_list->arr_tail->next = NULL;
            mmu->vmm->segment_list->arr_tail->prev = NULL;

            if( mmu->vmm->segment_list->var_tail != NULL) {
                mmu->vmm->segment_list->arr_head->prev =  mmu->vmm->segment_list->var_tail;
                mmu->vmm->segment_list->var_tail->next =  mmu->vmm->segment_list->arr_head;
            }
        } else {
            n->next = NULL;
            mmu->vmm->segment_list->arr_tail->next = n;
            n->prev = mmu->vmm->segment_list->arr_tail;
            mmu->vmm->segment_list->arr_tail = n;
        }
    }
}

void delete_hole (mmu_t* mmu, hole* h) {
    if(h->next != NULL) {
        h->next->prev = h->prev;
    }
    if(h->prev != NULL) {
        h->prev->next = h->next;
    }

    if(h == mmu->vmm->free_list->head) {
        mmu->vmm->free_list->head = h->next;
    }
    if(h == mmu->vmm->free_list->tail) {
        mmu->vmm->free_list->tail = h->prev;
    }
    free(h);
    if(ANALYSE) {
        _bookkeeping -= sizeof(hole);
        logfile<<_bookkeeping<<" "<<_alloted<<endl;
    }
    
    mmu->vmm->free_list->size -= 1;
}

void free_segment (mmu_t* mmu, segment* seg) {
    if(seg->next!=NULL) seg->next->prev = seg->prev;
    if(seg->prev!=NULL) seg->prev->next = seg->next;
    if(mmu->vmm->segment_list->var_head == seg) {
        mmu->vmm->segment_list->var_head = seg->next;
    }
    if(mmu->vmm->segment_list->var_tail == seg) {
        mmu->vmm->segment_list->var_tail = seg->prev;
    }
    if(mmu->vmm->segment_list->arr_head == seg) {
        mmu->vmm->segment_list->arr_head = seg->next;
    }
    if(mmu->vmm->segment_list->arr_tail == seg) {
        mmu->vmm->segment_list->arr_tail = seg->prev;
    }
    hole* h = (hole*) malloc(sizeof(hole));
    h->baseptr = seg->baseptr;
    h->size = seg->size;
    h->next = NULL;
    h->prev = NULL;
    free(seg);
    if(ANALYSE) {
        cout<<"bingo"<<endl;
        _alloted -= (h->size)*4;
        _bookkeeping = _bookkeeping - (sizeof(segment)) + sizeof(hole);
        logfile<<_bookkeeping<<" "<<_alloted<<endl;
    }
    hole* loc = mmu->vmm->free_list->head;
    while(loc != NULL) {
        if(loc->baseptr > h->baseptr) break;
        loc = loc->next;
    }
    if(loc == NULL) {
        if(mmu->vmm->free_list->tail != NULL && mmu->vmm->free_list->tail->baseptr + mmu->vmm->free_list->tail->size == h->baseptr) {
            mmu->vmm->free_list->tail->size += h->size;
            free(h);
            if(ANALYSE) {
                _bookkeeping -= sizeof(hole);
                logfile<<_bookkeeping<<" "<<_alloted<<endl;
            }
        } else {
            mmu->vmm->free_list->size += 1;
            if(mmu->vmm->free_list->tail == NULL) {
                mmu->vmm->free_list->head = h;
                mmu->vmm->free_list->tail = h;
            } else {
                h->prev = mmu->vmm->free_list->tail;
                mmu->vmm->free_list->tail->next = h;
                mmu->vmm->free_list->tail = h;
            }
        }
    } else {
        if(loc->prev == NULL) {
            if(loc->baseptr == h->baseptr + h->size) {
                loc->size += h->size;
                loc->baseptr = h->baseptr;
                free(h);
                if(ANALYSE) {
                    _bookkeeping -= sizeof(hole);
                    logfile<<_bookkeeping<<" "<<_alloted<<endl;
                }
            } else {
                mmu->vmm->free_list->size += 1;
                loc->prev = h;
                h->next = loc;
                mmu->vmm->free_list->head = h;
            }
        } else {
            h->next = loc->prev->next;
            h->next->prev = h;
            loc->prev->next = h;
            h->prev = loc->prev;
            mmu->vmm->free_list->size += 1;
             
            if(loc->prev->baseptr + loc->prev->size == h->baseptr) {
                loc->prev->size += h->size;
                delete_hole(mmu, h);
                h = loc->prev;
            }

            if(h->baseptr + h->size == loc->baseptr) {
                h->size += loc->size;
                delete_hole(mmu, loc);
            }

        }
    }
}

void push_rbp() {
    pthread_mutex_lock(&book_lock);
    gc_stack_node* newnode = (gc_stack_node*) malloc(sizeof(gc_stack_node));
    if(ANALYSE) {
        _bookkeeping += sizeof(gc_stack_node);
        logfile<<_bookkeeping<<" "<<_alloted<<endl;
    }
    newnode->seg = (segment*)(gc_stack->rbp);
    newnode->next = gc_stack->top;

    gc_stack->top = newnode;
    gc_stack->rbp = newnode;
    pthread_mutex_unlock(&book_lock);
}

void gc_mark() {
    // mark all segments currently in the stack as alive.
    cout<<"GC:Mark Phase running..."<<endl;
    gc_stack_node* h = gc_stack->top;
    gc_stack_node* rbp = gc_stack->rbp;
    while(h != NULL) {
        if(h == rbp) {
            rbp = (gc_stack_node*)((rbp)->seg);
        } else {
            h->seg->info = h->seg->info | (1<<1);
            cout<<"GC_mArk::"<<h->seg->baseptr<<" "<<h->seg->size;
        }
        h = h->next;
    }
} 

void gc_compact(mmu_t* mmu) {
    // compute new baseptr locations for all segments
    // memcpy to these locations
    // update free list 
    cout<<"GC:Compacting..."<<endl;
    size_t baseptr = 0;
    segment* s = mmu->vmm->segment_list->var_head;
    while(s != NULL) {
        memcpy(mmu->baseptr + baseptr, mmu->baseptr+s->baseptr, s->size);
        s->baseptr = baseptr;
        baseptr = baseptr + s->size;
    }

    hole* h = mmu->vmm->free_list->head;
    while(h->next != NULL) {
        delete_hole(mmu, h);
    }
    if(h->next == NULL) {
        h->size += h->baseptr - baseptr;
        h->baseptr = baseptr;
    }
}


void gc_sweep(mmu_t* mmu) {
    cout<<"GC:Sweep Phase running..."<<endl;
    segment* s = get_segment_head(mmu);
    int cnt = 0;
    segment* temp = s;
    while(s != NULL) {
        if(s->info & (1<<1)) {
            // live segment, do not touch
            // reset the bit
            s->info = s->info ^ (1<<1);
            temp = s->next;
        } else {
            // delete segment; add to hole list; and all;
            cnt++;
            cout<<"GC:: "<<s->baseptr<<" "<<s->size<<endl;
            temp = s->next;
            free_segment(mmu, s);
        }
        s = temp;
    }
    cout<<"GC:"<<cnt<<" Marked Segments freed."<<endl;
    return;
}

void gc_run(mmu_t* mmu, bool compact = false) {
    gc_mark();
    gc_sweep(mmu);
    if(mmu->vmm->free_list->size > max_holes || compact) {
        gc_compact(mmu);
    }
}

void gc_pop_frame(mmu_t* mmu) {
    pthread_mutex_lock(&book_lock);
    cout<<"GC:Pop function stackframe, before returning..."<<endl;
    gc_stack_node* h = gc_stack->top;
    gc_stack_node* rbp = gc_stack->rbp;

    // delete all nodes part of the current frame, insert after the current base pointer
    while(h != rbp && h != NULL) {
        gc_stack->top = h->next;
        h->seg->info = h->seg->info ^ (1<<2); // denote it absence from the stack
        free(h);
        if(ANALYSE) {
            _bookkeeping -= sizeof(gc_stack_node);
            logfile<<_bookkeeping<<" "<<_alloted<<endl;
        }
        h = gc_stack->top;
    }


    if(h == rbp) {
        // repostion the base pointer to the prev frame base pointer and delete the rbp node.
        gc_stack->rbp = (gc_stack_node*)(gc_stack->rbp->seg);
        gc_stack->top = h->next;
        free(h);
        if(ANALYSE) {
            _bookkeeping -= sizeof(gc_stack_node);
            logfile<<_bookkeeping<<" "<<_alloted<<endl;
        }
        h = gc_stack->top;
    }
    else {
        cerr<<"GC:Stack may be corrupted.";
        exit(EXIT_FAILURE);
    }

    gc_run(mmu);
    pthread_mutex_unlock(&book_lock);
}

void* gc_thread_handler(void* data) {
    thread_data* tdata = (thread_data*) data;
    mmu_t* mmu = tdata->mmu;
    while(1) {
        sleep(1);
        pthread_mutex_lock(&book_lock);
        cout<<"Running GC..."<<endl;
        gc_run(mmu);
        pthread_mutex_unlock(&book_lock);
    }
    pthread_exit(NULL);
}

void gc_init(mmu_t* mmu) {
    // Insert a sentinel node at the bottom of the stack
    cout<<"GC:init all data structures. Spawning a gc thread..."<<endl;
    gc_stack = (gc_stack_t*) malloc(sizeof(gc_stack));
    gc_stack->top = (gc_stack_node*) malloc(sizeof(gc_stack_node));
    if(ANALYSE) {
        _bookkeeping += sizeof(gc_stack_node) + sizeof(gc_stack_t);
        logfile<<_bookkeeping<<" "<<_alloted<<endl;
    }
    gc_stack->top->seg = (segment*)gc_stack->top;
    gc_stack->top->next = NULL;
    gc_stack->rbp = gc_stack->top;

    // Spawn a thread and call gc_run(); ??
    pthread_mutexattr_t attrmutex;

    pthread_mutexattr_init(&attrmutex);
    pthread_mutex_init(&book_lock, &attrmutex);

    pthread_t gc_tid;
    thread_data data;
    data.mmu = mmu;
    pthread_create(&gc_tid, NULL, gc_thread_handler, (void *)&data);

    return;
}

void gc_push(segment* s) {
    if(s->info & (1<<2)) {
        gc_stack_node* newnode = (gc_stack_node*) malloc(sizeof(gc_stack_node));
        if(ANALYSE) {
            _bookkeeping += sizeof(gc_stack_node);
            logfile<<_bookkeeping<<" "<<_alloted<<endl;
        }
        newnode->seg = s;
        newnode->next = gc_stack->top;
        gc_stack->top = newnode;
        s->info = s->info | (1<<2);
    }
    return;
}

mmu_t* create_mem (mmu_t* mmu, size_t size) {
    cout<<"Creating memory block of size : "<<size<<endl;
    mmu = (mmu_t*) malloc(sizeof(mmu_t));
    mmu->baseptr = malloc(size);
    if(ANALYSE) {
        logfile.open("log.txt", ios::out);
        _bookkeeping += sizeof(mmu_t);
        //_alloted += size;
        logfile<<_bookkeeping<<" "<<_alloted<<endl;
    }

    // book-keeping space allocation
    // size_t book_size = size/2; // Revision 1: compute more carefully
    // size_t free_list_size = size/4; // Revision 1: compute more carefully
    cout<<"Intializing book-keeping structrues..."<<endl;
    mmu->vmm = (books*) malloc(sizeof(books));
    mmu->vmm->seg_counter = 0;

    mmu->vmm->free_list = (free_list_t*)malloc(sizeof(free_list_t));
    mmu->vmm->segment_list = (segment_list_t*)malloc(sizeof(segment_list_t));

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
    hole* hole_1 = (hole*) malloc(sizeof(hole));
    if(ANALYSE) {
        _bookkeeping += sizeof(free_list_t) + sizeof(segment_list_t) + sizeof(books) + sizeof(hole);
        logfile<<_bookkeeping<<" "<<_alloted<<endl;
    }
    hole_1->baseptr = 0;
    hole_1->size = size>>2;
    hole_1->next = hole_1->prev = NULL;
    mmu->vmm->free_list->size = 1;
    mmu->vmm->free_list->head = mmu->vmm->free_list->tail = hole_1;

    cout<<"Memory Manager initialised."<<endl;
    if(GC_ENABLE) {
        gc_init(mmu);
        push_rbp();
    }

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
    pthread_mutex_lock(&book_lock);
    cout<<"Creating variable..."<<endl;
    segment* s = mmu->vmm->segment_list->var_head;
    addrs var = 0;
    while(s != NULL && s != mmu->vmm->segment_list->arr_head) {
        if (s->info>>3 == type) { 
            if (s->bitmap != slab_full_indicator) {
                int i = 0;
                while((s->bitmap & 1<<i) == 1) {
                    i++;
                    if(i == 32) break;
                }
                if(i != 32) {
                    s->bitmap = (s->bitmap | (1<<i));
                    var = ((s->seg_num<<5 ) + i);

                    if(GC_ENABLE) gc_push(s);
                    cout<<"Found a segment..."<<endl;
                    cout<<"New variable created at : "<<var<<endl;
                    pthread_mutex_unlock(&book_lock);
                    return var;
                }
            }
        }
        s = s->next;
    }

    /** find a free hole enough for 32*4 bytes, if not: ( @a Review_Required : if not what to do? )
     */
    hole* h = mmu->vmm->free_list->head;
    while(h != NULL) {
        if(h->size >= 32){
            break;
        } else {
            h = h->next;
        }
    }

    if(h == NULL){
        cerr<<"Fatal Error :: assign_var() : Out of memory"<<endl;
        exit(EXIT_FAILURE);
    }
    cout<<"Found a hole. Creating new segment..."<<endl;
    segment* newseg = (segment*) malloc(sizeof(segment));
    newseg->info = static_cast<u_int32_t>(0);
    newseg->info |= 1;
    newseg->info |= type<<3;
    if(type != INT) {
        cout<<"Word_Align:Allocating redundant space"<<endl;
    }
    newseg->size = 32 ;
    if(ANALYSE) {
        _bookkeeping += sizeof(segment) ;
        _alloted += (newseg->size)<<2;
        logfile<<_bookkeeping<<" "<<_alloted<<endl;
    }
    newseg->seg_num = mmu->vmm->seg_counter;
    mmu->vmm->seg_counter += newseg->size;
    newseg->baseptr = h->baseptr;

    insert_after_segment(mmu, newseg);

    h->baseptr += 32;
    h->size -= 32;
    
    if(h->size == 0) {
        delete_hole(mmu, h);
    }

    newseg->bitmap |= 1;
    var = newseg->seg_num<<5;
    if(GC_ENABLE) gc_push(newseg);
    cout<<"New variable created at : "<<var<<endl;
    pthread_mutex_unlock(&book_lock);
    return var;
}

void assign_var (mmu_t* mmu, addrs var, var_type type, int value) {
    /**
     * @brief use var to find segment index and variable index, check for type mismatch, use memcpy and variable size
     * 
     */
    pthread_mutex_lock(&book_lock);
    cout<<"Assigning variable at : "<<var<<endl;
    segment *s = get_segment_head(mmu);
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
    cout<<"Found the address..."<<endl;
    u_int8_t cur_var_type = (s->info >> 3);
    if( cur_var_type != type ){
        cerr << "Fatal Error:: assign_arr() : Data type mismatch during array assignment" << endl;
        exit(EXIT_FAILURE);
    }
    cout<<"Assigning value now..."<<endl;
    void *var_mem_baseptr = (void * )(mmu->baseptr + (s->baseptr + var_actual_addr_offset) * word_size);
    memcpy( var_mem_baseptr , (void *)&value, var_size(type));
    cout<<"Assignment successful for variable at : "<<var<<endl;
    pthread_mutex_unlock(&book_lock);
}


addrs create_arr (mmu_t* mmu, var_type type, int arr_size) {
    /**
     * @brief similar to create_var, but we dont have to wait check first, we just create after finding a space.
     *
     */
    pthread_mutex_lock(&book_lock);
    cout<<"Creating array..."<<endl;
    size_t required_arr_size = (size_t)(arr_size);
    hole* h = mmu->vmm->free_list->head;

    cout<<"Searching for a hole..."<<endl;
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

    cout<<"Found a hole. Allocating..."<<endl;
    segment *arr_seg = (segment *)malloc(sizeof(segment));
    arr_seg->baseptr = h->baseptr;
    arr_seg->info = static_cast<u_int32_t>(0);
    arr_seg->info |= (type<<3);
    if(type != INT) {
        cout<<"Word_Align:Allocating redundant space"<<endl;
    }
    arr_seg->bitmap = 0;
    arr_seg->size = required_arr_size;
    if(ANALYSE) {
        _bookkeeping += sizeof(segment) ;
        _alloted += (arr_seg->size)<<2;
        logfile<<_bookkeeping<<" "<<_alloted<<endl;
    }
    arr_seg->seg_num = mmu->vmm->seg_counter;
    mmu->vmm->seg_counter += arr_seg->size;

    /**
     * @brief 
     * Return seg_num as the token for this array. It should be enough for searching
     */
    addrs var = arr_seg->seg_num;

    insert_after_segment(mmu, arr_seg);

    if(GC_ENABLE) gc_push(arr_seg);

    h->baseptr += required_arr_size;
    h->size -= required_arr_size;
    cout<<"Reducing hole size..."<<endl;

    if(h->size == 0){
        cout<<"Hole exhausted. Removing..."<<endl;
        delete_hole(mmu, h);
    }
    cout<<"Array created at : "<<var<<endl;
    pthread_mutex_unlock(&book_lock);
    return var;
}

int getval(mmu_t* mmu, addrs var) {
    pthread_mutex_lock(&book_lock);
    segment *s = get_segment_head(mmu);
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
    u_int type = s->info>>3;
    int value = 0;
    void *var_mem_baseptr = (void * )(mmu->baseptr + (s->baseptr + var_actual_addr_offset) * word_size);
    memcpy((void *)&value, var_mem_baseptr, var_size(type));
    pthread_mutex_unlock(&book_lock);
    return value;
}

int getval(mmu_t* mmu, addrs arr, unsigned int index) {
    pthread_mutex_lock(&book_lock);
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

    int arr_size = (int)(s->size);
    if(index >= arr_size) {
        cerr << "Fatal Error:: getval() : Out of Bounds Access"<<endl;
        exit(EXIT_FAILURE);
    }
    int value = 0;
    void *memptr = (void *)(mmu->baseptr + (s->baseptr + index)*word_size);
    memcpy((void *)&value, memptr, sizeof(int));
    pthread_mutex_unlock(&book_lock);
    return value;
}

void assign_arr (mmu_t* mmu, addrs arr, var_type type, int value) {
    /**
     * @brief similar to assign_arr
     * 
     */
    pthread_mutex_lock(&book_lock);
    cout<<"Assigning array..."<<endl;
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
    cout<<"Array located..."<<endl;
    u_int8_t arr_type = ((s->info) >> 3); 
    if(type != arr_type){
        cerr << "Fatal Error:: assign_arr() : Data type mismatch during array assignment" << endl;
        exit(EXIT_FAILURE);
    }

    cout<<"Assigning array at all fields."<<endl;
    int arr_size = (int)(s->size);
    for(int i=0;i<arr_size;i++){
        void *memptr = (void *)(mmu->baseptr + (s->baseptr + i)*word_size);
        memcpy(memptr, (void *)&value, var_size(arr_type));
    }

    cout<<"Value assigned to array at : "<<arr<<endl;
    pthread_mutex_unlock(&book_lock);
}

void assign_arr (mmu_t* mmu, addrs arr, var_type type, int value, int index) {
    /**
     * @brief similar to assign_arr but assigns a single element
     * 
     */
    pthread_mutex_lock(&book_lock);
    cout<<"Assigning array..."<<endl;
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
    cout<<"Array located..."<<endl;
    u_int8_t arr_type = ((s->info) >> 3); 
    if(type != arr_type){
        cerr << "Fatal Error:: assign_arr() : Data type mismatch during array assignment" << endl;
        exit(EXIT_FAILURE);
    }

    cout<<"Assigning array at all fields."<<endl;
    int arr_size = (int)(s->size);
    if(index >= arr_size) {
        cerr << "Fatal Error:: assign_arr() : Out of Bounds." << endl;
        exit(EXIT_FAILURE);
    }
    
    void *memptr = (void *)(mmu->baseptr + (s->baseptr + index)*word_size);
    memcpy(memptr, (void *)&value, var_size(arr_type));

    cout<<"Value assigned to array at : "<<arr<<endl;
    pthread_mutex_unlock(&book_lock);
}


void free_elem (mmu_t* mmu, addrs var) {
    /**
     * @brief if var is a variable then just mark it (set bitmap to 0) and move ahead, 
     *        if arr is an array, then just remove it add to holes
     * @a Review_Required : should we just delete var segment if the bitmap is all 0
     * 
     */
    pthread_mutex_lock(&book_lock);
    cout<<"Freeing segment allocated at : "<<var<<endl;
    segment* s = get_segment_head(mmu);
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
        s->bitmap = s->bitmap ^ (1<<offset);
        if(s->bitmap == 0) {
            // delete segment and add to hole if !GC_ENABLE otherwise just MARK ;
            if(GC_ENABLE) {
                // mark the segment
                s->info = s->info ^ (1<<1);
                cout<<"Marked for GC."<<endl;
            } else {
                // function will handle deleting the segment node and adding to the free_list
                free_segment(mmu, s);
                cout<<"Segment freed."<<endl;
            }
        } else {
            // done;
        }
    } else {
        // delete segment and add to hole if !GC_ENABLE otherwise just MARK ;
        if(GC_ENABLE) {
            // mark the segment
            s->info = s->info ^ (1<<1);
            cout<<"Marked for GC."<<endl;
        } else {
            // function will handle deleting the segment node and adding to the free_list
            free_segment(mmu, s);
            cout<<"Segment freed."<<endl;
        }
    }
    
    pthread_mutex_unlock(&book_lock);
    return;
}