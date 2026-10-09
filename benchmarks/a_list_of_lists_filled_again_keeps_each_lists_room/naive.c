/* naive/ written in C the way it reads: each order malloc'ed, the buckets a growable array of pointers to bucket
 * lists, all freed and made again every round, each bucket growing its array from nothing as orders arrive. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Order {
    int32_t customer;
} Order;

typedef struct List {
    void** items;
    int32_t count;
    int32_t capacity;
} List;

static List orders = {NULL, 0, 0};
static List buckets = {NULL, 0, 0};
static int64_t seed = 777;

static void list_append(List* list, void* item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(void*) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static List* list_make(void) {
    List* list = malloc(sizeof(List));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void list_free(List* list) {
    free(list->items);
    free(list);
}

static void group(int32_t round) {
    for (int32_t index = 0; index < buckets.count; index++) list_free(buckets.items[index]);
    buckets.count = 0;
    for (int32_t made = 0; made < 32768; made++) list_append(&buckets, list_make());
    for (int32_t index = 0; index < orders.count; index++) {
        Order* order = orders.items[index];
        list_append(buckets.items[(order->customer + round) % 32768], order);
    }
}

static int32_t largest(void) {
    int32_t most = 0;
    for (int32_t customer = 0; customer < buckets.count; customer++) {
        List* bucket = buckets.items[customer];
        if (bucket->count > most) most = bucket->count;
    }
    return most;
}

int main(void) {
    for (int32_t index = 0; index < 200000; index++) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        Order* order = malloc(sizeof(Order));
        order->customer = (int32_t)(seed % 32768);
        list_append(&orders, order);
    }
    int64_t start = now_nanoseconds();
    int64_t total = 0;
    for (int32_t round = 0; round < 40; round++) {
        group(round);
        total += largest();
    }
    int64_t microseconds = microseconds_since(start);
    printf("total %lld\n", (long long)total);
    print_microseconds(microseconds);
    for (int32_t index = 0; index < buckets.count; index++) list_free(buckets.items[index]);
    free(buckets.items);
    for (int32_t index = 0; index < orders.count; index++) free(orders.items[index]);
    free(orders.items);
    return 0;
}
