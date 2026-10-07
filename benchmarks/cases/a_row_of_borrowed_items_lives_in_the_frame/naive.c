/* naive/ written in C the way it reads: a Vector is a growable array of structs held inline, and each row literal is
 * an object made for the call: a malloc'd struct of two pointers into the arrays, passed to the system and freed
 * after it. The systems read their components through the row. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

typedef struct Position {
    int32_t left;
    int32_t top;
} Position;

typedef struct Velocity {
    int32_t across;
    int32_t down;
} Velocity;

typedef struct Health {
    int32_t amount;
} Health;

typedef struct Regeneration {
    int32_t per_tick;
} Regeneration;

typedef struct Moving {
    Position* position;
    Velocity* velocity;
} Moving;

typedef struct Mending {
    Health* health;
    Regeneration* regeneration;
} Mending;

typedef struct Vector {
    void* items;
    int32_t count;
    int32_t capacity;
} Vector;

static Vector positions;
static Vector velocities;
static Vector healths;
static Vector regenerations;

static void vector_append(Vector* vector, const void* item, size_t size) {
    if (vector->count == vector->capacity) {
        vector->capacity = vector->capacity == 0 ? 4 : vector->capacity * 2;
        vector->items = realloc(vector->items, size * vector->capacity);
    }
    memcpy((char*)vector->items + size * vector->count, item, size);
    vector->count = vector->count + 1;
}

static void mover_update_each(Moving* moving) {
    moving->position->left = moving->position->left + moving->velocity->across;
    moving->position->top = moving->position->top + moving->velocity->down;
}

static void healer_update_each(Mending* mending) {
    mending->health->amount = mending->health->amount + mending->regeneration->per_tick;
}

static void spawn_all(int32_t count) {
    for (int32_t entity = 0; entity < count; entity = entity + 1) {
        Position position = {0, 0};
        vector_append(&positions, &position, sizeof(Position));
        Velocity velocity = {entity % 13 - 6, entity % 7 - 3};
        vector_append(&velocities, &velocity, sizeof(Velocity));
        Health health = {0};
        vector_append(&healths, &health, sizeof(Health));
        Regeneration regeneration = {entity % 4};
        vector_append(&regenerations, &regeneration, sizeof(Regeneration));
    }
}

static void tick_once(void) {
    for (int32_t entity = 0; entity < positions.count; entity = entity + 1) {
        if (entity >= velocities.count || entity >= healths.count || entity >= regenerations.count) abort();
        Moving* moving = malloc(sizeof(Moving));
        moving->position = &((Position*)positions.items)[entity];
        moving->velocity = &((Velocity*)velocities.items)[entity];
        mover_update_each(moving);
        free(moving);
        Mending* mending = malloc(sizeof(Mending));
        mending->health = &((Health*)healths.items)[entity];
        mending->regeneration = &((Regeneration*)regenerations.items)[entity];
        healer_update_each(mending);
        free(mending);
    }
}

static void run_ticks(int32_t count) {
    for (int32_t tick = 0; tick < count; tick = tick + 1) tick_once();
}

int main(void) {
    int64_t start = now_nanoseconds();
    spawn_all(100000);
    run_ticks(100);
    int64_t places = 0;
    for (int32_t index = 0; index < positions.count; index = index + 1) {
        Position* position = &((Position*)positions.items)[index];
        places = places + ((int64_t)position->left + position->top);
    }
    int32_t amounts = 0;
    for (int32_t index = 0; index < healths.count; index = index + 1) amounts = amounts + ((Health*)healths.items)[index].amount;
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("places %lld health %d\n", (long long)places, amounts);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(positions.items);
    free(velocities.items);
    free(healths.items);
    free(regenerations.items);
    return 0;
}
