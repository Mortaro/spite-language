/* naive/ written in C the way it reads: a Vector is a growable array of structs held inline, the row literal a
 * struct of two pointers into those arrays, and the shape Moving what a C programmer writes for an interface: the
 * object and the class it is, with a getter per attribute that switches on the class and answers that attribute,
 * or a default object for a class without it. advance reads every attribute through its getter, as the Spite does. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Position {
    int32_t left;
    int32_t top;
} Position;

typedef struct Velocity {
    int32_t across;
    int32_t down;
} Velocity;

typedef struct PositionVector {
    Position* items;
    int32_t count;
    int32_t capacity;
} PositionVector;

typedef struct VelocityVector {
    Velocity* items;
    int32_t count;
    int32_t capacity;
} VelocityVector;

typedef struct MovingRow {
    Position* position;
    Velocity* velocity;
} MovingRow;

typedef enum MovingClass {
    MOVING_ROW,
} MovingClass;

typedef struct Moving {
    MovingClass class;
    void* object;
} Moving;

static Position default_position = {0, 0};
static Velocity default_velocity = {0, 0};

static Position* moving_position(Moving moving) {
    switch (moving.class) {
        case MOVING_ROW: return ((MovingRow*)moving.object)->position;
    }
    return &default_position;
}

static Velocity* moving_velocity(Moving moving) {
    switch (moving.class) {
        case MOVING_ROW: return ((MovingRow*)moving.object)->velocity;
    }
    return &default_velocity;
}

static void advance(Moving moving, int32_t steps) {
    moving_position(moving)->left = moving_position(moving)->left + moving_velocity(moving)->across * steps;
    moving_position(moving)->top = moving_position(moving)->top + moving_velocity(moving)->down * steps;
}

static void position_append(PositionVector* vector, Position item) {
    if (vector->count == vector->capacity) {
        vector->capacity = vector->capacity == 0 ? 4 : vector->capacity * 2;
        vector->items = realloc(vector->items, sizeof(Position) * vector->capacity);
    }
    vector->items[vector->count] = item;
    vector->count = vector->count + 1;
}

static void velocity_append(VelocityVector* vector, Velocity item) {
    if (vector->count == vector->capacity) {
        vector->capacity = vector->capacity == 0 ? 4 : vector->capacity * 2;
        vector->items = realloc(vector->items, sizeof(Velocity) * vector->capacity);
    }
    vector->items[vector->count] = item;
    vector->count = vector->count + 1;
}

static void fill(PositionVector* positions, VelocityVector* velocities, int32_t count) {
    for (int32_t index = 0; index < count; index = index + 1) {
        Position position = {0, 0};
        position_append(positions, position);
        Velocity velocity = {index % 13 - 6, index % 7 - 3};
        velocity_append(velocities, velocity);
    }
}

static void ticks(PositionVector* positions, VelocityVector* velocities) {
    for (int32_t tick = 0; tick < 200; tick = tick + 1) {
        for (int32_t index = 0; index < positions->count; index = index + 1) {
            if (index >= velocities->count) abort();
            MovingRow row = {&positions->items[index], &velocities->items[index]};
            Moving moving = {MOVING_ROW, &row};
            advance(moving, tick % 3);
        }
    }
}

static int64_t sum_place(PositionVector* positions) {
    int64_t total = 0;
    for (int32_t index = 0; index < positions->count; index = index + 1) {
        total = total + ((int64_t)positions->items[index].left + positions->items[index].top);
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    PositionVector positions = {NULL, 0, 0};
    VelocityVector velocities = {NULL, 0, 0};
    fill(&positions, &velocities, 100000);
    ticks(&positions, &velocities);
    int64_t total = sum_place(&positions);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(positions.items);
    free(velocities.items);
    return 0;
}
