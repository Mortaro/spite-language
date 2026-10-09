/* The same work tuned by hand: no tag and no switch, each class's measure written straight into the loop, the two
 * objects held by value in locals. The four sums are added in the order the program adds them, so the answer is
 * the same to the bit, and every step still measures all four values. */
#include <stdint.h>
#include <stdio.h>
#include "../clock.h"

static double measure_all(int32_t steps) {
    double total = 0.0;
    float crate_height = 2.0f;
    float crate_depth = 3.0f;
    for (int32_t step = 0; step < steps; step++) {
        double count = (double)(step % 10);
        total += count + count;
        double share = (double)(float)(step % 4);
        total += share + share;
        float crate_width = (float)(step % 7);
        double volume = (double)(crate_width * crate_height * crate_depth);
        total += volume + volume;
        float radius = (float)(step % 5);
        double across = (double)(radius + radius);
        total += across + across;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    double total = measure_all(20000000);
    int64_t microseconds = microseconds_since(start);
    int64_t whole = (int64_t)total;
    printf("total %lld\n", (long long)whole);
    print_microseconds(microseconds);
    return 0;
}
