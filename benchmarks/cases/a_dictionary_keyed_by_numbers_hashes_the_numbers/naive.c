/* naive/ written in C the way it reads: the dictionary the hash table a C programmer writes from memory for number
 * keys: the key itself, modulo the bucket count, picks a bucket, each bucket a chain of malloc'd entries, the
 * buckets doubled when there are as many entries as buckets. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Entry {
    int32_t key;
    int32_t value;
    struct Entry* next;
} Entry;

typedef struct Dictionary {
    Entry** buckets;
    int32_t bucket_count;
    int32_t count;
} Dictionary;

static uint32_t bucket_of(int32_t key, int32_t bucket_count) {
    return (uint32_t)key % (uint32_t)bucket_count;
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
            uint32_t home = bucket_of(entry->key, grown);
            entry->next = buckets[home];
            buckets[home] = entry;
            entry = next;
        }
    }
    free(dictionary->buckets);
    dictionary->buckets = buckets;
    dictionary->bucket_count = grown;
}

static Entry* dictionary_find(Dictionary* dictionary, int32_t key) {
    for (Entry* entry = dictionary->buckets[bucket_of(key, dictionary->bucket_count)]; entry != NULL; entry = entry->next) {
        if (entry->key == key) return entry;
    }
    return NULL;
}

static void dictionary_set(Dictionary* dictionary, int32_t key, int32_t value) {
    Entry* found = dictionary_find(dictionary, key);
    if (found != NULL) {
        found->value = value;
        return;
    }
    if (dictionary->count + 1 > dictionary->bucket_count) dictionary_grow(dictionary);
    Entry* entry = malloc(sizeof(Entry));
    entry->key = key;
    entry->value = value;
    uint32_t home = bucket_of(key, dictionary->bucket_count);
    entry->next = dictionary->buckets[home];
    dictionary->buckets[home] = entry;
    dictionary->count = dictionary->count + 1;
}

static void dictionary_free(Dictionary* dictionary) {
    for (int32_t bucket = 0; bucket < dictionary->bucket_count; bucket = bucket + 1) {
        Entry* entry = dictionary->buckets[bucket];
        while (entry != NULL) {
            Entry* next = entry->next;
            free(entry);
            entry = next;
        }
    }
    free(dictionary->buckets);
    free(dictionary);
}

static int32_t key_of(int32_t index) {
    return index * 7919 % 1000003;
}

static Dictionary* make_table(int32_t count) {
    Dictionary* by_number = dictionary_make();
    for (int32_t index = 0; index < count; index = index + 1) {
        int32_t key = key_of(index);
        dictionary_set(by_number, key, index * 2);
    }
    return by_number;
}

static int64_t look_up(Dictionary* by_number, int32_t count) {
    int64_t total = 0;
    for (int32_t round = 0; round < 40; round = round + 1) {
        for (int32_t index = 0; index < count; index = index + 1) {
            int32_t key = key_of(index + round);
            Entry* found = dictionary_find(by_number, key);
            if (found != NULL) {
                total = total + found->value;
            }
        }
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    Dictionary* by_number = make_table(100000);
    int64_t total = look_up(by_number, 100000);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld entries %d\n", (long long)total, by_number->count);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    dictionary_free(by_number);
    return 0;
}
