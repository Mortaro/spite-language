/* The same work tuned by hand: the entries' keys as one column, a byte per place saying whether an entry is
 * there, the probe's remainder kept by a subtraction instead of a division, and each probe one unsigned compare
 * and one byte read. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define ENTRY_COUNT 200000

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t* keys = malloc(sizeof(int32_t) * ENTRY_COUNT);
    uint8_t* present = malloc(ENTRY_COUNT);
    for (int32_t index = 0; index < ENTRY_COUNT; index++) {
        keys[index] = index % 1000;
        present[index] = 1;
    }
    int32_t probes = 20000000;
    int32_t found = 0;
    int32_t at = 0;
    for (int32_t index = 0; index < probes; index++) {
        at += 7919;
        if (at >= 400000) at -= 400000;
        if ((uint32_t)at < ENTRY_COUNT) found += present[at];
    }
    int32_t key_total = 0;
    for (int32_t index = 0; index < ENTRY_COUNT; index++) {
        key_total += keys[index];
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("found %d keys %d\n", found, key_total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(keys);
    free(present);
    return 0;
}
