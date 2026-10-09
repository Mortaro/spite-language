/* The same work tuned by hand: the point is two numbers, so the copy is two locals in registers, moved and
 * measured with no memory touched. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t origin_x = 3;
    int32_t origin_y = -4;
    int64_t total = 0;
    for (int32_t trial = 0; trial < 20000000; trial++) {
        int32_t x = origin_x + trial % 7 - 3;
        int32_t y = origin_y + trial % 5 - 2;
        total += abs(x) + abs(y);
    }
    int64_t microseconds = microseconds_since(start);
    printf("total %lld origin %d %d\n", (long long)total, origin_x, origin_y);
    print_microseconds(microseconds);
    return 0;
}
