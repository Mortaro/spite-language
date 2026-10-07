/* The same work tuned by hand: the positions and the speeds as two arrays of numbers on the stack, twenty rounds
 * of one pass that adds each speed to its position, and the sum at the end. */
#include <stdint.h>
#include <stdio.h>

#define PARTICLE_COUNT 1000

int main(void) {
    int32_t position[PARTICLE_COUNT];
    int32_t speed[PARTICLE_COUNT];
    for (int32_t index = 0; index < PARTICLE_COUNT; index++) {
        position[index] = index % 10;
        speed[index] = index % 3;
    }
    for (int32_t round = 0; round < 20; round++) {
        for (int32_t index = 0; index < PARTICLE_COUNT; index++) {
            position[index] += speed[index];
        }
    }
    int32_t total = 0;
    for (int32_t index = 0; index < PARTICLE_COUNT; index++) total += position[index];
    printf("position %d\n", total);
    return 0;
}
