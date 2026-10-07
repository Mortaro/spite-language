/* The same work tuned by hand: the movers' speeds as one array of numbers with no counts, the kept ones copied into a
 * second array, and the first emptied, so letting go of a mover costs nothing at all. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MOVER_COUNT 1000

int main(void) {
    int32_t* speeds = malloc(sizeof(int32_t) * MOVER_COUNT);
    int32_t* every_other = malloc(sizeof(int32_t) * (MOVER_COUNT / 2 + 1));
    int32_t mover_count = 0;
    for (int32_t index = 0; index < MOVER_COUNT; index++) speeds[mover_count++] = index;
    int32_t total = 0;
    int32_t kept = 0;
    for (int32_t position = 0; position < mover_count; position += 2) {
        total += speeds[position];
        every_other[kept++] = speeds[position];
    }
    mover_count = 0;
    printf("%d %d\n", kept, total);
    free(speeds);
    free(every_other);
    return 0;
}
