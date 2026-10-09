/* The same work tuned by hand: an open-addressing table made at its final size (2^20 slots for 500 000 keys), so it
 * never grows, with the keys and the values in two arrays and -1 marking an empty slot, so a probe reads four bytes
 * instead of twelve and a lookup that misses stops at the first empty key. The hash is the naive program's. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define SHIFT 44
#define CAPACITY (1u << (64 - SHIFT))
#define MASK (CAPACITY - 1)

static inline uint32_t home(int32_t key) {
    return (uint32_t)(((uint64_t)(int64_t)key * 11400714819323198485ULL) >> SHIFT);
}

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t* keys = malloc(sizeof(int32_t) * CAPACITY);
    int32_t* values = malloc(sizeof(int32_t) * CAPACITY);
    for (uint32_t slot = 0; slot < CAPACITY; slot++) {
        keys[slot] = -1;
    }
    int32_t count = 0;
    for (int32_t index = 0; index < 500000; index++) {
        int32_t key = index * 7;
        uint32_t slot = home(key);
        while (keys[slot] != -1 && keys[slot] != key) {
            slot = (slot + 1) & MASK;
        }
        if (keys[slot] == -1) {
            count++;
        }
        keys[slot] = key;
        values[slot] = index;
    }
    int64_t total = 0;
    int32_t found = 0;
    for (int32_t round = 0; round < 10; round++) {
        for (int32_t index = 0; index < 500000; index++) {
            int32_t key = index * 3;
            uint32_t slot = home(key);
            for (;;) {
                int32_t held = keys[slot];
                if (held == key) {
                    total += values[slot];
                    found++;
                    break;
                }
                if (held == -1) {
                    break;
                }
                slot = (slot + 1) & MASK;
            }
        }
    }
    int64_t microseconds = microseconds_since(start);
    printf("entries %d found %d total %lld\n", count, found, (long long)total);
    print_microseconds(microseconds);
    free(keys);
    free(values);
    return 0;
}
