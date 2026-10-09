/* naive/ written in C the way it reads: each name a string of its own on the heap, a growable array of pointers for
 * the list, and the dictionary the hash table a C programmer writes from memory: a hash of `hash * 31 + character`,
 * a bucket found with `%`, a chain of malloc'd entries per bucket, each holding its own copy of the key, and strcmp
 * on every entry of the chain. A lookup written twice in the Spite is two lookups here too. */
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

typedef struct Entry {
    char* key;
    int32_t value;
    struct Entry* next;
} Entry;

typedef struct Dictionary {
    Entry** buckets;
    int32_t bucket_count;
    int32_t count;
} Dictionary;

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

static uint32_t hash_of(const char* key) {
    uint32_t hash = 0;
    for (const char* at = key; *at != 0; at = at + 1) hash = hash * 31 + (uint8_t)*at;
    return hash;
}

static Dictionary* dictionary_make(void) {
    Dictionary* dictionary = malloc(sizeof(Dictionary));
    dictionary->bucket_count = 16;
    dictionary->buckets = calloc(dictionary->bucket_count, sizeof(Entry*));
    dictionary->count = 0;
    return dictionary;
}

static void dictionary_grow(Dictionary* dictionary) {
    int32_t grown = dictionary->bucket_count * 2;
    Entry** buckets = calloc(grown, sizeof(Entry*));
    for (int32_t bucket = 0; bucket < dictionary->bucket_count; bucket = bucket + 1) {
        Entry* entry = dictionary->buckets[bucket];
        while (entry != NULL) {
            Entry* next = entry->next;
            uint32_t home = hash_of(entry->key) % grown;
            entry->next = buckets[home];
            buckets[home] = entry;
            entry = next;
        }
    }
    free(dictionary->buckets);
    dictionary->buckets = buckets;
    dictionary->bucket_count = grown;
}

static Entry* dictionary_find(Dictionary* dictionary, const char* key) {
    uint32_t home = hash_of(key) % dictionary->bucket_count;
    for (Entry* entry = dictionary->buckets[home]; entry != NULL; entry = entry->next) {
        if (strcmp(entry->key, key) == 0) return entry;
    }
    return NULL;
}

static void dictionary_set(Dictionary* dictionary, const char* key, int32_t value) {
    Entry* found = dictionary_find(dictionary, key);
    if (found != NULL) {
        found->value = value;
        return;
    }
    if (dictionary->count + 1 > dictionary->bucket_count) dictionary_grow(dictionary);
    Entry* entry = malloc(sizeof(Entry));
    entry->key = malloc(strlen(key) + 1);
    strcpy(entry->key, key);
    entry->value = value;
    uint32_t home = hash_of(key) % dictionary->bucket_count;
    entry->next = dictionary->buckets[home];
    dictionary->buckets[home] = entry;
    dictionary->count = dictionary->count + 1;
}

static void dictionary_free(Dictionary* dictionary) {
    for (int32_t bucket = 0; bucket < dictionary->bucket_count; bucket = bucket + 1) {
        Entry* entry = dictionary->buckets[bucket];
        while (entry != NULL) {
            Entry* next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }
    free(dictionary->buckets);
    free(dictionary);
}

static StringList* make_names(int32_t count) {
    StringList* names = list_make();
    for (int32_t index = 0; index < count; index = index + 1) {
        char* name = malloc(32);
        snprintf(name, 32, "entity_%d", index);
        list_append(names, name);
    }
    return names;
}

static Dictionary* index_names(StringList* names) {
    Dictionary* by_name = dictionary_make();
    for (int32_t index = 0; index < names->count; index = index + 1) {
        dictionary_set(by_name, names->items[index], index * 3);
    }
    return by_name;
}

static int64_t look_up(StringList* names, Dictionary* by_name) {
    int64_t total = 0;
    for (int32_t round = 0; round < 20; round = round + 1) {
        for (int32_t index = 0; index < 100000; index = index + 1) {
            int32_t position = (index * 7 + round) % 5000;
            char* name = names->items[position];
            if (dictionary_find(by_name, name) != NULL) {
                total = total + dictionary_find(by_name, name)->value;
            }
        }
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    StringList* names = make_names(5000);
    Dictionary* by_name = index_names(names);
    int64_t total = look_up(names, by_name);
    int64_t microseconds = microseconds_since(start);
    printf("total %lld entries %d\n", (long long)total, by_name->count);
    print_microseconds(microseconds);
    dictionary_free(by_name);
    list_free(names);
    return 0;
}
