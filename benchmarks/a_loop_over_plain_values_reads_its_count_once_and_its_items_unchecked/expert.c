/* The same work tuned by hand: two plain arrays that cannot overlap (restrict), and each round's scaling and its sum
 * in one pass with eight running sums the C compiler keeps in vector registers. Every round still writes all of
 * `into` and adds all of it up: the offset changes with the round, so no round's work can be done once for all. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define ITEM_COUNT 100000

static float scale_and_add(const float* restrict from, float* restrict into, float offset) {
    float sums[8] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    for (int32_t index = 0; index < ITEM_COUNT; index += 8) {
        for (int32_t lane = 0; lane < 8; lane++) {
            float scaled = from[index + lane] * 1.5f + offset;
            into[index + lane] = scaled;
            sums[lane] += scaled;
        }
    }
    return ((sums[0] + sums[1]) + (sums[2] + sums[3])) + ((sums[4] + sums[5]) + (sums[6] + sums[7]));
}

int main(void) {
    float* from = malloc(sizeof(float) * ITEM_COUNT);
    float* into = malloc(sizeof(float) * ITEM_COUNT);
    for (int32_t index = 0; index < ITEM_COUNT; index++) {
        from[index] = (float)(index % 7);
        into[index] = 0.0f;
    }
    int64_t start = now_nanoseconds();
    int64_t total = 0;
    for (int32_t round = 0; round < 2000; round++) {
        float offset = (float)(round % 4) * 0.25f;
        float sum = scale_and_add(from, into, offset);
        total += (int64_t)(sum * 4.0f);
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total in quarters %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(from);
    free(into);
    return 0;
}
