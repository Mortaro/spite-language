/* naive/ written in C the way it reads: a struct per class, malloc per object, and a registry of the live lamps
 * that lamp_make adds to and lamp_free removes from, since the program asks how many lamps are alive. Nothing asks
 * for the doors, so they have none. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Lamp {
    bool is_lit;
} Lamp;

typedef struct Door {
    bool is_open;
} Door;

typedef struct List {
    void** items;
    int32_t count;
    int32_t capacity;
} List;

static List* list_make(void) {
    List* list = malloc(sizeof(List));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void list_append(List* list, void* item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(void*) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static void list_remove(List* list, void* item) {
    for (int32_t index = 0; index < list->count; index = index + 1) {
        if (list->items[index] == item) {
            list->items[index] = list->items[list->count - 1];
            list->count = list->count - 1;
            return;
        }
    }
}

static void list_free(List* list) {
    free(list->items);
    free(list);
}

static List* live_lamps;

static Lamp* lamp_make(bool starts_lit) {
    Lamp* lamp = malloc(sizeof(Lamp));
    lamp->is_lit = starts_lit;
    list_append(live_lamps, lamp);
    return lamp;
}

static void lamp_free(Lamp* lamp) {
    list_remove(live_lamps, lamp);
    free(lamp);
}

static Door* door_make(bool starts_open) {
    Door* door = malloc(sizeof(Door));
    door->is_open = starts_open;
    return door;
}

static List* make_lamps(int32_t count) {
    List* lamps = list_make();
    for (int32_t index = 0; index < count; index = index + 1) {
        Lamp* lamp = lamp_make(index % 3 == 0);
        list_append(lamps, lamp);
    }
    return lamps;
}

static List* make_doors(int32_t count) {
    List* doors = list_make();
    for (int32_t index = 0; index < count; index = index + 1) {
        Door* door = door_make(index % 4 == 0);
        list_append(doors, door);
    }
    return doors;
}

static int32_t count_is_lit(List* lamps) {
    int32_t count = 0;
    for (int32_t index = 0; index < lamps->count; index = index + 1) {
        Lamp* lamp = lamps->items[index];
        if (lamp->is_lit) count = count + 1;
    }
    return count;
}

static int32_t count_is_open(List* doors) {
    int32_t count = 0;
    for (int32_t index = 0; index < doors->count; index = index + 1) {
        Door* door = doors->items[index];
        if (door->is_open) count = count + 1;
    }
    return count;
}

int main(void) {
    live_lamps = list_make();
    List* lamps = make_lamps(1000);
    List* doors = make_doors(1000);
    int32_t lit = count_is_lit(lamps);
    int32_t open = count_is_open(doors);
    int32_t tracked = live_lamps->count;
    printf("lamps lit %d doors open %d lamps alive %d\n", lit, open, tracked);
    for (int32_t index = lamps->count - 1; index >= 0; index = index - 1) {
        lamp_free(lamps->items[index]);
    }
    for (int32_t index = 0; index < doors->count; index = index + 1) {
        free(doors->items[index]);
    }
    list_free(lamps);
    list_free(doors);
    list_free(live_lamps);
    return 0;
}
