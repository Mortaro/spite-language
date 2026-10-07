/* naive/ written in C the way it reads: one Meter struct for both kinds, which keeps whether it squares as a field,
 * and meter_add testing that field on every call. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Meter {
    bool is_squared;
    int64_t total;
} Meter;

typedef struct IntegerList {
    int32_t* items;
    int32_t count;
    int32_t capacity;
} IntegerList;

static IntegerList* integer_list_make(void) {
    IntegerList* list = malloc(sizeof(IntegerList));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void integer_list_append(IntegerList* list, int32_t item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(int32_t) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static void integer_list_free(IntegerList* list) {
    free(list->items);
    free(list);
}

static Meter* meter_make(bool is_squared) {
    Meter* meter = malloc(sizeof(Meter));
    meter->is_squared = is_squared;
    meter->total = 0;
    return meter;
}

static void meter_add(Meter* meter, int32_t value) {
    if (meter->is_squared) {
        int64_t square = value;
        meter->total = meter->total + square * value;
    } else {
        meter->total = meter->total + value;
    }
}

static IntegerList* make_values(int32_t count) {
    IntegerList* values = integer_list_make();
    for (int32_t index = 0; index < count; index = index + 1) {
        integer_list_append(values, index % 1000);
    }
    return values;
}

static void measure(IntegerList* values, Meter* plain, Meter* squared) {
    for (int32_t index = 0; index < values->count; index = index + 1) {
        int32_t value = values->items[index];
        meter_add(plain, value);
        meter_add(squared, value);
    }
}

int main(void) {
    int64_t start = now_nanoseconds();
    IntegerList* values = make_values(1000000);
    Meter* plain = meter_make(false);
    Meter* squared = meter_make(true);
    for (int32_t round = 0; round < 20; round = round + 1) {
        measure(values, plain, squared);
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("sum %lld squares %lld\n", (long long)plain->total, (long long)squared->total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    integer_list_free(values);
    free(plain);
    free(squared);
    return 0;
}
