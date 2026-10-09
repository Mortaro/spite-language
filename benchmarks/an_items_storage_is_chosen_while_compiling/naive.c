/* naive/ written in C the way it reads: Column is generic, so it is written once, as a C programmer writes a generic
 * container: a growable array of void pointers, each component malloc'd and stored by its pointer, whatever its
 * class. A Trail's list of marks is its own malloc'd list, as the Spite makes one. The templates are a loop each. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct IntegerList {
    int32_t* items;
    int32_t count;
    int32_t capacity;
} IntegerList;

typedef struct Velocity {
    int32_t across;
    int32_t down;
} Velocity;

typedef struct Trail {
    int32_t across;
    int32_t down;
    IntegerList* marks;
} Trail;

typedef struct Column {
    void** items;
    int32_t count;
    int32_t capacity;
} Column;

static void column_add(Column* column, void* component) {
    if (column->count == column->capacity) {
        column->capacity = column->capacity == 0 ? 4 : column->capacity * 2;
        column->items = realloc(column->items, sizeof(void*) * column->capacity);
    }
    column->items[column->count] = component;
    column->count = column->count + 1;
}

static Velocity* velocity_make(int32_t across, int32_t down) {
    Velocity* velocity = malloc(sizeof(Velocity));
    velocity->across = across;
    velocity->down = down;
    return velocity;
}

static Trail* trail_make(int32_t across, int32_t down) {
    Trail* trail = malloc(sizeof(Trail));
    trail->across = across;
    trail->down = down;
    trail->marks = calloc(1, sizeof(IntegerList));
    return trail;
}

static void velocity_integrate(Velocity* velocity) {
    velocity->across = velocity->across + velocity->down;
}

static void trail_integrate(Trail* trail) {
    trail->across = trail->across + trail->down;
}

static void fill(Column* velocities, Column* trails, int32_t count) {
    for (int32_t index = 0; index < count; index = index + 1) {
        column_add(velocities, velocity_make(index % 100, index % 7));
        column_add(trails, trail_make(index % 50, index % 5));
    }
}

static int64_t ticks(Column* velocities, Column* trails) {
    int64_t total = 0;
    for (int32_t tick = 0; tick < 100; tick = tick + 1) {
        for (int32_t index = 0; index < velocities->count; index = index + 1) velocity_integrate(velocities->items[index]);
        for (int32_t index = 0; index < trails->count; index = index + 1) trail_integrate(trails->items[index]);
        int32_t moved = 0;
        for (int32_t index = 0; index < velocities->count; index = index + 1) moved = moved + ((Velocity*)velocities->items[index])->across;
        int32_t trailed = 0;
        for (int32_t index = 0; index < trails->count; index = index + 1) trailed = trailed + ((Trail*)trails->items[index])->across;
        total = total + moved + trailed;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    Column velocities = {NULL, 0, 0};
    Column trails = {NULL, 0, 0};
    fill(&velocities, &trails, 100000);
    int64_t total = ticks(&velocities, &trails);
    int64_t microseconds = microseconds_since(start);
    printf("total %lld\n", (long long)total);
    print_microseconds(microseconds);
    for (int32_t index = 0; index < velocities.count; index = index + 1) free(velocities.items[index]);
    for (int32_t index = 0; index < trails.count; index = index + 1) {
        Trail* trail = trails.items[index];
        free(trail->marks->items);
        free(trail->marks);
        free(trail);
    }
    free(velocities.items);
    free(trails.items);
    return 0;
}
