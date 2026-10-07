/* naive/ written in C the way it reads: each mover counted, a release that counts down and frees in one function,
 * and the list's clear releasing every mover it holds. One file, as a C programmer writes a program this size. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Mover {
    int32_t ref_count;
    int32_t speed;
} Mover;

typedef struct List {
    Mover** items;
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

static void list_append(List* list, Mover* item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(Mover*) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static Mover* mover_make(int32_t speed) {
    Mover* mover = malloc(sizeof(Mover));
    mover->ref_count = 1;
    mover->speed = speed;
    return mover;
}

static Mover* mover_retain(Mover* mover) {
    mover->ref_count = mover->ref_count + 1;
    return mover;
}

static void mover_release(Mover* mover) {
    mover->ref_count = mover->ref_count - 1;
    if (mover->ref_count == 0) free(mover);
}

static void list_clear(List* list) {
    for (int32_t index = 0; index < list->count; index = index + 1) mover_release(list->items[index]);
    list->count = 0;
}

static void list_free(List* list) {
    list_clear(list);
    free(list->items);
    free(list);
}

static List* make_movers(int32_t count) {
    List* movers = list_make();
    for (int32_t index = 0; index < count; index = index + 1) {
        Mover* mover = mover_make(index);
        list_append(movers, mover);
    }
    return movers;
}

static int32_t keep_every_other(List* movers, List* every_other) {
    int32_t total = 0;
    for (int32_t position = 0; position < movers->count; position = position + 2) {
        Mover* mover = movers->items[position];
        total = total + mover->speed;
        list_append(every_other, mover_retain(mover));
    }
    return total;
}

int main(void) {
    List* movers = make_movers(1000);
    List* every_other = list_make();
    int32_t total = keep_every_other(movers, every_other);
    list_clear(movers);
    int32_t kept = every_other->count;
    printf("%d %d\n", kept, total);
    list_free(every_other);
    list_free(movers);
    return 0;
}
