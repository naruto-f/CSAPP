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

#define GET_ALLOC(address) (GET(address) & 0x1)

#define GET_SIZE(address) (GET(address) & ~0x7)

#define PACK(size, alloc) (size | alloc)

#define DATA_PTR(address) ((char *)(address) + 12)

#define SUCC_PTR(address) ((char *)(address) + 4)

#define PREV_PTR(address) ((char *)(address) + 8)

#define BLOCK_START_PTR(address) ((char *)(address) - 12)

#define NEXT_BLKP(ptr) ((char*)(ptr) + GET_SIZE(ptr))

#define PREV_BLKP(ptr) ((char*)(ptr) - GET_SIZE(((char*)ptr) - 4))

#define FT_PTR(ptr) ((char*)(ptr) + GET_SIZE(ptr) - 4)

#define CONTROL_DATA_LEN 16

#define EXTEND_PAGE_NUM 1

#define FIRST_MATCH 0

#define BEST_MATCH 1

#define MIN_ADDR_MATCH 2

static unsigned int **free_lists;

static int match_method = FIRST_MATCH;

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

static void *find_by_first_match(int free_list_index, size_t size, unsigned int **address_of_list_front) {
    unsigned int *pre = (unsigned int *)free_lists + free_list_index;
    unsigned int *cur = free_lists[free_list_index];
    uintptr_t succ_address = 0;

    while (GET_SIZE(cur) < size) {
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

static void *find_by_best_match(int free_list_index, size_t size, unsigned int **address_of_list_front) {
    unsigned int *pre = (unsigned int *)free_lists + free_list_index;
    unsigned int *cur = free_lists[free_list_index];
    unsigned int *ret = NULL;
    unsigned int min_mul = UINT32_MAX;
    uintptr_t succ_address = 0;

    while (1) {
        unsigned int cur_block_size = GET_SIZE(cur);
        if (cur_block_size >= size) {
            if (cur_block_size == size) {
                ret = cur;
                break;
            } else {
                if (cur_block_size - size < min_mul) {
                    min_mul = cur_block_size - size;
                    ret = cur;
                }
            }
        }


        if (GET(SUCC_PTR(cur)) == 0) {
            break;
        } else {
            succ_address = (uintptr_t)*(unsigned int *)SUCC_PTR(cur);
            pre = cur;
            //succ_address = (uintptr_t)SUCC_PTR(cur);
            cur = (unsigned int *)succ_address;
        }
    }

    *address_of_list_front = pre;
    return ret;
}

static void *find_by_min_addr_match(int free_list_index, size_t size, unsigned int **address_of_list_front) {
    unsigned int *pre = (unsigned int *)free_lists + free_list_index;
    unsigned int *cur = free_lists[free_list_index];
    unsigned int *ret = NULL;
    uintptr_t succ_address = 0;

    while (1) {
        unsigned int cur_block_size = GET_SIZE(cur);
        if (cur_block_size >= size) {
            if (cur < ret) {
                ret = cur;
            }
        }

        if (GET(SUCC_PTR(cur)) == 0) {
            break;
        } else {
            succ_address = (uintptr_t)*(unsigned int *)SUCC_PTR(cur);
            pre = cur;
            //succ_address = (uintptr_t)SUCC_PTR(cur);
            cur = (unsigned int *)succ_address;
        }
    }

    *address_of_list_front = pre;
    return ret;
}


static int in_free_lists(void *ptr) {
    if ((char*)ptr >= (char*)free_lists && (char*)ptr < (char*)free_lists + 4 * 32) {
        return 1;
    }
    return 0;
}



static void *try_find_in_freelist(int free_list_index, size_t size, unsigned int **address_of_list_front) {
    void *ret = NULL;

    switch (match_method) {
        case FIRST_MATCH:
            ret = find_by_first_match(free_list_index, size, address_of_list_front);
            break;
        case BEST_MATCH:
            ret = find_by_best_match(free_list_index, size, address_of_list_front);
            break;
        case MIN_ADDR_MATCH:
            ret = find_by_min_addr_match(free_list_index, size, address_of_list_front);
            break;
        default:
            break;
    }
    return ret;
}

static void delete_block_from_free_list(void *block_start, void *front_block_in_list) {
    uintptr_t address = (uintptr_t)GET(SUCC_PTR(block_start));
    if (in_free_lists(front_block_in_list)) {
        PUT(front_block_in_list, address);
    } else {
        PUT(SUCC_PTR(front_block_in_list), address);
    }

    if (address) {
        PUT(PREV_PTR((unsigned *)address), (uintptr_t)front_block_in_list);
    }

    PUT(SUCC_PTR(block_start), 0);
    PUT(PREV_PTR(block_start), 0);
}

static void insert_block_to_free_list(void *block_start, int free_list_index) {
    PUT(SUCC_PTR(block_start), 0);
    PUT(PREV_PTR(block_start), 0);
    uintptr_t addr = (uintptr_t)block_start;
    if (free_lists[free_list_index] != NULL) {
        uintptr_t temp_addr = free_lists[free_list_index];
        PUT(SUCC_PTR(block_start), temp_addr);
        PUT(PREV_PTR(temp_addr), (uintptr_t)block_start);
    }
    PUT(PREV_PTR(block_start), (uintptr_t)((unsigned int*)free_lists + free_list_index));
    //PUT(&free_lists[free_list_index], addr);
    free_lists[free_list_index] = (unsigned int *)addr;
}

static void insert_block_to_fitting_free_list(void *block_start, size_t block_size) {
    int free_list_index = caculate_largest_power_of_two_lessthan_n(block_size);
    insert_block_to_free_list(block_start, free_list_index);
    PUT(block_start, PACK(block_size, 0));
    PUT(FT_PTR(block_start), PACK(block_size, 0));
}

static void *extend_mem_for_free_list(int free_list_index, int newsize) {
    int block_size = ALIGN((int)pow(2, free_list_index));
    int block_num = ALIGN(EXTEND_PAGE_NUM * mem_pagesize()) / block_size - 1;
    int extend_size = ALIGN(EXTEND_PAGE_NUM * mem_pagesize());

    if (block_size > extend_size) {
        extend_size = block_size;
        block_num = 0;
    }

    // TODO(优化点3): 能不能将大块的空间占用设置为仅存在适当数量的内部碎片，但是目前有bug需要排查
//    if (newsize > extend_size) {
//        extend_size = newsize;
//        block_size = extend_size;
//        block_num = 0;
//    }

    void *new_extend_base = mem_sbrk(extend_size);
    if (new_extend_base == (void *)-1) {
        return NULL;
    }

    char *ptr = (char *)new_extend_base + block_size;
    for (int i = 0; i < block_num; ++i) {
        PUT(ptr, PACK(block_size, 0));
        PUT(FT_PTR(ptr), PACK(block_size, 0));
        insert_block_to_free_list(ptr, free_list_index);
        ptr += block_size;
    }

    PUT(new_extend_base, PACK(block_size, 1));
    PUT(FT_PTR(new_extend_base), PACK(block_size, 1));
    PUT(PREV_PTR(new_extend_base), 0);
    PUT(SUCC_PTR(new_extend_base), 0);
    return new_extend_base;
}


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((free_lists = mem_sbrk(33 * 4)) == (void *)-1) {
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

    if (1) {
        ++debug_count;
    }

    int newsize = ALIGN(size + CONTROL_DATA_LEN);
    int free_list_index = caculate_largest_power_of_two_lessthan_n(newsize);
    void *ptr = NULL;
    unsigned int *front_address_ptr = NULL;
    int old_free_list_index = free_list_index;

    while (free_list_index <= 31) {
        if (free_lists[free_list_index] != NULL) {
            //如果当前空闲链表中有空闲块则尝试在链表中寻找第一个合适的空闲块
            ptr = try_find_in_freelist(free_list_index, newsize, &front_address_ptr);
            if (ptr != NULL) {
                size_t block_size = GET_SIZE(ptr);
                int need_partion = 0;
                size_t partion_size = 0;
                if (block_size - newsize > 16) {
                    need_partion = 1;
                    partion_size = block_size - newsize;
                    block_size = newsize;
                }

                PUT(ptr, PACK(block_size, 1));
                PUT(FT_PTR(ptr), PACK(block_size, 1));
                delete_block_from_free_list(ptr, front_address_ptr);

                if (need_partion) {
                    insert_block_to_fitting_free_list((char *)ptr + newsize, partion_size);
                }

                if ((char*)ptr + block_size - 1 > (char*)mem_heap_hi()) {
                    printf("heap overflow\n");
                }

                return DATA_PTR(ptr);
            }
        }
        ++free_list_index;
    }


    //如果空闲链表中没有任何空闲块或没有合适的空闲块则为该空闲链表分配一批大小为当前空闲链表最大能存放的空闲块并取一块给用户
    void *new_extend_base = extend_mem_for_free_list(old_free_list_index, newsize);
    if (new_extend_base == NULL) {
        return NULL;
    }

//    if ((char*)ptr + block_size - 1 > (char*)mem_heap_hi()) {
//        printf("heap overflow\n");
//    }

    return DATA_PTR(new_extend_base);
}

static void delete_block_for_coalesce(void *ptr) {
    uintptr_t addr = 0;
    addr = GET(PREV_PTR(ptr));
    PUT(FT_PTR(ptr), 0);
    PUT(ptr, 0);
    delete_block_from_free_list(ptr, (void*)addr);
}

static void *coalesce(void *ptr) {
    size_t prev_alloc = 1;
    size_t next_alloc = 1;
    size_t size = GET_SIZE(ptr);
    void *last_header_ptr = ptr;
    uintptr_t addr = 0;

    if ((char*)ptr <= mem_heap_lo() + 33 * 4) {
        prev_alloc = 1;
    } else {
        prev_alloc = GET_ALLOC(PREV_BLKP(ptr));
    }

    if (NEXT_BLKP(ptr) < (char*)mem_heap_hi() + 1) {
        next_alloc = GET_ALLOC(NEXT_BLKP(ptr));
    } else {
        next_alloc = 1;
    }

    if (prev_alloc && next_alloc) {
        return NULL;
    } else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(NEXT_BLKP(ptr));
        delete_block_for_coalesce(NEXT_BLKP(ptr));
    } else if (!prev_alloc && next_alloc) {
        last_header_ptr = PREV_BLKP(ptr);
        size += GET_SIZE(PREV_BLKP(ptr));
        delete_block_for_coalesce(PREV_BLKP(ptr));
    } else {
        last_header_ptr = PREV_BLKP(ptr);
        size += GET_SIZE(PREV_BLKP(ptr));
        size += GET_SIZE(NEXT_BLKP(ptr));
        delete_block_for_coalesce(PREV_BLKP(ptr));
        delete_block_for_coalesce(NEXT_BLKP(ptr));
    }

    delete_block_for_coalesce(ptr);
    PUT(last_header_ptr, size);
    PUT(FT_PTR(last_header_ptr), size);
    insert_block_to_fitting_free_list(last_header_ptr, size);
    return last_header_ptr;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    if (!ptr) {
        return;
    }

    size_t block_size = GET_SIZE(BLOCK_START_PTR(ptr));
    insert_block_to_fitting_free_list(BLOCK_START_PTR(ptr), block_size);
    // TODO(优化点1): 多次重复聚合空闲块，但是目前有bug需要排查
    while ((ptr = coalesce(BLOCK_START_PTR(ptr))) != NULL) {

    }
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
    size_t block_size = GET_SIZE(BLOCK_START_PTR(ptr));
    size_t new_block_size = ALIGN(size + CONTROL_DATA_LEN);
    size_t copySize = 0;

    if (block_size >= new_block_size) {
        // TODO(优化点2): 将多余的空间划分为一个空闲块，但是目前有bug需要排查
//        if (block_size - new_block_size > 16) {
//            PUT(BLOCK_START_PTR(ptr), new_block_size);
//            insert_block_to_fitting_free_list((char *)oldptr + new_block_size, block_size - new_block_size);
//        }

        return oldptr;
    } else {
        newptr = mm_malloc(size);
        if (newptr == NULL) {
            return NULL;
        }

        copySize = block_size - CONTROL_DATA_LEN;
        memcpy(newptr, oldptr, copySize);
        mm_free(oldptr);
        return newptr;
    }
}














