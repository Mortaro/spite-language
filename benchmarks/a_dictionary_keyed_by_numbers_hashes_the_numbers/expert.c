/* The same work tuned by hand: one open-addressed table made once at its size (a power of two, under a quarter
 * full), each slot the key and its value side by side, a key hashed by one multiply and its high bits taken as the
 * slot, linear probing, and an empty slot marked by a key no entry has. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define ENTRY_COUNT 100000
#define SLOT_BITS 19
#define SLOT_COUNT (1 << SLOT_BITS)
#define EMPTY_KEY (-1)

typedef struct Slot {
    int32_t key;
    int32_t value;
} Slot;

static Slot slots[SLOT_COUNT];
static int32_t entry_count = 0;

static uint32_t home_of(int32_t key) {
    return (uint32_t)(((uint64_t)(uint32_t)key * 11400714819323198485ull) >> (64 - SLOT_BITS));
}

static void set(int32_t key, int32_t value) {
    uint32_t slot = home_of(key);
    while (slots[slot].key != EMPTY_KEY && slots[slot].key != key) slot = (slot + 1) & (SLOT_COUNT - 1);
    if (slots[slot].key == EMPTY_KEY) entry_count++;
    slots[slot].key = key;
    slots[slot].value = value;
}

static const Slot* find(int32_t key) {
    uint32_t slot = home_of(key);
    while (slots[slot].key != EMPTY_KEY) {
        if (slots[slot].key == key) return &slots[slot];
        slot = (slot + 1) & (SLOT_COUNT - 1);
    }
    return NULL;
}

static int32_t key_of(int32_t index) {
    return index * 7919 % 1000003;
}

int main(void) {
    int64_t start = now_nanoseconds();
    for (int32_t slot = 0; slot < SLOT_COUNT; slot++) slots[slot].key = EMPTY_KEY;
    for (int32_t index = 0; index < ENTRY_COUNT; index++) set(key_of(index), index * 2);
    int64_t total = 0;
    for (int32_t round = 0; round < 40; round++) {
        for (int32_t index = 0; index < ENTRY_COUNT; index++) {
            const Slot* found = find(key_of(index + round));
            if (found != NULL) total += found->value;
        }
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld entries %d\n", (long long)total, entry_count);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
