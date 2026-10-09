/* The same work tuned by hand: the four remainders in an array on the stack, the weights a constant array, and the
 * largest taken with no branch. */
#include <stdint.h>
#include <stdio.h>
#include "../clock.h"

static const int32_t weights[4] = {4, 3, 2, 1};

int main(void) {
    int64_t start = now_nanoseconds();
    int64_t total = 0;
    for (int32_t round = 0; round < 10000000; round++) {
        int32_t remainders[4] = {round % 10, round % 7, round % 13, round % 3};
        int32_t biggest = 0;
        for (int32_t index = 0; index < 4; index++) {
            int32_t weighted = remainders[index] * weights[index];
            biggest = weighted > biggest ? weighted : biggest;
        }
        total += biggest;
    }
    int64_t microseconds = microseconds_since(start);
    printf("total %lld\n", (long long)total);
    print_microseconds(microseconds);
    return 0;
}
