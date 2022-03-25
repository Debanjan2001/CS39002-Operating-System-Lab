#include <stdio.h>
#include <stdlib.h>
#define BOOL 1
#define CHAR 2
#define MEDINT 3
#define INT 4

typedef size_t addrs_t;
static size_t word_size = 4;

size_t var_size(int var_type) {
    switch(var_type) {
        case 1: return 1;
        case 2: return 1;
        case 3: return 3;
        case 4: return 4;
        default: return 4;
    }
}

typedef struct _hole_t {
    struct _hole_t* prev;
    size_t index;
    size_t size;
    struct _hole_t* next;
} hole_t;

typedef struct _alloc_t {
    struct _alloc_t* prev;
    size_t index;
    int data_type;
    size_t alloc_size;
    struct _alloc_t* next;
} alloc_t;

typedef struct _alloc_list_t {
    alloc_t* head;
    alloc_t* tail;
} alloc_table_t;

typedef struct _hole_list_t {
    hole_t* head;
    hole_t* tail;
} hole_table_t;

typedef struct _mmu_t {
    void* baseptr;
    size_t memsize;
    alloc_table_t alloc_list;
    hole_table_t hole_list;
}mmu_t;

mmu_t* create_mem(size_t size) {
    mmu_t* mmu = (mmu_t*) malloc(sizeof(mmu_t));

    /**
     * @brief Allocate a huge chunk of memory. Assign the start address to the baseptr.
     * 
     */
    mmu->baseptr = malloc(size);

    mmu->alloc_list.head = NULL;
    mmu->alloc_list.tail = NULL;
    mmu->hole_list.head = NULL;
    mmu->hole_list.tail = NULL;
    mmu->memsize = size;

    /**
     * @brief Initially entire memory is one big hole.
     * 
     */
    hole_t* newhole = (hole_t*) malloc(sizeof(hole_t));
    newhole->index = 0;
    newhole->size = size;
    newhole->prev = NULL;
    newhole->next = NULL;
    mmu->hole_list.head = newhole;
    mmu->hole_list.tail = newhole;

    return mmu;
}

int create_var(mmu_t* mmu, int var_type) {
    /**
     * @brief First Fit : Find the first hole of sufficient size; Allocate a 4-byte space and continue;
     * 
     */
    hole_t* h = mmu->hole_list.head;
    while(h != NULL) {
        if(h->size >= word_size) {
            break;
        }
        h = h->next;
    }

    if(h == NULL) {
        printf("ERROR:: create_var() : out of memory. \n");
        exit(EXIT_SUCCESS);
    }

    /**
     * @brief Reduce the size of the selected hole that slot is now allocated. 
     * If the hole is empty then just remove it from the table;
     * 
     */
    size_t thisvar = h->index;
    h->size -= word_size;
    h->index += word_size;
    if(h->size == 0) {
        if(h->prev == NULL) {
            mmu->hole_list.head = h->next;
        } else {
            h->prev->next = h->next;
        }

        if(h->next == NULL) {
            mmu->hole_list.tail = h->prev;
        } else {
            h->next->prev = h->prev;
        }
    }

    /**
     * @brief Enter the allocated space into the allocated slots table.
     * 
     */
    alloc_t* newalloc = (alloc_t*) malloc(sizeof(alloc_t));
    newalloc->index = thisvar;
    newalloc->alloc_size = word_size;
    newalloc->data_type = var_type;

    newalloc->next = mmu->alloc_list.head;
    if(mmu->alloc_list.head != NULL) {
        mmu->alloc_list.head->prev = newalloc;
    } else {
        mmu->alloc_list.tail = newalloc; 
    }
    mmu->alloc_list.head = newalloc;

    return thisvar;
}

void assign_var(mmu_t* mmu, size_t index, int value, int var_type) {
    /**
     * @brief Search the index in allocated slots table.
     * 
     */
    alloc_t* block = mmu->alloc_list.head;
    while(block != NULL) {
        if(block->index == index) {
            break;
        }
        block = block->next;
    }

    /**
     * @brief Check the block found, if any. 
     * @doubt How to enforce type checking ?
     */
    if(block == NULL) {
        printf("ERROR:: assign_var() : invalid memory access. \n");
        exit(EXIT_SUCCESS);
    } else {
        if(var_type > block->data_type) {
            printf("WARNING:: assign_var() : type mismatch. implicit cast may result in data loss.");
        }
        *((int *)(mmu->baseptr + block->index)) = value;
    } 
}

int main() {

}