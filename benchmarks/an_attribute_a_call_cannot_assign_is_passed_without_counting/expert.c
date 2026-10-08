/* The same work tuned by hand: the offset and the sixteen readings are loaded once into locals, and the loop reads
 * them in place with a mask for the remainder, which the C compiler turns into vector additions. */
#include <stdint.h>
#include <stdio.h>
#include "../clock.h"

int main(void) {
    int32_t readings[16];
    for (int32_t index = 0; index < 16; index = index + 1) readings[index] = index;
    int32_t offset = 3;
    int64_t start = now_nanoseconds();
    int32_t total = 0;
    for (int32_t index = 0; index < 10000000; index = index + 1) total = total + readings[index & 15] + offset;
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %d\n", total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
