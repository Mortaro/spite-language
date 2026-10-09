/* The same work tuned by hand: one plain array, no checks, the window's three reads and the rise test written so the
 * C compiler vectorises both loops (the rise counted branchlessly, the first item compared with 0 as the program
 * does). Each round still walks every window and every item: the weight and the threshold change with the round. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define ITEM_COUNT 100000

static int32_t window_total(const int32_t* restrict values, int32_t weight) {
    int32_t total = 0;
    for (int32_t at = 0; at + 2 < ITEM_COUNT; at++) {
        total += values[at] * weight + values[at + 1] + values[at + 2];
    }
    return total;
}

static int32_t rise_count(const int32_t* restrict values, int32_t threshold) {
    int32_t rises = values[0] - 0 > threshold;
    for (int32_t index = 1; index < ITEM_COUNT; index++) {
        rises += values[index] - values[index - 1] > threshold;
    }
    return rises;
}

int main(void) {
    int32_t* values = malloc(sizeof(int32_t) * ITEM_COUNT);
    for (int32_t index = 0; index < ITEM_COUNT; index++) {
        values[index] = index * 37 % 101;
    }
    int64_t start = now_nanoseconds();
    int64_t total = 0;
    for (int32_t round = 0; round < 300; round++) {
        int32_t windows = window_total(values, round % 3 + 1);
        int32_t rises = rise_count(values, round % 50);
        total += (int64_t)windows + rises;
    }
    int64_t microseconds = microseconds_since(start);
    printf("total %lld\n", (long long)total);
    print_microseconds(microseconds);
    free(values);
    return 0;
}
