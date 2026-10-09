/* naive/ written in C the way it reads: points_for makes its dictionary of three entries on every call, looks up
 * the three keys by hashing them, and frees it. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

typedef struct Result {
    int32_t golds;
    int32_t silvers;
    int32_t bronzes;
} Result;

typedef struct List {
    void** items;
    int32_t count;
    int32_t capacity;
} List;

typedef struct Entry {
    const char* key;
    uint64_t hash;
    int32_t value;
    bool used;
} Entry;

typedef struct Dictionary {
    Entry* entries;
    int32_t count;
    int32_t capacity;
} Dictionary;

static List* list_make(void) {
    List* list = malloc(sizeof(List));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void list_append(List* list, void* item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(void*) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static void list_free(List* list) {
    free(list->items);
    free(list);
}

static uint64_t text_hash(const char* text) {
    uint64_t hash = 14695981039346656037ull;
    for (const char* at = text; *at != 0; at = at + 1) {
        hash = (hash ^ (uint8_t)*at) * 1099511628211ull;
    }
    return hash;
}

static Dictionary* dictionary_make(void) {
    Dictionary* dictionary = malloc(sizeof(Dictionary));
    dictionary->capacity = 8;
    dictionary->count = 0;
    dictionary->entries = calloc((size_t)dictionary->capacity, sizeof(Entry));
    return dictionary;
}

static void dictionary_set(Dictionary* dictionary, const char* key, int32_t value) {
    uint64_t hash = text_hash(key);
    int32_t slot = (int32_t)(hash % (uint64_t)dictionary->capacity);
    while (dictionary->entries[slot].used) {
        Entry* entry = &dictionary->entries[slot];
        if (entry->hash == hash && strcmp(entry->key, key) == 0) {
            entry->value = value;
            return;
        }
        slot = (slot + 1) % dictionary->capacity;
    }
    dictionary->entries[slot].key = key;
    dictionary->entries[slot].hash = hash;
    dictionary->entries[slot].value = value;
    dictionary->entries[slot].used = true;
    dictionary->count = dictionary->count + 1;
}

static bool dictionary_get(Dictionary* dictionary, const char* key, int32_t* value) {
    uint64_t hash = text_hash(key);
    int32_t slot = (int32_t)(hash % (uint64_t)dictionary->capacity);
    while (dictionary->entries[slot].used) {
        Entry* entry = &dictionary->entries[slot];
        if (entry->hash == hash && strcmp(entry->key, key) == 0) {
            *value = entry->value;
            return true;
        }
        slot = (slot + 1) % dictionary->capacity;
    }
    return false;
}

static void dictionary_free(Dictionary* dictionary) {
    free(dictionary->entries);
    free(dictionary);
}

static Result* result_make(int32_t golds, int32_t silvers, int32_t bronzes) {
    Result* result = malloc(sizeof(Result));
    result->golds = golds;
    result->silvers = silvers;
    result->bronzes = bronzes;
    return result;
}

static int32_t points_for(Result* result) {
    Dictionary* points = dictionary_make();
    dictionary_set(points, "gold", 50);
    dictionary_set(points, "silver", 20);
    dictionary_set(points, "bronze", 5);
    int32_t gold = 0;
    int32_t silver = 0;
    int32_t bronze = 0;
    if (!dictionary_get(points, "gold", &gold)) abort();
    if (!dictionary_get(points, "silver", &silver)) abort();
    if (!dictionary_get(points, "bronze", &bronze)) abort();
    int32_t total = result->golds * gold + result->silvers * silver + result->bronzes * bronze;
    dictionary_free(points);
    return total;
}

static int64_t totals(List* results) {
    int64_t total = 0;
    for (int32_t round = 0; round < 40; round = round + 1) {
        for (int32_t index = 0; index < results->count; index = index + 1) {
            Result* result = results->items[index];
            int32_t points = points_for(result);
            total = total + points;
        }
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    List* results = list_make();
    for (int32_t index = 0; index < 200000; index = index + 1) {
        Result* result = result_make(index % 4, index % 7, index % 11);
        list_append(results, result);
    }
    int64_t total = totals(results);
    int64_t microseconds = microseconds_since(start);
    printf("points %lld\n", (long long)total);
    print_microseconds(microseconds);
    for (int32_t index = 0; index < results->count; index = index + 1) {
        free(results->items[index]);
    }
    list_free(results);
    return 0;
}
