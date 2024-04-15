#define _GNU_SOURCE
#include "cachelab.h"

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>

struct Cache_Node {
    struct Cache_Node *front;
    struct Cache_Node *next;
    unsigned long long tag;
    char *offset;
};

struct LRU_Cache {
    struct Cache_Node *begin;
    struct Cache_Node *end;
    struct Cache_Node *idle_list;
};

struct LRU_Cache **cache;
int hits, misses, evictions;
int tag_bit_len, set_bit_len, block_offset_bit_len, line_num_peer_set;
int verbose;

uint64_t convert_to_addr_ll(char* buf) {
    uint64_t ret = 0;
    int len = strlen(buf);
    //buf[len] = 32;

    for (int i = len - 1; i >= 0; --i) {
        char c = buf[i];
        if (c >= '0' && c <= '9') {
            c = c - '0';
        } else {
            c = (c - 'a') + 10;
        }

        ret = ret | ((uint64_t)c << ((len - 1 - i) * 4));
    }

    return ret;
}

struct Cache_Node* init_cache_node() {
    struct Cache_Node* node = (struct Cache_Node*) malloc(sizeof(struct Cache_Node));
    memset(node, '\0', sizeof(struct Cache_Node));
    return node;
}

struct LRU_Cache* init_one_cache_set() {
    struct LRU_Cache* lru_cache = (struct LRU_Cache*) malloc(sizeof(struct LRU_Cache));
    lru_cache->begin = init_cache_node();
    lru_cache->end = init_cache_node();
    lru_cache->begin->next = lru_cache->end;
    lru_cache->end->front = lru_cache->begin;

    for (int i = 0; i < line_num_peer_set; ++i) {
        struct Cache_Node* node = init_cache_node();
        if (i == 0) {
            lru_cache->idle_list = node;
        } else {
            node->next = lru_cache->idle_list;
            lru_cache->idle_list->front = node;
            lru_cache->idle_list = node;
        }
    }

    return lru_cache;
}

void init_cache() {
    size_t malloc_size = pow(2, set_bit_len) * sizeof(struct LRU_Cache*);
    cache = (struct LRU_Cache**) malloc(malloc_size);
    memset(cache, '\0', malloc_size);

    for (int i = 0; i < pow(2, set_bit_len); ++i) {
        cache[i] = init_one_cache_set();
    }
}

struct Cache_Node* cache_line_exist(uint64_t tag, struct LRU_Cache* lru_cache) {
    struct Cache_Node* node = lru_cache->begin->next;

    for (; node != lru_cache->end; node = node->next) {
        if (node->tag == tag) {
            return node;
        }
    }
    return NULL;
}

void insert_before_node(struct Cache_Node* target_node, struct Cache_Node* insert_node) {
    insert_node->front = target_node->front;
    target_node->front->next = insert_node;
    target_node->front = insert_node;
    insert_node->next = target_node;
}

struct Cache_Node* delete_node(struct Cache_Node* delete_node) {
    delete_node->front->next = delete_node->next;
    delete_node->next->front = delete_node->front;
    delete_node->front = NULL;
    delete_node->next = NULL;
    return delete_node;
}

void fetch_cache_block(uint64_t tag, uint64_t set) {
    struct LRU_Cache* lru_cache = cache[set];
    struct Cache_Node* node = cache_line_exist(tag, lru_cache);
    if (!node) {
        ++misses;
        if (verbose) {
            printf("miss ");
        }

        if (!lru_cache->idle_list) {
            ++evictions;
            if (verbose) {
                printf("eviction ");
            }

            node = lru_cache->begin->next;
            delete_node(node);
        } else {
            node = lru_cache->idle_list;
            lru_cache->idle_list = lru_cache->idle_list->next;
            if (lru_cache->idle_list) {
                lru_cache->idle_list->front = NULL;
            }
            node->next = NULL;
        }
        node->tag = tag;
        insert_before_node(lru_cache->end, node);
    } else {
        ++hits;
        if (verbose) {
            printf("hit ");
        }
        delete_node(node);
        insert_before_node(lru_cache->end, node);
    }
}

void process_one_line(const char* line) {
    if (line[0] == 'I') {
        return;
    }

    char op = 0;
    char buf[9] = { '\0' };
    int offset = 0;
    uint64_t address = 0;

    if (sscanf(line, " %c %[^,],%d\n", &op, buf, &offset) == -1) {
        exit(1);
    }

    address = convert_to_addr_ll(buf);
    uint64_t tag = address & ~(UINT64_MAX >> tag_bit_len);
    uint64_t set = (address & (UINT64_MAX >> tag_bit_len) & (UINT64_MAX << block_offset_bit_len)) >> block_offset_bit_len;

    if (verbose) {
        printf("%s", line);
    }

    switch (op) {
        case 'L':
            fetch_cache_block(tag, set);
            break;
        case 'S':
            fetch_cache_block(tag, set);
            break;
        case 'M':
            fetch_cache_block(tag, set);
            ++hits;
            if (verbose) {
                printf("hit ");
            }
            break;
        default:
            break;
    }

    if (verbose) {
        printf("\n");
    }
}

int main(int argc, char *argv[])
{
    int opt = -1;
    FILE *trace_file = NULL;
    char *line = NULL;
    size_t line_len = 0;
    ssize_t getline_ret = -1;

    // 选项字符串定义了程序接受的选项，冒号表示该选项需要一个参数
    while ((opt = getopt(argc, argv, "s:E:b:t:v")) != -1) {
        switch (opt) {
            case 's':
                set_bit_len = atoi(optarg);
                break;
            case 'E':
                line_num_peer_set = atoi(optarg);
                break;
            case 'b':  // 不带参数的选项
                block_offset_bit_len = atoi(optarg);
                break;
            case 't':
                trace_file = fopen(optarg, "r");
                if (trace_file == NULL) {
                    perror("打开文件失败");
                    return 1;
                }
                break;
            case 'v':
                verbose = 1;
                break;
            default:
                break;
        }
    }
    tag_bit_len = 64 - block_offset_bit_len - set_bit_len;

    init_cache();

    while (1) {
        getline_ret = getline(&line, &line_len, trace_file);
        if (getline_ret == -1) {
            break;
        }
        process_one_line(line);
    }

    printSummary(hits, misses, evictions);
    return 0;
}
