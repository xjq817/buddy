#include "buddy.h"
#include <stdlib.h>
#include <stdio.h>
#define NULL ((void *)0)

#define PAGE 4096

struct list {
    int start; // 起始位置
    struct list* next;
};

struct free {
    struct list head; // 头节点
    int* area; // 伙伴忙的状态，为1则其中1个忙，用于合并
    int size; //area的大小
    int num; //多少空块
    int order; //存储 2^order 的页面
};

struct free* zone; // 每个rank相关信息
int size; // 多少rank，0~size-1

void* init; // 空间分配起始地址
int* alloc_start;
int* alloc_rank;

void insert(struct free* node, int start) {
    struct list* res = (struct list*)malloc(sizeof(struct list));
    res->start = start;
    res->next = node->head.next;
    node->head.next = res;
    node->num++;
    node->area[start >> node->order] ^= 1;
}

int begin(struct free* node){
    return node->head.next->start;
}

void erase(struct free* node, int start){
    struct list* res = &node->head;
    while(res->next->start != start) res = res->next;
    struct list* tmp = res->next->next;
    free(res->next);
    res->next = tmp;
    node->area[start >> node->order] ^= 1;
    node->num--;
}

int init_page(void *p, int pgcount) {
    init = p;
    size = 0;
    while((1 << size) <= pgcount) size++;
    zone = (struct free*)malloc(sizeof(struct free) * size);
    alloc_start = (int*)malloc(sizeof(int) * pgcount);
    alloc_rank = (int*)malloc(sizeof(int) * pgcount);
    for (int i = 0; i < pgcount; i++) {
        alloc_start[i] = -1;
        alloc_rank[i] = size - 1;
    }
    for (int i = 0; i < size; i++) {
        zone[i].order = i + 1;
        zone[i].size = 1 << (size - 1 - i);
        zone[i].area = malloc(sizeof(int) * zone[i].size);
        for (int j = 0; j < zone[i].size; j++)
            zone[i].area[j] = 0;
        zone[i].num = 0;
        zone[i].head.next = NULL;
    }
    insert(&zone[size - 1], 0);
    return OK;
}

void *alloc_pages(int rank) {
    rank--;
    if (rank < 0 || rank >= size) return (void*)-EINVAL;
    for (int i = rank; i < size; i++) if (zone[i].num > 0) {
        int start = begin(&zone[i]);
        erase(&zone[i], start);
        start += (1 << i);
        for (int j = i - 1; j >= rank; j--){
            start -= (1 << j);
            insert(&zone[j], start);
        }
        start -= (1 << rank);
        for (int j = 0; j < (1 << rank); j++) {
            alloc_start[j + start] = start;
            alloc_rank[j + start] = rank;
        }
        return init + start * PAGE;
    }
    return (void*)-ENOSPC;
}

int return_pages(void *p) {
    if (p == NULL) return -EINVAL;
    int start = p - init;
    if (start < 0 || start % PAGE != 0) return -EINVAL;
    start /= PAGE;
    if (start >= (1 << (size - 1))) return -EINVAL;
    if (alloc_start[start] == -1) return -EINVAL;
    start = alloc_start[start];
    int rank = alloc_rank[start];
    for (int j = 0; j < (1 << rank); j++) {
        alloc_start[j + start] = -1;
        alloc_rank[j + start] = size - 1;
    }
    for (int i = rank; i < size; i++) {
        if (zone[i].area[start >> (1 + i)] == 1) {
            int another = start ^ (1 << i);
            erase(&zone[i], another);
            if (start > another) start = another;
        }
        else {
            insert(&zone[i], start);
            break;
        }
    }
    return OK;
}

int query_ranks(void *p) {
    if (p == NULL) return -EINVAL;
    int start = p - init;
    if (start < 0 || start % PAGE != 0) return -EINVAL;
    start /= PAGE;
    if (start >= (1 << (size - 1))) return -EINVAL;
    return alloc_rank[start] + 1;
}

int query_page_counts(int rank) {
    rank--;
    if (rank < 0 || rank >= size) return -EINVAL;
    return zone[rank].num;
}