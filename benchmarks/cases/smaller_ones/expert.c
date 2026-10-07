/* The same work tuned by hand: the counting written into the loop, kept in locals while it runs, and the test made
 * branchless so the C compiler vectorises the pass. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define VALUE_COUNT 1000000

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t* values = malloc(sizeof(int32_t) * VALUE_COUNT);
    for (int32_t index = 0; index < VALUE_COUNT; index++) values[index] = index % 1000;
    int64_t total = 0;
    int32_t large = 0;
    for (int32_t round = 0; round < 50; round++) {
        int64_t round_total = 0;
        int32_t round_large = 0;
        for (int32_t index = 0; index < VALUE_COUNT; index++) {
            round_total += values[index];
            round_large += values[index] > 900;
        }
        total += round_total;
        large += round_large;
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld large %d\n", (long long)total, large);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(values);
    return 0;
}
