/* The same work written by hand once: the two classes have one layout, so one struct, one list and one function
 * of each kind serve both. */
#include <stdint.h>
#include <stdio.h>

typedef struct Pair {
    int32_t across;
    int32_t down;
} Pair;

static int32_t sum_across(const Pair* pairs, int32_t count) {
    int32_t total = 0;
    for (int32_t index = 0; index < count; index++) total += pairs[index].across;
    return total;
}

static int32_t sum_down(const Pair* pairs, int32_t count) {
    int32_t total = 0;
    for (int32_t index = 0; index < count; index++) total += pairs[index].down;
    return total;
}

static Pair positions[1000];
static Pair velocities[1000];

int main(void) {
    for (int32_t index = 0; index < 1000; index++) {
        positions[index] = (Pair){index, index * 2};
        velocities[index] = (Pair){index % 7, index % 5};
    }
    int32_t across = sum_across(positions, 1000) + sum_across(velocities, 1000);
    int32_t down = sum_down(positions, 1000) + sum_down(velocities, 1000);
    printf("across %d down %d items %d\n", across, down, 2000);
    return 0;
}
