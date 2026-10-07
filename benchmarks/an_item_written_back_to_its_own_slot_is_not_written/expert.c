/* The same work tuned by hand: the particles as two columns of plain numbers, so a pass is one loop that adds
 * one column into the other, which the C compiler vectorises. There is no slot to write back. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define PARTICLE_COUNT 200000

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t* left = malloc(sizeof(int32_t) * PARTICLE_COUNT);
    int32_t* speed = malloc(sizeof(int32_t) * PARTICLE_COUNT);
    for (int32_t index = 0; index < PARTICLE_COUNT; index++) {
        left[index] = 0;
        speed[index] = index % 7 + 1;
    }
    for (int32_t tick = 0; tick < 200; tick++) {
        for (int32_t index = 0; index < PARTICLE_COUNT; index++) {
            left[index] += speed[index];
        }
    }
    int32_t total = 0;
    for (int32_t index = 0; index < PARTICLE_COUNT; index++) {
        total += left[index];
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %d\n", total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(left);
    free(speed);
    return 0;
}
