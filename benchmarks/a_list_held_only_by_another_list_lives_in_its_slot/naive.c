/* naive/ written in C the way it reads: each order malloc'ed, kept in a growable list of pointers, and the buckets a
 * growable list of pointers to bucket lists, each bucket a list malloc'ed on its own and freed when the buckets are
 * grouped again, so a question reads the bucket's pointer, then the bucket, then its items. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Order {
    int32_t customer;
    int32_t amount;
} Order;

typedef struct List {
    void** items;
    int32_t count;
    int32_t capacity;
} List;

static List orders = {NULL, 0, 0};
static List buckets = {NULL, 0, 0};
static int64_t seed = 12345;

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

static void make_orders(int32_t count) {
    for (int32_t index = 0; index < count; index++) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        Order* order = malloc(sizeof(Order));
        order->customer = (int32_t)(seed % 65536);
        order->amount = (int32_t)(seed % 1000);
        list_append(&orders, order);
    }
}

static void group(void) {
    for (int32_t index = 0; index < buckets.count; index++) list_free(buckets.items[index]);
    buckets.count = 0;
    for (int32_t made = 0; made < 65536; made++) list_append(&buckets, list_make());
    for (int32_t index = 0; index < orders.count; index++) {
        Order* order = orders.items[index];
        list_append(buckets.items[order->customer], order);
    }
}

static int64_t ask(int32_t round) {
    int64_t asked = 0;
    int32_t customer = round;
    for (int32_t query = 0; query < 400000; query++) {
        customer = (customer * 7919 + 13) % 65536;
        List* bucket = buckets.items[customer];
        for (int32_t at = 0; at < bucket->count; at++) asked += ((Order*)bucket->items[at])->amount;
    }
    return asked;
}

int main(void) {
    make_orders(400000);
    int64_t start = now_nanoseconds();
    int64_t total = 0;
    for (int32_t round = 0; round < 10; round++) {
        group();
        total += ask(round);
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
