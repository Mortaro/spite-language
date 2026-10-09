/* The same work tuned by hand: the four values passed in an array on the caller's stack, with their count, and the
 * largest taken with no branch. */
#include <stdint.h>
#include <stdio.h>
#include "../clock.h"

static int32_t largest(const int32_t* values, int32_t count) {
    int32_t biggest = 0;
    for (int32_t index = 0; index < count; index++) biggest = values[index] > biggest ? values[index] : biggest;
    return biggest;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int64_t total = 0;
    for (int32_t round = 0; round < 2000000; round++) {
        int32_t values[4] = {round % 10, round % 7, round % 13, round % 3};
        total += largest(values, 4);
    }
    int64_t microseconds = microseconds_since(start);
    printf("total %lld\n", (long long)total);
    print_microseconds(microseconds);
    return 0;
}
