/* naive/ written in C the way a C programmer writes it from the Spite: a growable array of Velocity structs, one of
 * entity numbers and one of marks, filled by appending, and each removal a function that walks its array once and
 * moves every element that stays down to the next free place: one for the velocities, testing each one's
 * `despawned`, and one for the entities, calling the test it is handed. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define ITEM_TOTAL 200000

typedef struct Velocity {
    int32_t entity;
    float across;
    float down;
    bool despawned;
} Velocity;

typedef struct Velocities {
    Velocity* items;
    int32_t count;
    int32_t capacity;
} Velocities;

typedef struct Integers {
    int32_t* items;
    int32_t count;
    int32_t capacity;
} Integers;

typedef struct Booleans {
    bool* items;
    int32_t count;
    int32_t capacity;
} Booleans;

static Velocities velocities;
static Integers entities;
static Booleans marks;

static void append_velocity(Velocities* list, Velocity velocity) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(Velocity) * (size_t)list->capacity);
    }
    list->items[list->count++] = velocity;
}

static void append_integer(Integers* list, int32_t value) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(int32_t) * (size_t)list->capacity);
    }
    list->items[list->count++] = value;
}

static void append_boolean(Booleans* list, bool value) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(bool) * (size_t)list->capacity);
    }
    list->items[list->count++] = value;
}

static void mark_every(int32_t total, int32_t removed_in_ten) {
    marks.count = 0;
    for (int32_t entity = 0; entity < total; entity++) {
        int32_t scattered = entity * 7 % 10;
        append_boolean(&marks, scattered < removed_in_ten);
    }
}

static bool marked(int32_t entity) {
    if (entity >= marks.count) {
        fprintf(stderr, "no mark for entity %d\n", entity);
        exit(1);
    }
    return marks.items[entity];
}

static void fill(void) {
    velocities.count = 0;
    entities.count = 0;
    for (int32_t entity = 0; entity < ITEM_TOTAL; entity++) {
        bool despawned_now = marked(entity);
        Velocity velocity = {entity, 1.0f * (float)entity, 1.0f * (float)entity * 0.5f, despawned_now};
        append_velocity(&velocities, velocity);
        append_integer(&entities, entity);
    }
}

static void remove_despawned(Velocities* list) {
    int32_t kept = 0;
    for (int32_t index = 0; index < list->count; index++) {
        if (!list->items[index].despawned) {
            list->items[kept] = list->items[index];
            kept++;
        }
    }
    list->count = kept;
}

static void remove_where(Integers* list, bool (*test)(int32_t)) {
    int32_t kept = 0;
    for (int32_t index = 0; index < list->count; index++) {
        if (!test(list->items[index])) {
            list->items[kept] = list->items[index];
            kept++;
        }
    }
    list->count = kept;
}

static int64_t kept_sum(void) {
    int64_t sum = 0;
    for (int32_t row = 0; row < velocities.count; row++) {
        sum = sum + velocities.items[row].entity + entities.items[row];
    }
    return sum;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int64_t checksum = 0;
    for (int32_t round = 0; round < 40; round++) {
        int32_t removed_in_ten = round % 2 == 1 ? 9 : 5;
        mark_every(ITEM_TOTAL, removed_in_ten);
        fill();
        remove_despawned(&velocities);
        remove_where(&entities, marked);
        checksum = checksum + kept_sum();
    }
    int64_t microseconds = microseconds_since(start);
    printf("checksum %lld\n", (long long)checksum);
    print_microseconds(microseconds);
    free(velocities.items);
    free(entities.items);
    free(marks.items);
    return 0;
}
