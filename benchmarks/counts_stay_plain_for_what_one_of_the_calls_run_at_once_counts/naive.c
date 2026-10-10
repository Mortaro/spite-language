/* naive/ written in C the way it reads: an archive of 2000 letters and a gallery of 2000 paintings, each object
 * malloc'd on its own and counted, behind one interface (a struct of a function pointer); each sort makes a fresh
 * list per round, keeps the objects that match in it (counting each one), and lets the list go, one collection
 * after the other on the one thread the program has. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Item {
    int32_t count;
    int32_t value;
} Item;

typedef struct Collection Collection;
struct Collection {
    void (*sort)(Collection* collection);
    Item** items;
    int32_t item_count;
    int32_t total;
};

static void keep(Item*** kept, int32_t* count, int32_t* room, Item* item) {
    if (*count == *room) {
        *room = *room == 0 ? 16 : *room * 2;
        *kept = realloc(*kept, sizeof(Item*) * *room);
    }
    item->count = item->count + 1;
    (*kept)[*count] = item;
    *count = *count + 1;
}

static void let_go(Item** kept, int32_t count) {
    for (int32_t index = 0; index < count; index = index + 1) kept[index]->count = kept[index]->count - 1;
    free(kept);
}

static void archive_sort(Collection* collection) {
    for (int32_t round = 0; round < 2000; round = round + 1) {
        Item** kept = 0;
        int32_t count = 0;
        int32_t room = 0;
        for (int32_t index = 0; index < collection->item_count; index = index + 1) {
            Item* letter = collection->items[index];
            if (letter->value % 3 == round % 3) keep(&kept, &count, &room, letter);
        }
        collection->total = collection->total + count;
        let_go(kept, count);
    }
}

static void gallery_sort(Collection* collection) {
    for (int32_t round = 0; round < 2000; round = round + 1) {
        Item** hung = 0;
        int32_t count = 0;
        int32_t room = 0;
        for (int32_t index = 0; index < collection->item_count; index = index + 1) {
            Item* painting = collection->items[index];
            if (painting->value % 5 == round % 5) keep(&hung, &count, &room, painting);
        }
        collection->total = collection->total + count;
        let_go(hung, count);
    }
}

static Collection* make_collection(void (*sort)(Collection* collection), int32_t factor) {
    Collection* collection = malloc(sizeof(Collection));
    collection->sort = sort;
    collection->items = malloc(sizeof(Item*) * 2000);
    collection->item_count = 2000;
    collection->total = 0;
    for (int32_t index = 0; index < 2000; index = index + 1) {
        Item* item = malloc(sizeof(Item));
        item->count = 1;
        item->value = index * factor % 1000;
        collection->items[index] = item;
    }
    return collection;
}

int main(void) {
    Collection* collections[2];
    collections[0] = make_collection(archive_sort, 7);
    collections[1] = make_collection(gallery_sort, 11);
    int64_t start = now_nanoseconds();
    for (int index = 0; index < 2; index = index + 1) collections[index]->sort(collections[index]);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("%d %d\n", collections[0]->total, collections[1]->total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    for (int index = 0; index < 2; index = index + 1) {
        for (int32_t item = 0; item < 2000; item = item + 1) free(collections[index]->items[item]);
        free(collections[index]->items);
        free(collections[index]);
    }
    return 0;
}
