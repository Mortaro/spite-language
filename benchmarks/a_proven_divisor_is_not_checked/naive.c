/* naive/ written in C the way it reads: a growable array of numbers, and the same loops and calls. A C programmer
 * writes no zero test before the division here: the caller always passes a divisor of one or more. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct IntegerList {
    int32_t* items;
    int32_t count;
    int32_t capacity;
} IntegerList;

static IntegerList* list_make(void) {
    IntegerList* list = malloc(sizeof(IntegerList));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void list_append(IntegerList* list, int32_t item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(int32_t) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static void list_free(IntegerList* list) {
    free(list->items);
    free(list);
}

static int64_t shares(IntegerList* amounts, int32_t parts) {
    int64_t total = 0;
    for (int32_t index = 0; index < amounts->count; index = index + 1) {
        int32_t amount = amounts->items[index];
        total = total + amount / parts + amount % parts;
    }
    return total;
}

static int64_t rounds(IntegerList* amounts) {
    int64_t total = 0;
    for (int32_t round = 0; round < 20; round = round + 1) {
        int32_t parts = round % 7 + 1;
        int64_t shared = shares(amounts, parts);
        total = total + shared;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    IntegerList* amounts = list_make();
    for (int32_t index = 0; index < 1000000; index = index + 1) {
        list_append(amounts, index % 9973);
    }
    int64_t total = rounds(amounts);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    list_free(amounts);
    return 0;
}
