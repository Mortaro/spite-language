/* The same work tuned by hand: whoever wrote the pipeline knows it is discount, tax, discount, so the price is those
 * three steps written out, with no list and no tests. */
#include <stdint.h>
#include <stdio.h>
#include "../clock.h"

static int32_t price(int32_t amount) {
    int32_t total = amount - amount / 10;
    total = total + total / 5;
    return total - total / 10;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int64_t total = 0;
    for (int32_t index = 0; index < 10000000; index = index + 1) {
        total = total + price(1000 + index % 1000);
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
