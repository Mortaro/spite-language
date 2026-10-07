/* naive/ written in C the way it reads: the values of a variadic call arrive in a list, so every call of largest
 * makes a list (a malloc for the struct, a growable array of numbers), appends the four values and frees it. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct List {
    int32_t* items;
    int32_t count;
    int32_t capacity;
} List;

static List* list_make(void) {
    List* list = malloc(sizeof(List));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void list_append(List* list, int32_t value) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(int32_t) * list->capacity);
    }
    list->items[list->count] = value;
    list->count = list->count + 1;
}

static void list_free(List* list) {
    free(list->items);
    free(list);
}

static int32_t largest(List* values) {
    int32_t biggest = 0;
    for (int32_t index = 0; index < values->count; index = index + 1) {
        if (values->items[index] > biggest) biggest = values->items[index];
    }
    return biggest;
}

static int64_t all_rounds(int32_t rounds) {
    int64_t total = 0;
    for (int32_t round = 0; round < rounds; round = round + 1) {
        List* values = list_make();
        list_append(values, round % 10);
        list_append(values, round % 7);
        list_append(values, round % 13);
        list_append(values, round % 3);
        int32_t biggest = largest(values);
        list_free(values);
        total = total + biggest;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int64_t total = all_rounds(2000000);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
