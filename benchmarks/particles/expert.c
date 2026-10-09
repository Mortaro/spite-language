/* The same work tuned by hand: the particles as six columns of floats instead of an array of structs, so each tick
 * is one loop the C compiler vectorises, with the bounce a select instead of a branch. The arithmetic is the naive
 * program's, in the same order and at the same precision, so the answer is the same. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define COUNT 100000

static void tick_all(float* restrict position_x, float* restrict position_y, float* restrict position_z,
                     const float* restrict velocity_x, float* restrict velocity_y, const float* restrict velocity_z) {
    for (int32_t index = 0; index < COUNT; index++) {
        float fallen = velocity_y[index] - 9.8 * 0.016;
        float y = position_y[index] + fallen * 0.016;
        position_x[index] = position_x[index] + velocity_x[index] * 0.016;
        position_z[index] = position_z[index] + velocity_z[index] * 0.016;
        int bounced = y < 0.0;
        float bounced_velocity = -fallen * 0.8;
        position_y[index] = bounced ? -y : y;
        velocity_y[index] = bounced ? bounced_velocity : fallen;
    }
}

int main(void) {
    int64_t start = now_nanoseconds();
    float* position_x = malloc(sizeof(float) * COUNT);
    float* position_y = malloc(sizeof(float) * COUNT);
    float* position_z = malloc(sizeof(float) * COUNT);
    float* velocity_x = malloc(sizeof(float) * COUNT);
    float* velocity_y = malloc(sizeof(float) * COUNT);
    float* velocity_z = malloc(sizeof(float) * COUNT);
    for (int32_t index = 0; index < COUNT; index++) {
        position_x[index] = index % 100;
        position_y[index] = index % 37 + 1;
        position_z[index] = index % 53;
        velocity_x[index] = index % 7 - 3;
        velocity_y[index] = index % 11;
        velocity_z[index] = index % 5 - 2;
    }
    for (int32_t tick = 0; tick < 300; tick++) {
        tick_all(position_x, position_y, position_z, velocity_x, velocity_y, velocity_z);
    }
    float height = 0.0;
    float spread = 0.0;
    for (int32_t index = 0; index < COUNT; index++) {
        height = height + position_y[index];
        spread = spread + position_x[index] + position_z[index];
    }
    int64_t microseconds = microseconds_since(start);
    printf("height %lld spread %lld\n", (long long)(int64_t)height, (long long)(int64_t)spread);
    print_microseconds(microseconds);
    free(position_x);
    free(position_y);
    free(position_z);
    free(velocity_x);
    free(velocity_y);
    free(velocity_z);
    return 0;
}
