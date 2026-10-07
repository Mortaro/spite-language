/* The same work tuned by hand: the cosine and sine of half a radian written as the constants they are, and the
 * points as two columns of plain values that each round turns in one loop the C compiler vectorises. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define POINT_COUNT 100000

int main(void) {
    int64_t start = now_nanoseconds();
    float* across = malloc(sizeof(float) * POINT_COUNT);
    float* down = malloc(sizeof(float) * POINT_COUNT);
    for (int32_t index = 0; index < POINT_COUNT; index++) {
        across[index] = (float)(index % 100);
        down[index] = (float)(index % 37);
    }
    const float cosine = 0x1.c15280p-1f;
    const float sine = 0x1.eaee88p-2f;
    for (int32_t round = 0; round < 200; round++) {
        for (int32_t index = 0; index < POINT_COUNT; index++) {
            float turned_across = across[index] * cosine - down[index] * sine;
            float turned_down = across[index] * sine + down[index] * cosine;
            across[index] = turned_across;
            down[index] = turned_down;
        }
    }
    float total = 0.0f;
    for (int32_t index = 0; index < POINT_COUNT; index++) {
        total = total + across[index];
    }
    int32_t whole = (int32_t)roundf(total);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("across %d\n", whole);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(across);
    free(down);
    return 0;
}
