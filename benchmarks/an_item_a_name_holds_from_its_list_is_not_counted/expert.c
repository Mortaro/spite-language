/* The same work tuned by hand: the columns as two arrays of plain numbers beside the weights, and a round one loop
 * that adds each step into its hits and writes its weight, which the C compiler vectorises. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define COLUMN_COUNT 100000

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t* hits = malloc(sizeof(int32_t) * COLUMN_COUNT);
    int32_t* steps = malloc(sizeof(int32_t) * COLUMN_COUNT);
    int32_t* weights = malloc(sizeof(int32_t) * COLUMN_COUNT);
    for (int32_t index = 0; index < COLUMN_COUNT; index++) {
        hits[index] = 0;
        steps[index] = index % 3 + 1;
        weights[index] = 0;
    }
    for (int32_t round = 0; round < 100; round++) {
        for (int32_t index = 0; index < COLUMN_COUNT; index++) {
            int32_t hit = hits[index] + steps[index];
            hits[index] = hit;
            weights[index] = hit * 10;
        }
    }
    int32_t hit_total = 0;
    for (int32_t index = 0; index < COLUMN_COUNT; index++) {
        hit_total += hits[index];
    }
    int64_t microseconds = microseconds_since(start);
    printf("hits %d last weight %d\n", hit_total, weights[COLUMN_COUNT - 1]);
    print_microseconds(microseconds);
    free(hits);
    free(steps);
    free(weights);
    return 0;
}
