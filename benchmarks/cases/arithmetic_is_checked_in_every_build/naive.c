/* naive/ written in C the way it reads: a growable array of numbers, and the same loops and calls. C's own
 * arithmetic checks nothing: a sum that did not fit would wrap (or, signed, be undefined) without a word. */
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

static int32_t checksum(IntegerList* values, int32_t round) {
    int32_t total = 0;
    for (int32_t index = 0; index < values->count; index = index + 1) {
        total = total + values->items[index] * 3 + round;
    }
    return total;
}

static int64_t rounds(IntegerList* values) {
    int64_t total = 0;
    for (int32_t round = 0; round < 400; round = round + 1) {
        int32_t sum = checksum(values, round);
        total = total + sum;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    IntegerList* values = list_make();
    for (int32_t index = 0; index < 100000; index = index + 1) {
        list_append(values, index % 1000);
    }
    int64_t total = rounds(values);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    list_free(values);
    return 0;
}
