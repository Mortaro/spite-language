/* naive/ written in C the way it reads: a growable array of floats, and the same loops and calls. In C a decimal
 * literal is a double, so `(value + shift) * 1.5 + 0.25` on a float widens the float to double, works in double
 * and narrows the answer back to store it. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct FloatList {
    float* items;
    int32_t count;
    int32_t capacity;
} FloatList;

static FloatList* list_make(void) {
    FloatList* list = malloc(sizeof(FloatList));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void list_append(FloatList* list, float item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(float) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static void list_free(FloatList* list) {
    free(list->items);
    free(list);
}

static float scaled_sum(FloatList* values, float shift) {
    float total = 0.0;
    for (int32_t index = 0; index < values->count; index = index + 1) {
        total = total + (values->items[index] + shift) * 1.5 + 0.25;
    }
    return total;
}

static double rounds(FloatList* values) {
    double total = 0.0;
    for (int32_t round = 0; round < 4000; round = round + 1) {
        float shift = round % 8;
        float sum = scaled_sum(values, shift);
        total = total + sum;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    FloatList* values = list_make();
    for (int32_t index = 0; index < 50000; index = index + 1) {
        float quarters = index % 64;
        list_append(values, quarters * 0.25);
    }
    double total = rounds(values);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    list_free(values);
    return 0;
}
