/* The same work tuned by hand: the bodies as two columns of plain numbers and the labels as a third, and each
 * round's advance and sum in one loop over the columns, which the C compiler vectorises. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define BODY_COUNT 200000

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t* positions = malloc(sizeof(int32_t) * BODY_COUNT);
    int32_t* speeds = malloc(sizeof(int32_t) * BODY_COUNT);
    int32_t* numbers = malloc(sizeof(int32_t) * BODY_COUNT);
    for (int32_t index = 0; index < BODY_COUNT; index++) {
        positions[index] = 0;
        speeds[index] = index % 5 + 1;
        numbers[index] = index % 10;
    }
    int64_t total = 0;
    for (int32_t round = 0; round < 100; round++) {
        int32_t sum = 0;
        for (int32_t index = 0; index < BODY_COUNT; index++) {
            int32_t position = positions[index] + speeds[index];
            positions[index] = position;
            sum += position;
        }
        total += sum;
    }
    int32_t number_total = 0;
    for (int32_t index = 0; index < BODY_COUNT; index++) {
        number_total += numbers[index];
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld labels %d\n", (long long)total, number_total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(positions);
    free(speeds);
    free(numbers);
    return 0;
}
