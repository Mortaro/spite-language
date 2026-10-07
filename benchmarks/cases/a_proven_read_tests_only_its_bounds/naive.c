/* naive/ written in C the way it reads: a growable array of ints for the List<Integer>, a function per Spite
 * function with the same loops, the count read through the list where the Spite reads it. */
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

static void list_append(IntegerList* list, int32_t value) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(int32_t) * list->capacity);
    }
    list->items[list->count] = value;
    list->count = list->count + 1;
}

static void list_free(IntegerList* list) {
    free(list->items);
    free(list);
}

static int32_t window_total(IntegerList* values, int32_t weight) {
    int32_t total = 0;
    int32_t at = 0;
    while (at + 2 < values->count) {
        total = total + values->items[at] * weight + values->items[at + 1] + values->items[at + 2];
        at = at + 1;
    }
    return total;
}

static int32_t rise_count(IntegerList* values, int32_t threshold) {
    int32_t count = values->count;
    int32_t rises = 0;
    int32_t previous = 0;
    int32_t index = 0;
    while (index < count) {
        int32_t value = values->items[index];
        if (value - previous > threshold) {
            rises = rises + 1;
        }
        previous = value;
        index = index + 1;
    }
    return rises;
}

static int64_t rounds(IntegerList* values) {
    int64_t total = 0;
    for (int32_t round = 0; round < 300; round = round + 1) {
        int32_t weight = round % 3 + 1;
        int32_t windows = window_total(values, weight);
        int32_t threshold = round % 50;
        int32_t rises = rise_count(values, threshold);
        total = total + windows + rises;
    }
    return total;
}

int main(void) {
    IntegerList* values = list_make();
    for (int32_t index = 0; index < 100000; index = index + 1) {
        list_append(values, index * 37 % 101);
    }
    int64_t start = now_nanoseconds();
    int64_t total = rounds(values);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    list_free(values);
    return 0;
}
