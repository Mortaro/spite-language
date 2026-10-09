/* The same work tuned by hand: the points as two columns of plain values with no count at all, and each round's
 * kept list a list of positions written branchlessly into one buffer reused across rounds. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define POINT_COUNT 200000

/* each round's kept list is read once, so the C compiler cannot leave the list unwritten */
static volatile int32_t last_kept;

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t* across = malloc(sizeof(int32_t) * POINT_COUNT);
    int32_t* down = malloc(sizeof(int32_t) * POINT_COUNT);
    int32_t* kept = malloc(sizeof(int32_t) * POINT_COUNT);
    for (int32_t index = 0; index < POINT_COUNT; index++) {
        across[index] = index % 1000;
        down[index] = index % 700;
    }
    int64_t near_count = 0;
    for (int32_t round = 0; round < 20; round++) {
        int32_t bound = round * 80;
        int32_t kept_count = 0;
        for (int32_t index = 0; index < POINT_COUNT; index++) {
            kept[kept_count] = index;
            kept_count += across[index] + down[index] > bound;
        }
        near_count += kept_count;
        last_kept = kept[kept_count > 0 ? kept_count - 1 : 0];
    }
    int64_t microseconds = microseconds_since(start);
    printf("near %lld\n", (long long)near_count);
    print_microseconds(microseconds);
    free(across);
    free(down);
    free(kept);
    return 0;
}
