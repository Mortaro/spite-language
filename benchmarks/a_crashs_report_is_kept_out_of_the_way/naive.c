/* naive/ written in C the way it reads: two growable arrays of numbers, and three hundred rounds of adding up the
 * picked values, each pick checked against the list with its report written where the check is, as a C
 * programmer writes a check. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

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

static void integer_list_append(IntegerList* list, int32_t value) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(int32_t) * list->capacity);
    }
    list->items[list->count] = value;
    list->count = list->count + 1;
}

static void integer_list_free(IntegerList* list) {
    free(list->items);
    free(list);
}

static int64_t round_total(IntegerList* values, IntegerList* picks, int32_t round) {
    int64_t total = 0;
    for (int32_t index = 0; index < picks->count; index = index + 1) {
        int32_t pick = picks->items[index] + round;
        if (pick < 0 || pick >= values->count) {
            fflush(stdout);
            fprintf(stderr, "crash naive.spite:39 in Naive.round_total\tvalues[pick] is missing: index %d, count %d\tround=%d\ttotal=%lld\tindex=%d\n",
                pick, values->count, round, (long long)total, index);
            exit(1);
        }
        total = total + values->items[pick];
    }
    return total;
}

static int64_t picked_total(IntegerList* values, IntegerList* picks) {
    int64_t total = 0;
    for (int32_t round = 0; round < 300; round = round + 1) {
        total = total + round_total(values, picks, round);
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    IntegerList* values = integer_list_make();
    for (int32_t index = 0; index < 1300; index = index + 1) {
        integer_list_append(values, index * 7 % 1000);
    }
    IntegerList* picks = integer_list_make();
    for (int32_t index = 0; index < 100000; index = index + 1) {
        integer_list_append(picks, index * 13 % 1000);
    }
    int64_t total = picked_total(values, picks);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    integer_list_free(values);
    integer_list_free(picks);
    return 0;
}
