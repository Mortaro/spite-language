/* The same work tuned by hand: the amounts in one array made at its final size, every amount known to be zero or
 * more, so each round divides unsigned (cheaper than signed), and one division gives both the quotient and the
 * remainder. Every round still divides every amount by that round's divisor. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define AMOUNT_COUNT 1000000

static int64_t shares(const uint32_t* amounts, uint32_t parts) {
    int64_t total = 0;
    for (int32_t index = 0; index < AMOUNT_COUNT; index++) {
        uint32_t amount = amounts[index];
        uint32_t quotient = amount / parts;
        uint32_t remainder = amount - quotient * parts;
        total += (int64_t)quotient + remainder;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    uint32_t* amounts = malloc(sizeof(uint32_t) * AMOUNT_COUNT);
    for (int32_t index = 0; index < AMOUNT_COUNT; index++) {
        amounts[index] = (uint32_t)(index % 9973);
    }
    int64_t total = 0;
    for (int32_t round = 0; round < 20; round++) {
        total += shares(amounts, (uint32_t)(round % 7 + 1));
    }
    int64_t microseconds = microseconds_since(start);
    printf("total %lld\n", (long long)total);
    print_microseconds(microseconds);
    free(amounts);
    return 0;
}
