/* naive/ written in C the way it reads: every class is a reference, so each answer of scaled and sum is a new
 * object made with malloc, and the one it replaces is freed. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Offset {
    float x;
    float y;
    float z;
} Offset;

static Offset* offset_make(float x, float y, float z) {
    Offset* offset = malloc(sizeof(Offset));
    offset->x = x;
    offset->y = y;
    offset->z = z;
    return offset;
}

static Offset* offset_sum(Offset* self, Offset* other) {
    return offset_make(self->x + other->x, self->y + other->y, self->z + other->z);
}

static Offset* offset_scaled(Offset* self, float factor) {
    return offset_make(self->x * factor, self->y * factor, self->z * factor);
}

static Offset* simulate(int32_t steps) {
    Offset* position = offset_make(0.0f, 0.0f, 0.0f);
    Offset* velocity = offset_make(1.0f, 0.5f, 0.25f);
    Offset* gravity = offset_make(0.0f, -0.5f, 0.0f);
    for (int32_t step = 0; step < steps; step = step + 1) {
        Offset* pulled = offset_scaled(gravity, (float)0.000001);
        Offset* pulled_velocity = offset_sum(velocity, pulled);
        free(velocity);
        free(pulled);
        velocity = pulled_velocity;
        Offset* moved = offset_scaled(velocity, (float)0.001);
        Offset* moved_position = offset_sum(position, moved);
        free(position);
        free(moved);
        position = moved_position;
    }
    free(velocity);
    free(gravity);
    return position;
}

int main(void) {
    int64_t start = now_nanoseconds();
    Offset* ended = simulate(20000000);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    int64_t across = (int64_t)(ended->x * 1000.0f);
    int64_t up = (int64_t)(ended->y * 1000.0f);
    int64_t ahead = (int64_t)(ended->z * 1000.0f);
    printf("ended at %lld %lld %lld\n", (long long)across, (long long)up, (long long)ahead);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(ended);
    return 0;
}
