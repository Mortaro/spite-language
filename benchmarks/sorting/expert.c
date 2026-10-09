/* The same work tuned by hand: the numbers are below a million, so they are sorted by two passes of a radix sort on
 * ten bits each instead of a quicksort, which compares nothing and reads memory in order. The checksum and the
 * order test read the sorted numbers as the naive program does. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

#define COUNT 2000000
#define BITS 10
#define BUCKETS (1 << BITS)

static void radix_pass(const int32_t* restrict from, int32_t* restrict into, int shift) {
    int32_t starts[BUCKETS];
    memset(starts, 0, sizeof(starts));
    for (int32_t index = 0; index < COUNT; index++) {
        starts[(from[index] >> shift) & (BUCKETS - 1)]++;
    }
    int32_t place = 0;
    for (int32_t bucket = 0; bucket < BUCKETS; bucket++) {
        int32_t size = starts[bucket];
        starts[bucket] = place;
        place += size;
    }
    for (int32_t index = 0; index < COUNT; index++) {
        into[starts[(from[index] >> shift) & (BUCKETS - 1)]++] = from[index];
    }
}

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t* numbers = malloc(sizeof(int32_t) * COUNT);
    int32_t* spare = malloc(sizeof(int32_t) * COUNT);
    int64_t seed = 42;
    for (int32_t index = 0; index < COUNT; index++) {
        seed = seed * 48271 % 2147483647;
        numbers[index] = (int32_t)(seed % 1000000);
    }
    radix_pass(numbers, spare, 0);
    radix_pass(spare, numbers, BITS);
    int64_t checksum = 0;
    int ordered = 1;
    for (int32_t index = 0; index < COUNT; index++) {
        checksum += numbers[index] * (index % 7);
        if (index > 0 && numbers[index - 1] > numbers[index]) {
            ordered = 0;
        }
    }
    int64_t microseconds = microseconds_since(start);
    printf("ordered %s checksum %lld\n", ordered ? "true" : "false", (long long)checksum);
    print_microseconds(microseconds);
    free(numbers);
    free(spare);
    return 0;
}
