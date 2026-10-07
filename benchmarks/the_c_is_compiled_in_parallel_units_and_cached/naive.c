/* naive/ written in C the way it reads: a struct per item, a growable array of pointers, and the sum of each
 * item's worth. A C program this size is one file, compiled whole by every build; how it is split and cached is
 * its build system's business, which for a Spite program is the compiler's. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Item {
    int32_t price;
    int32_t count;
} Item;

typedef struct ItemList {
    Item** items;
    int32_t count;
    int32_t capacity;
} ItemList;

static ItemList* item_list_make(void) {
    ItemList* list = malloc(sizeof(ItemList));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void item_list_append(ItemList* list, Item* item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(Item*) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static Item* item_make(int32_t price, int32_t count) {
    Item* item = malloc(sizeof(Item));
    item->price = price;
    item->count = count;
    return item;
}

static int32_t item_worth(Item* item) {
    return item->price * item->count;
}

static int32_t sum_worth(ItemList* items) {
    int32_t total = 0;
    for (int32_t index = 0; index < items->count; index = index + 1) {
        total = total + item_worth(items->items[index]);
    }
    return total;
}

int main(void) {
    ItemList* items = item_list_make();
    for (int32_t index = 0; index < 500; index = index + 1) {
        Item* item = item_make(index % 13, index % 4);
        item_list_append(items, item);
    }
    int32_t worth = sum_worth(items);
    printf("worth %d\n", worth);
    for (int32_t index = 0; index < items->count; index = index + 1) {
        free(items->items[index]);
    }
    free(items->items);
    free(items);
    return 0;
}
