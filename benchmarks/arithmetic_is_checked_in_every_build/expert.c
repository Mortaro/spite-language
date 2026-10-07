/* The same work tuned by hand: the values in one array made at its final size, and each round's sum a plain loop
 * of unchecked additions the C compiler vectorises. The expert knows the sums fit (each value is under 1000, and
 * 100 000 of them times 3, plus the round, stay under 2^31), so checks nothing; that knowledge is the whole
 * difference from Spite, which checks every operation instead. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define VALUE_COUNT 100000

static int32_t checksum(const int32_t* values, int32_t round) {
    int32_t total = 0;
    for (int32_t index = 0; index < VALUE_COUNT; index++) {
        total += values[index] * 3 + round;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t* values = malloc(sizeof(int32_t) * VALUE_COUNT);
    for (int32_t index = 0; index < VALUE_COUNT; index++) {
        values[index] = index % 1000;
    }
    int64_t total = 0;
    for (int32_t round = 0; round < 400; round++) {
        total += checksum(values, round);
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(values);
    return 0;
}
