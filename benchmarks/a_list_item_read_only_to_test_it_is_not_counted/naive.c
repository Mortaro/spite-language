/* naive/ written in C the way it reads: a list of pointers to entries, and each probe asks whether the list has an
 * entry at that place, as `if entries[at]` does: the place inside the list and a pointer in its slot. C keeps no
 * counts, so the test reads only the slot. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Entry {
    int32_t key;
} Entry;

typedef struct List {
    Entry** items;
    int32_t count;
    int32_t capacity;
} List;

static List* entries;

static List* list_make(void) {
    List* list = malloc(sizeof(List));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void list_append(List* list, Entry* item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(Entry*) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static Entry* list_get(List* list, int32_t index) {
    if (index >= 0 && index < list->count) return list->items[index];
    return NULL;
}

static Entry* entry_make(int32_t key) {
    Entry* entry = malloc(sizeof(Entry));
    entry->key = key;
    return entry;
}

static int32_t probe(int32_t probes) {
    int32_t found = 0;
    int32_t missing = 0;
    int32_t at = 0;
    for (int32_t index = 0; index < probes; index = index + 1) {
        at = (at + 7919) % 400000;
        if (list_get(entries, at) != NULL) {
            found = found + 1;
        } else {
            missing = missing + 1;
        }
    }
    if (found + missing != probes) abort();
    return found;
}

static int32_t sum_key(List* list) {
    int32_t total = 0;
    for (int32_t index = 0; index < list->count; index = index + 1) {
        total = total + list->items[index]->key;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    entries = list_make();
    for (int32_t index = 0; index < 200000; index = index + 1) {
        Entry* made = entry_make(index % 1000);
        list_append(entries, made);
    }
    int32_t found = probe(20000000);
    int32_t keys = sum_key(entries);
    int64_t microseconds = microseconds_since(start);
    printf("found %d keys %d\n", found, keys);
    print_microseconds(microseconds);
    for (int32_t index = 0; index < entries->count; index = index + 1) {
        free(entries->items[index]);
    }
    free(entries->items);
    free(entries);
    return 0;
}
