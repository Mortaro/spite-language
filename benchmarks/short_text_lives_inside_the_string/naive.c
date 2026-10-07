/* naive/ written in C the way it reads: each label a string of its own on the heap (malloc, then snprintf into
 * it), a growable array of pointers for the list, strlen for a length, and every label freed with its list. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

typedef struct StringList {
    char** items;
    int32_t count;
    int32_t capacity;
} StringList;

static StringList* list_make(void) {
    StringList* list = malloc(sizeof(StringList));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void list_append(StringList* list, char* item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(char*) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static void list_free(StringList* list) {
    for (int32_t index = 0; index < list->count; index = index + 1) free(list->items[index]);
    free(list->items);
    free(list);
}

static StringList* make_labels(int32_t count, int32_t round) {
    StringList* labels = list_make();
    for (int32_t index = 0; index < count; index = index + 1) {
        char* label = malloc(32);
        snprintf(label, 32, "r%d n%d", round, index);
        list_append(labels, label);
    }
    return labels;
}

static int32_t checksum(StringList* labels) {
    int32_t total = 0;
    for (int32_t index = 0; index < labels->count; index = index + 1) {
        char* label = labels->items[index];
        int32_t last = (int32_t)strlen(label) - 1;
        total = total + (int32_t)strlen(label) + (uint8_t)label[last];
    }
    return total;
}

static int64_t rounds(int32_t count) {
    int64_t total = 0;
    for (int32_t round = 0; round < count; round = round + 1) {
        StringList* labels = make_labels(100000, round);
        int32_t sum = checksum(labels);
        total = total + sum;
        list_free(labels);
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int64_t total = rounds(20);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
