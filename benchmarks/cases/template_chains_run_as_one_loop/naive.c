/* naive/ written in C the way it reads: each step of a chain makes its list, and the next step walks it. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Owner {
    int32_t age;
} Owner;

typedef struct Item {
    int32_t price;
    bool active;
    Owner* owner;
} Item;

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

static void list_free(List* list) {
    free(list->items);
    free(list);
}

static Owner* owner_make(int32_t age) {
    Owner* owner = malloc(sizeof(Owner));
    owner->age = age;
    return owner;
}

static Item* item_make(int32_t price, bool active, Owner* owner) {
    Item* item = malloc(sizeof(Item));
    item->price = price;
    item->active = active;
    item->owner = owner;
    return item;
}

static bool item_is_active(Item* item) {
    return item->active;
}

static bool owner_is_adult(Owner* owner) {
    return owner->age >= 18;
}

static List* filter_is_active(List* items) {
    List* kept = list_make();
    for (int32_t index = 0; index < items->count; index = index + 1) {
        if (item_is_active(items->items[index])) list_append(kept, items->items[index]);
    }
    return kept;
}

static List* map_owners(List* items) {
    List* owners = list_make();
    for (int32_t index = 0; index < items->count; index = index + 1) {
        Item* item = items->items[index];
        list_append(owners, item->owner);
    }
    return owners;
}

static List* filter_is_adult(List* owners) {
    List* kept = list_make();
    for (int32_t index = 0; index < owners->count; index = index + 1) {
        if (owner_is_adult(owners->items[index])) list_append(kept, owners->items[index]);
    }
    return kept;
}

static int32_t sum_price(List* items) {
    int32_t total = 0;
    for (int32_t index = 0; index < items->count; index = index + 1) {
        Item* item = items->items[index];
        total = total + item->price;
    }
    return total;
}

static int32_t sum_age(List* owners) {
    int32_t total = 0;
    for (int32_t index = 0; index < owners->count; index = index + 1) {
        Owner* owner = owners->items[index];
        total = total + owner->age;
    }
    return total;
}

static int64_t totals(List* items) {
    int64_t total = 0;
    for (int32_t round = 0; round < 100; round = round + 1) {
        List* active = filter_is_active(items);
        int32_t prices = sum_price(active);
        list_free(active);
        List* active_again = filter_is_active(items);
        List* owners = map_owners(active_again);
        List* adults = filter_is_adult(owners);
        int32_t ages = sum_age(adults);
        list_free(active_again);
        list_free(owners);
        list_free(adults);
        total = total + prices + ages;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    List* items = list_make();
    for (int32_t index = 0; index < 100000; index = index + 1) {
        Owner* owner = owner_make(index % 90);
        Item* item = item_make(index % 1000, index % 3 != 0, owner);
        list_append(items, item);
    }
    int64_t total = totals(items);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    for (int32_t index = 0; index < items->count; index = index + 1) {
        Item* item = items->items[index];
        free(item->owner);
        free(item);
    }
    list_free(items);
    return 0;
}
