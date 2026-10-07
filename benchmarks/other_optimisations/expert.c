/* The same work tuned by hand: each velocity's two numbers written straight into an array sized once, which is
 * what an appended item made in place would do, and both sums in one pass. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Velocity {
    int32_t across;
    int32_t down;
} Velocity;

#define VELOCITY_COUNT 1000

int main(void) {
    Velocity* velocities = malloc(sizeof(Velocity) * VELOCITY_COUNT);
    for (int32_t index = 0; index < VELOCITY_COUNT; index++) {
        velocities[index].across = index % 7;
        velocities[index].down = index % 5;
    }
    int32_t across = 0;
    int32_t down = 0;
    for (int32_t index = 0; index < VELOCITY_COUNT; index++) {
        across += velocities[index].across;
        down += velocities[index].down;
    }
    printf("across %d down %d\n", across, down);
    free(velocities);
    return 0;
}
