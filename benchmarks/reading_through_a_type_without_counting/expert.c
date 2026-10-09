/* The same work tuned by hand: no row and no shape, the components as four columns of numbers, and each tick one
 * loop over them that the C compiler vectorises. Every tick still moves every entity by that tick's steps. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define ENTITY_COUNT 100000

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t* lefts = calloc(ENTITY_COUNT, sizeof(int32_t));
    int32_t* tops = calloc(ENTITY_COUNT, sizeof(int32_t));
    int32_t* acrosses = malloc(sizeof(int32_t) * ENTITY_COUNT);
    int32_t* downs = malloc(sizeof(int32_t) * ENTITY_COUNT);
    for (int32_t index = 0; index < ENTITY_COUNT; index++) {
        acrosses[index] = index % 13 - 6;
        downs[index] = index % 7 - 3;
    }
    for (int32_t tick = 0; tick < 200; tick++) {
        int32_t steps = tick % 3;
        for (int32_t index = 0; index < ENTITY_COUNT; index++) {
            lefts[index] += acrosses[index] * steps;
            tops[index] += downs[index] * steps;
        }
    }
    int64_t total = 0;
    for (int32_t index = 0; index < ENTITY_COUNT; index++) total += (int64_t)lefts[index] + tops[index];
    int64_t microseconds = microseconds_since(start);
    printf("total %lld\n", (long long)total);
    print_microseconds(microseconds);
    free(lefts);
    free(tops);
    free(acrosses);
    free(downs);
    return 0;
}
