/* naive/ written in C the way it reads: each velocity made on the heap, copied into a growable array of velocities
 * held inline, and freed, as the Spite program does today; then the sums of both attributes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Velocity {
    int32_t across;
    int32_t down;
} Velocity;

typedef struct VelocityVector {
    Velocity* items;
    int32_t count;
    int32_t capacity;
} VelocityVector;

static VelocityVector* velocity_vector_make(void) {
    VelocityVector* vector = malloc(sizeof(VelocityVector));
    vector->items = NULL;
    vector->count = 0;
    vector->capacity = 0;
    return vector;
}

static void velocity_vector_append(VelocityVector* vector, Velocity* velocity) {
    if (vector->count == vector->capacity) {
        vector->capacity = vector->capacity == 0 ? 4 : vector->capacity * 2;
        vector->items = realloc(vector->items, sizeof(Velocity) * vector->capacity);
    }
    vector->items[vector->count] = *velocity;
    vector->count = vector->count + 1;
}

static Velocity* velocity_make(int32_t across, int32_t down) {
    Velocity* velocity = malloc(sizeof(Velocity));
    velocity->across = across;
    velocity->down = down;
    return velocity;
}

static int32_t sum_across(VelocityVector* vector) {
    int32_t total = 0;
    for (int32_t index = 0; index < vector->count; index = index + 1) total = total + vector->items[index].across;
    return total;
}

static int32_t sum_down(VelocityVector* vector) {
    int32_t total = 0;
    for (int32_t index = 0; index < vector->count; index = index + 1) total = total + vector->items[index].down;
    return total;
}

int main(void) {
    VelocityVector* velocities = velocity_vector_make();
    for (int32_t index = 0; index < 1000; index = index + 1) {
        Velocity* made = velocity_make(index % 7, index % 5);
        velocity_vector_append(velocities, made);
        free(made);
    }
    int32_t across = sum_across(velocities);
    int32_t down = sum_down(velocities);
    printf("across %d down %d\n", across, down);
    free(velocities->items);
    free(velocities);
    return 0;
}
