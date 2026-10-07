/* The same work tuned by hand: the items as columns of plain values (the owner's age beside the price, since each
 * item has its own owner), and both sums of a round in one branchless pass the C compiler vectorises. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define ITEM_COUNT 100000

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t* prices = malloc(sizeof(int32_t) * ITEM_COUNT);
    int32_t* ages = malloc(sizeof(int32_t) * ITEM_COUNT);
    int32_t* active = malloc(sizeof(int32_t) * ITEM_COUNT);
    for (int32_t index = 0; index < ITEM_COUNT; index++) {
        prices[index] = index % 1000;
        ages[index] = index % 90;
        active[index] = index % 3 != 0 ? -1 : 0;
    }
    int64_t total = 0;
    for (int32_t round = 0; round < 100; round++) {
        int32_t price_sum = 0;
        int32_t age_sum = 0;
        for (int32_t index = 0; index < ITEM_COUNT; index++) {
            int32_t adult = -(ages[index] >= 18);
            price_sum += prices[index] & active[index];
            age_sum += ages[index] & active[index] & adult;
        }
        total += (int64_t)price_sum + age_sum;
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(prices);
    free(ages);
    free(active);
    return 0;
}
