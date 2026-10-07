/* The same work tuned by hand: the values and picks in plain arrays, each pick still checked against the values,
 * with one unsigned comparison marked unlikely and the report in a cold function of its own that is never
 * inlined, so the loop holds a comparison, two loads and an addition. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define VALUE_COUNT 1300
#define PICK_COUNT 100000

static __attribute__((noinline, cold, noreturn)) void report_missing(int32_t pick, int32_t round, int64_t total, int32_t index) {
    fflush(stdout);
    fprintf(stderr, "crash naive.spite:39 in Naive.round_total\tvalues[pick] is missing: index %d, count %d\tround=%d\ttotal=%lld\tindex=%d\n",
        pick, VALUE_COUNT, round, (long long)total, index);
    exit(1);
}

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t* values = malloc(sizeof(int32_t) * VALUE_COUNT);
    int32_t* picks = malloc(sizeof(int32_t) * PICK_COUNT);
    for (int32_t index = 0; index < VALUE_COUNT; index++) values[index] = index * 7 % 1000;
    for (int32_t index = 0; index < PICK_COUNT; index++) picks[index] = index * 13 % 1000;
    int64_t total = 0;
    for (int32_t round = 0; round < 300; round++) {
        int64_t round_sum = 0;
        for (int32_t index = 0; index < PICK_COUNT; index++) {
            int32_t pick = picks[index] + round;
            if (__builtin_expect((uint32_t)pick >= VALUE_COUNT, 0)) report_missing(pick, round, round_sum, index);
            round_sum += values[pick];
        }
        total += round_sum;
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(values);
    free(picks);
    return 0;
}
