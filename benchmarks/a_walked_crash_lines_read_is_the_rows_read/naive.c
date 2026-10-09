/* naive/ written in C the way it reads: each column a sparse set (the dense place of each entity) beside a growable
 * array of components, reached through a function that makes it on first use, as a singleton is; the runner finds
 * each component's place into a list it clears per entity, and fills the row as the template's lines read: for
 * each component, test that its place is in the list and that the column has an item there, then read the place
 * and the item again to fill the row. The row is a struct of pointers, the system reads through it. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

typedef struct IntegerList {
    int32_t* items;
    int32_t count;
    int32_t capacity;
} IntegerList;

typedef struct Entity {
    int32_t id;
} Entity;

typedef struct Position {
    int32_t left;
    int32_t top;
} Position;

typedef struct Velocity {
    int32_t across;
    int32_t down;
} Velocity;

typedef struct SparseSet {
    IntegerList dense_of;
    IntegerList entities;
} SparseSet;

typedef struct PositionColumn {
    SparseSet set;
    Position* values;
    int32_t count;
    int32_t capacity;
} PositionColumn;

typedef struct VelocityColumn {
    SparseSet set;
    Velocity* values;
    int32_t count;
    int32_t capacity;
} VelocityColumn;

typedef struct Moving {
    Entity* entity;
    Position* position;
    Velocity* velocity;
} Moving;

static int32_t marked = 0;

static void list_append(IntegerList* list, int32_t item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(int32_t) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static int32_t* list_at(IntegerList* list, int32_t index) {
    if (index < 0 || index >= list->count) return NULL;
    return &list->items[index];
}

static void set_place(SparseSet* set, int32_t entity) {
    while (set->dense_of.count <= entity) list_append(&set->dense_of, 0);
    list_append(&set->entities, entity);
    set->dense_of.items[entity] = set->entities.count;
}

static int32_t set_dense_index(SparseSet* set, int32_t entity) {
    if (entity >= set->dense_of.count) return -1;
    return set->dense_of.items[entity] - 1;
}

static PositionColumn* position_column_made = NULL;
static VelocityColumn* velocity_column_made = NULL;

static PositionColumn* position_column(void) {
    if (position_column_made == NULL) position_column_made = calloc(1, sizeof(PositionColumn));
    return position_column_made;
}

static VelocityColumn* velocity_column(void) {
    if (velocity_column_made == NULL) velocity_column_made = calloc(1, sizeof(VelocityColumn));
    return velocity_column_made;
}

static Position* position_at(PositionColumn* column, int32_t index) {
    if (index < 0 || index >= column->count) return NULL;
    return &column->values[index];
}

static Velocity* velocity_at(VelocityColumn* column, int32_t index) {
    if (index < 0 || index >= column->count) return NULL;
    return &column->values[index];
}

static void position_insert(int32_t entity, Position value) {
    PositionColumn* column = position_column();
    set_place(&column->set, entity);
    if (column->count == column->capacity) {
        column->capacity = column->capacity == 0 ? 4 : column->capacity * 2;
        column->values = realloc(column->values, sizeof(Position) * column->capacity);
    }
    column->values[column->count] = value;
    column->count = column->count + 1;
}

static void velocity_insert(int32_t entity, Velocity value) {
    VelocityColumn* column = velocity_column();
    set_place(&column->set, entity);
    if (column->count == column->capacity) {
        column->capacity = column->capacity == 0 ? 4 : column->capacity * 2;
        column->values = realloc(column->values, sizeof(Velocity) * column->capacity);
    }
    column->values[column->count] = value;
    column->count = column->count + 1;
}

static void mover_update_each(Moving* moving) {
    moving->position->left = moving->position->left + moving->velocity->across;
    moving->position->top = moving->position->top + moving->velocity->down;
    if (moving->entity->id % 1000 == 0) marked = marked + 1;
}

static IntegerList found = {NULL, 0, 0};
static int32_t missing = 0;

static void note(int32_t dense) {
    if (dense < 0) missing = 1;
    list_append(&found, dense);
}

static void run(int32_t entity_count) {
    for (int32_t entity = 0; entity < entity_count; entity = entity + 1) {
        found.count = 0;
        missing = 0;
        note(entity);
        note(set_dense_index(&position_column()->set, entity));
        note(set_dense_index(&velocity_column()->set, entity));
        if (!missing) {
            Entity made = {entity};
            Moving row;
            row.entity = &made;
            if (list_at(&found, 1) == NULL) abort();
            int32_t position_row = *list_at(&found, 1);
            if (position_at(position_column(), position_row) == NULL) abort();
            row.position = position_at(position_column(), position_row);
            if (list_at(&found, 2) == NULL) abort();
            int32_t velocity_row = *list_at(&found, 2);
            if (velocity_at(velocity_column(), velocity_row) == NULL) abort();
            row.velocity = velocity_at(velocity_column(), velocity_row);
            mover_update_each(&row);
        }
    }
}

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t entity_count = 100000;
    for (int32_t entity = 0; entity < entity_count; entity = entity + 1) {
        Position position = {0, 0};
        position_insert(entity, position);
    }
    for (int32_t backwards = entity_count - 1; backwards >= 0; backwards = backwards - 1) {
        if (backwards % 3 != 0) {
            Velocity velocity = {backwards % 13 - 6, backwards % 7 - 3};
            velocity_insert(backwards, velocity);
        }
    }
    for (int32_t tick = 0; tick < 50; tick = tick + 1) run(entity_count);
    int64_t places = 0;
    PositionColumn* positions = position_column();
    for (int32_t index = 0; index < positions->count; index = index + 1) {
        places = places + ((int64_t)positions->values[index].left + positions->values[index].top);
    }
    int64_t microseconds = microseconds_since(start);
    printf("places %lld marked %d\n", (long long)places, marked);
    print_microseconds(microseconds);
    return 0;
}
