/* The same work tuned by hand: the sixteen slots are weights in a local array, since only the weights are ever read,
 * and the loop compares and stores weights with no pointer chasing and no calls. */
#include <stdint.h>
#include <stdio.h>
#include "../clock.h"

int main(void) {
    int32_t slots[16];
    for (int32_t index = 0; index < 16; index = index + 1) slots[index] = 2;
    int64_t start = now_nanoseconds();
    int32_t changed = 0;
    for (int32_t index = 0; index < 10000000; index = index + 1) {
        int32_t at = index & 15;
        int32_t weight = (index & 63) == 0 ? 9 : 2;
        changed = changed + (slots[at] != weight);
        slots[at] = weight;
    }
    int32_t total = 0;
    for (int32_t index = 0; index < 16; index = index + 1) total = total + slots[index];
    int64_t microseconds = microseconds_since(start);
    printf("changed %d total %d\n", changed, total);
    print_microseconds(microseconds);
    return 0;
}
