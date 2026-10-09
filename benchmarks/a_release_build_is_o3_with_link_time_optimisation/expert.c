/* The same work tuned by hand: the factor in a register, the values unsigned (the index is never negative, so a
 * mask stands for the remainder by 65536), and the loop left for the C compiler to vectorise. */
#include <stdint.h>
#include <stdio.h>
#include "../clock.h"

int main(void) {
    int64_t start = now_nanoseconds();
    const uint32_t factor = 31;
    uint64_t total = 0;
    for (uint32_t index = 0; index < 50000000u; index++) {
        total += ((index & 65535u) * factor + 7u) % 1000u;
    }
    int64_t microseconds = microseconds_since(start);
    printf("checksum %llu\n", (unsigned long long)total);
    print_microseconds(microseconds);
    return 0;
}
