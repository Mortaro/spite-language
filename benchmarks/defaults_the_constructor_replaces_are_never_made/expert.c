/* The same work tuned by hand: no default is made, and since each item has its own owner, the items are two
 * columns of plain values (the price and the owner's age), filled and summed in one pass per round. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define ITEM_COUNT 50000

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t* prices = malloc(sizeof(int32_t) * ITEM_COUNT);
    int32_t* ages = malloc(sizeof(int32_t) * ITEM_COUNT);
    int64_t total = 0;
    for (int32_t round = 0; round < 10; round++) {
        for (int32_t index = 0; index < ITEM_COUNT; index++) {
            prices[index] = index % 1000;
            ages[index] = (index + round) % 90;
        }
        /* keeps the columns written, as the program makes every item before it sums them */
        __asm__ volatile("" : : "r"(prices), "r"(ages) : "memory");
        int32_t sum = 0;
        for (int32_t index = 0; index < ITEM_COUNT; index++) sum += prices[index] + ages[index];
        total += sum;
    }
    int64_t microseconds = microseconds_since(start);
    printf("total %lld\n", (long long)total);
    print_microseconds(microseconds);
    free(prices);
    free(ages);
    return 0;
}
