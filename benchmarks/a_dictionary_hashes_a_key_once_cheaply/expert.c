/* The same work tuned by hand: every name fits 16 bytes, so names are 16-byte slots with their length in the last
 * byte, and the dictionary is one open-addressed table (a power of two of slots, a quarter full) of the entry's
 * position and 32 bits of its FNV-1a hash, the keys and values in two arrays beside it. Each access hashes its key
 * once, compares the stored bits first and the key bytes only when they match, and is one lookup, not two. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

#define NAME_COUNT 5000
#define SLOT_COUNT 32768

typedef struct Name {
    char bytes[15];
    uint8_t length;
} Name;

typedef struct Slot {
    int32_t position;
    uint32_t fragment;
} Slot;

static Name names[NAME_COUNT];
static Name keys[NAME_COUNT];
static int32_t values[NAME_COUNT];
static Slot slots[SLOT_COUNT];
static int32_t entry_count = 0;

static uint64_t hash_of(const Name* name) {
    uint64_t hash = 1469598103934665603ull;
    for (int32_t index = 0; index < name->length; index++) {
        hash = (hash ^ (uint8_t)name->bytes[index]) * 1099511628211ull;
    }
    return hash;
}

static uint32_t home_of(uint64_t hash) {
    return (uint32_t)((hash ^ (hash >> 29)) & (SLOT_COUNT - 1));
}

static int32_t find(const Name* name) {
    uint64_t hash = hash_of(name);
    uint32_t fragment = (uint32_t)(hash >> 33);
    uint32_t slot = home_of(hash);
    while (slots[slot].position != 0) {
        int32_t position = slots[slot].position - 1;
        if (slots[slot].fragment == fragment && keys[position].length == name->length &&
            memcmp(keys[position].bytes, name->bytes, name->length) == 0) {
            return position;
        }
        slot = (slot + 1) & (SLOT_COUNT - 1);
    }
    return -1;
}

static void insert(const Name* name, int32_t value) {
    uint64_t hash = hash_of(name);
    uint32_t slot = home_of(hash);
    while (slots[slot].position != 0) slot = (slot + 1) & (SLOT_COUNT - 1);
    keys[entry_count] = *name;
    values[entry_count] = value;
    slots[slot].position = entry_count + 1;
    slots[slot].fragment = (uint32_t)(hash >> 33);
    entry_count++;
}

int main(void) {
    int64_t start = now_nanoseconds();
    for (int32_t index = 0; index < NAME_COUNT; index++) {
        names[index].length = (uint8_t)snprintf(names[index].bytes, 15, "entity_%d", index);
    }
    for (int32_t index = 0; index < NAME_COUNT; index++) {
        int32_t found = find(&names[index]);
        if (found >= 0) values[found] = index * 3;
        else insert(&names[index], index * 3);
    }
    int64_t total = 0;
    for (int32_t round = 0; round < 20; round++) {
        for (int32_t index = 0; index < 100000; index++) {
            int32_t found = find(&names[(index * 7 + round) % NAME_COUNT]);
            if (found >= 0) total += values[found];
        }
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld entries %d\n", (long long)total, entry_count);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
