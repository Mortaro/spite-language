/* The same work tuned by hand: each meter's kind decided once, so its loop has no test in it, and both sums of a
 * round taken in one pass over a plain array, kept in registers and vectorised by the C compiler. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define VALUE_COUNT 1000000

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t* values = malloc(sizeof(int32_t) * VALUE_COUNT);
    for (int32_t index = 0; index < VALUE_COUNT; index++) {
        values[index] = index % 1000;
    }
    int64_t sum = 0;
    int64_t squares = 0;
    for (int32_t round = 0; round < 20; round++) {
        __asm__ volatile("" : : "r"(values) : "memory");   /* every round reads the values again, as the program asks */
        int64_t round_sum = 0;
        int64_t round_squares = 0;
        for (int32_t index = 0; index < VALUE_COUNT; index++) {
            int64_t value = values[index];
            round_sum += value;
            round_squares += value * value;
        }
        sum += round_sum;
        squares += round_squares;
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("sum %lld squares %lld\n", (long long)sum, (long long)squares);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(values);
    return 0;
}
