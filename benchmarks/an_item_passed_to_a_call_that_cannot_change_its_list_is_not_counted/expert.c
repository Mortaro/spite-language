/* The same work tuned by hand: what each pass adds depends only on its track, so the sixteen tracks' two remainders
 * are worked out once into a local table, and the loop adds the table's entries, which the C compiler vectorises. */
#include <stdint.h>
#include <stdio.h>
#include "../clock.h"

int main(void) {
    int32_t added[16];
    for (int32_t index = 0; index < 16; index = index + 1) {
        int32_t seconds = 120 + index;
        added[index] = seconds % 7 + seconds % 5;
    }
    int64_t start = now_nanoseconds();
    int32_t total = 0;
    for (int32_t index = 0; index < 10000000; index = index + 1) total = total + added[index & 15];
    int64_t microseconds = microseconds_since(start);
    printf("total %d\n", total);
    print_microseconds(microseconds);
    return 0;
}
