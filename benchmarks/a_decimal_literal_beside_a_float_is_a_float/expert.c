/* The same work tuned by hand: the values in one array made at its final size, every literal written as a float
 * (1.5f, 0.25f) so nothing is widened to double, and each round's sum kept in eight running totals the C compiler
 * holds in one vector register. Every value and every partial sum here is exact in a float, so the order of the
 * additions does not change the answer. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define VALUE_COUNT 50000

static float scaled_sum(const float* values, float shift) {
    float totals[8] = {0};
    for (int32_t index = 0; index < VALUE_COUNT; index += 8) {
        for (int32_t lane = 0; lane < 8; lane++) {
            totals[lane] += (values[index + lane] + shift) * 1.5f + 0.25f;
        }
    }
    float total = 0.0f;
    for (int32_t lane = 0; lane < 8; lane++) total += totals[lane];
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    float* values = malloc(sizeof(float) * VALUE_COUNT);
    for (int32_t index = 0; index < VALUE_COUNT; index++) {
        values[index] = (float)(index % 64) * 0.25f;
    }
    double total = 0.0;
    for (int32_t round = 0; round < 4000; round++) {
        total += scaled_sum(values, (float)(round % 8));
    }
    int64_t microseconds = microseconds_since(start);
    printf("total %lld\n", (long long)total);
    print_microseconds(microseconds);
    free(values);
    return 0;
}
