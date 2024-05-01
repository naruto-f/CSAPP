/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define PUT(address, val) (*(unsigned int *)(address) = (val))

#define GET(address) (*(unsigned int *)(address))

#define DATA_PTR(address) ((char *)(address) + 8)

#define SUCC_PTR(address) ((char *)(address) + 4)

#define BLOCK_START_PTR(address) ((char *)(address) - 8)

#define EXTEND_PAGE_NUM 1

static unsigned int **free_lists;

/* 线性时间复杂度版本，后面可以优化为二分查找 */
static int caculate_largest_power_of_two_lessthan_n(int n) {
    if (n <= 0) {
        return -1;
    }

    int i = 0;
    int cur = 1;

    do {
        if (n <= cur) {
            return i;
        }

        ++i;
        cur <<= 1;
    } while (1);
}

static void *try_find_in_freelist(int free_list_index, size_t size, unsigned int **address_of_list_front) {
    unsigned int *pre = (unsigned int *)free_lists + free_list_index;
    unsigned int *cur = free_lists[free_list_index];
    uintptr_t succ_address = 0;

    while (GET(cur) < size) {
        pre = cur;
        if (GET(SUCC_PTR(cur)) == 0) {
            return NULL;
        } else {
            succ_address = (uintptr_t)*(unsigned int *)SUCC_PTR(cur);
            //succ_address = (uintptr_t)SUCC_PTR(cur);
            cur = (unsigned int *)succ_address;
        }
    }

    *address_of_list_front = pre;
    return cur;
}

static void delete_block_from_free_list(void *block_start, void *front_block_in_list) {
    uintptr_t address = (uintptr_t)GET(SUCC_PTR(block_start));
    if ((char*)front_block_in_list >= (char*)free_lists && (char*)front_block_in_list < (char*)free_lists + 4 * 32) {
        PUT(front_block_in_list, address);
    } else {
        PUT(SUCC_PTR(front_block_in_list), address);
    }

    PUT(SUCC_PTR(block_start), 0);
}

static void insert_block_to_free_list(void *block_start, int free_list_index) {
    PUT(SUCC_PTR(block_start), 0);
    uintptr_t addr = (uintptr_t)block_start;
    if (free_lists[free_list_index] != NULL) {
        uintptr_t temp_addr = free_lists[free_list_index];
        PUT(SUCC_PTR(block_start), temp_addr);
    }
    //PUT(&free_lists[free_list_index], addr);
    free_lists[free_list_index] = (unsigned int *)addr;
}

static void insert_block_to_fitting_free_list(void *block_start, size_t block_size) {
    int free_list_index = caculate_largest_power_of_two_lessthan_n(block_size);
    insert_block_to_free_list(block_start, free_list_index);
    PUT(block_start, block_size);
}

static void *extend_mem_for_free_list(int free_list_index) {
    int block_size = ALIGN((int)pow(2, free_list_index));
    int block_num = ALIGN(EXTEND_PAGE_NUM * mem_pagesize()) / block_size - 1;
    int extend_size = ALIGN(EXTEND_PAGE_NUM * mem_pagesize());

    if (block_size > extend_size) {
        extend_size = block_size;
        block_num = 0;
    }

    void *new_extend_base = mem_sbrk(extend_size);
    if (new_extend_base == (void *)-1) {
        return NULL;
    }

    char *ptr = (char *)new_extend_base + block_size;
    for (int i = 0; i < block_num; ++i) {
        PUT(ptr, block_size);
        insert_block_to_free_list(ptr, free_list_index);
        ptr += block_size;
    }

    PUT(new_extend_base, block_size);
    return new_extend_base;
}


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((free_lists = mem_sbrk(32 * 4)) == (void *)-1) {
        return -1;
    }

    for (int i = 0; i < 32; ++i) {
        free_lists[i] = NULL;
    }

    return 0;
}

int debug_count = 0;

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }

    if (size == 16) {
        ++debug_count;
    }

    int newsize = ALIGN(size + 8);
    int free_list_index = caculate_largest_power_of_two_lessthan_n(newsize);
    void *ptr = NULL;
    unsigned int *front_address_ptr = NULL;

    if (free_lists[free_list_index] != NULL) {
        //如果当前空闲链表中有空闲块则尝试在链表中寻找第一个合适的空闲块
        ptr = try_find_in_freelist(free_list_index, newsize, &front_address_ptr);
        if (ptr != NULL) {
            size_t block_size = GET(ptr);
            if (block_size - newsize > 8) {
                insert_block_to_fitting_free_list((char *)ptr + newsize, block_size - newsize);
                block_size = newsize;
            }

            PUT(ptr, block_size);
            delete_block_from_free_list(ptr, front_address_ptr);

            if ((char*)ptr + block_size - 1 > (char*)mem_heap_hi()) {
                printf("heap overflow\n");
            }

            return DATA_PTR(ptr);
        }
    }

    //如果空闲链表中没有任何空闲块或没有合适的空闲块则为该空闲链表分配一批大小为当前空闲链表最大能存放的空闲块并取一块给用户
    void *new_extend_base = extend_mem_for_free_list(free_list_index);
    if (new_extend_base == NULL) {
        return NULL;
    }

//    if ((char*)ptr + block_size - 1 > (char*)mem_heap_hi()) {
//        printf("heap overflow\n");
//    }

    return DATA_PTR(new_extend_base);
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    if (!ptr) {
        return;
    }

    size_t block_size = GET(BLOCK_START_PTR(ptr));
    insert_block_to_fitting_free_list(BLOCK_START_PTR(ptr), block_size);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    if (!ptr) {
        return mm_malloc(size);
    }

    void *oldptr = ptr;
    void *newptr;
    size_t block_size = GET(BLOCK_START_PTR(ptr));
    size_t new_block_size = ALIGN(size + SIZE_T_SIZE);
    size_t copySize = 0;

    if (block_size >= new_block_size) {
//        if (block_size - new_block_size > 8) {
//            PUT(BLOCK_START_PTR(ptr), new_block_size);
//            insert_block_to_fitting_free_list((char *)oldptr + new_block_size, block_size - new_block_size);
//        }

        return oldptr;
    } else {
        newptr = mm_malloc(size);
        if (newptr == NULL) {
            return NULL;
        }

        copySize = block_size - 4;
        memcpy(newptr, oldptr, copySize);
        mm_free(oldptr);
        return newptr;
    }
}














