/* naive/ written in C the way it reads: a struct per class, malloc per object, a growable array of pointers for the
 * list, and each item made with its default owner, Owner(0), which the constructor then replaces and frees. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Owner {
    int32_t age;
} Owner;

typedef struct Item {
    int32_t price;
    Owner* owner;
} Item;

typedef struct List {
    void** items;
    int32_t count;
    int32_t capacity;
} List;

static Owner* owner_make(int32_t age) {
    Owner* owner = malloc(sizeof(Owner));
    owner->age = age;
    return owner;
}

static Item* item_make(int32_t price, Owner* owner) {
    Item* item = malloc(sizeof(Item));
    item->price = 0;
    item->owner = owner_make(0);
    item->price = price;
    free(item->owner);
    item->owner = owner;
    return item;
}

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

static List* map_owners(List* items) {
    List* owners = list_make();
    for (int32_t index = 0; index < items->count; index = index + 1) {
        Item* item = items->items[index];
        list_append(owners, item->owner);
    }
    return owners;
}

static int32_t sum_age(List* owners) {
    int32_t total = 0;
    for (int32_t index = 0; index < owners->count; index = index + 1) {
        Owner* owner = owners->items[index];
        total = total + owner->age;
    }
    return total;
}

static int32_t sum_price(List* items) {
    int32_t total = 0;
    for (int32_t index = 0; index < items->count; index = index + 1) {
        Item* item = items->items[index];
        total = total + item->price;
    }
    return total;
}

static int32_t one_round(int32_t round) {
    List* items = list_make();
    for (int32_t index = 0; index < 50000; index = index + 1) {
        Owner* owner = owner_make((index + round) % 90);
        Item* item = item_make(index % 1000, owner);
        list_append(items, item);
    }
    List* owners = map_owners(items);
    int32_t ages = sum_age(owners);
    list_free(owners);
    int32_t sum = sum_price(items) + ages;
    for (int32_t index = 0; index < items->count; index = index + 1) {
        Item* item = items->items[index];
        free(item->owner);
        free(item);
    }
    list_free(items);
    return sum;
}

static int64_t all_rounds(int32_t rounds) {
    int64_t total = 0;
    for (int32_t round = 0; round < rounds; round = round + 1) {
        int32_t sum = one_round(round);
        total = total + sum;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int64_t total = all_rounds(10);
    int64_t microseconds = microseconds_since(start);
    printf("total %lld\n", (long long)total);
    print_microseconds(microseconds);
    return 0;
}
