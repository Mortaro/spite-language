/* naive/ written in C the way it reads: two classes, so two structs, and every function written once for each,
 * the lists of each and their sums included, though both are the same code over the same layout. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Position {
    int32_t across;
    int32_t down;
} Position;

typedef struct Velocity {
    int32_t across;
    int32_t down;
} Velocity;

typedef struct PositionList {
    Position** items;
    int32_t count;
    int32_t capacity;
} PositionList;

typedef struct VelocityList {
    Velocity** items;
    int32_t count;
    int32_t capacity;
} VelocityList;

static Position* position_make(int32_t across, int32_t down) {
    Position* position = malloc(sizeof(Position));
    position->across = across;
    position->down = down;
    return position;
}

static Velocity* velocity_make(int32_t across, int32_t down) {
    Velocity* velocity = malloc(sizeof(Velocity));
    velocity->across = across;
    velocity->down = down;
    return velocity;
}

static void position_list_append(PositionList* list, Position* item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(Position*) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static void velocity_list_append(VelocityList* list, Velocity* item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(Velocity*) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static int32_t position_list_sum_across(PositionList* list) {
    int32_t total = 0;
    for (int32_t index = 0; index < list->count; index = index + 1) total = total + list->items[index]->across;
    return total;
}

static int32_t velocity_list_sum_across(VelocityList* list) {
    int32_t total = 0;
    for (int32_t index = 0; index < list->count; index = index + 1) total = total + list->items[index]->across;
    return total;
}

static int32_t position_list_sum_down(PositionList* list) {
    int32_t total = 0;
    for (int32_t index = 0; index < list->count; index = index + 1) total = total + list->items[index]->down;
    return total;
}

static int32_t velocity_list_sum_down(VelocityList* list) {
    int32_t total = 0;
    for (int32_t index = 0; index < list->count; index = index + 1) total = total + list->items[index]->down;
    return total;
}

int main(void) {
    PositionList* positions = calloc(1, sizeof(PositionList));
    VelocityList* velocities = calloc(1, sizeof(VelocityList));
    for (int32_t index = 0; index < 1000; index = index + 1) {
        position_list_append(positions, position_make(index, index * 2));
        velocity_list_append(velocities, velocity_make(index % 7, index % 5));
    }
    int32_t across = position_list_sum_across(positions) + velocity_list_sum_across(velocities);
    int32_t down = position_list_sum_down(positions) + velocity_list_sum_down(velocities);
    int32_t items = positions->count + velocities->count;
    printf("across %d down %d items %d\n", across, down, items);
    for (int32_t index = 0; index < positions->count; index = index + 1) free(positions->items[index]);
    for (int32_t index = 0; index < velocities->count; index = index + 1) free(velocities->items[index]);
    free(positions->items);
    free(velocities->items);
    free(positions);
    free(velocities);
    return 0;
}
