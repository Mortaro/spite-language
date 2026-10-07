/* naive/ written in C the way it reads: each list literal is a list made per call, a struct and its array of numbers
 * each a malloc, filled, read and freed. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct List {
    int32_t* items;
    int32_t count;
    int32_t capacity;
} List;

static List* list_make(int32_t capacity) {
    List* list = malloc(sizeof(List));
    list->items = malloc(sizeof(int32_t) * capacity);
    list->count = 0;
    list->capacity = capacity;
    return list;
}

static void list_append(List* list, int32_t value) {
    list->items[list->count] = value;
    list->count = list->count + 1;
}

static void list_free(List* list) {
    free(list->items);
    free(list);
}

static int32_t largest_remainder(int32_t round) {
    List* remainders = list_make(4);
    list_append(remainders, round % 10);
    list_append(remainders, round % 7);
    list_append(remainders, round % 13);
    list_append(remainders, round % 3);
    List* weights = list_make(4);
    list_append(weights, 4);
    list_append(weights, 3);
    list_append(weights, 2);
    list_append(weights, 1);
    int32_t biggest = 0;
    for (int32_t index = 0; index < remainders->count; index = index + 1) {
        if (index >= weights->count) abort();
        int32_t weighted = remainders->items[index] * weights->items[index];
        if (weighted > biggest) biggest = weighted;
    }
    list_free(remainders);
    list_free(weights);
    return biggest;
}

static int64_t all_rounds(int32_t rounds) {
    int64_t total = 0;
    for (int32_t round = 0; round < rounds; round = round + 1) {
        int32_t biggest = largest_remainder(round);
        total = total + biggest;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int64_t total = all_rounds(10000000);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
