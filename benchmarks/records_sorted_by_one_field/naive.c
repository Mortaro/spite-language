/* naive/ written in C the way a C programmer writes it from the Spite: a struct per order, one malloc each, a
 * growable array of pointers, the library's qsort over a copy of that array by the time each order was placed, then
 * the running total and the audit sum over the sorted orders. Every time is distinct, so any correct sort gives the
 * same order. --orders=N sets how many orders. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

typedef struct Order {
    int32_t id;
    int64_t placed_at;
    int32_t customer;
    int32_t product;
    int32_t quantity;
    int32_t price;
    int32_t region;
    int32_t status;
} Order;

typedef struct Orders {
    Order** items;
    int32_t count;
    int32_t capacity;
} Orders;

static void append(Orders* orders, Order* order) {
    if (orders->count == orders->capacity) {
        orders->capacity = orders->capacity ? orders->capacity * 2 : 8;
        orders->items = realloc(orders->items, sizeof(Order*) * orders->capacity);
    }
    orders->items[orders->count] = order;
    orders->count = orders->count + 1;
}

static Order* make_order(int32_t id, int64_t seed) {
    Order* order = malloc(sizeof(Order));
    order->id = id;
    order->placed_at = seed;
    order->customer = (int32_t)(seed / 3 % 50000);
    order->product = (int32_t)(seed / 7 % 2000);
    order->quantity = (int32_t)(seed % 9 + 1);
    order->price = (int32_t)(seed / 11 % 500 + 100);
    order->region = (int32_t)(seed % 16);
    order->status = (int32_t)(seed / 13 % 4);
    return order;
}

static int by_placed_at(const void* left, const void* right) {
    int64_t left_time = (*(Order* const*)left)->placed_at;
    int64_t right_time = (*(Order* const*)right)->placed_at;
    return left_time < right_time ? -1 : left_time > right_time ? 1 : 0;
}

static int64_t audit(Order* order) {
    int64_t total = order->customer;
    return total + order->product * 3 + order->region * 5 + order->status * 7;
}

static int32_t setting_of(int argument_count, char** arguments, const char* flag, int32_t otherwise) {
    size_t flag_length = strlen(flag);
    for (int index = 1; index < argument_count; index++) {
        if (strncmp(arguments[index], flag, flag_length) == 0) return atoi(arguments[index] + flag_length);
    }
    return otherwise;
}

int main(int argument_count, char** arguments) {
    int32_t count = setting_of(argument_count, arguments, "--orders=", 2000000);
    int64_t start = now_nanoseconds();
    Orders orders = {0};
    int64_t seed = 3;
    for (int32_t index = 0; index < count; index = index + 1) {
        seed = seed * 48271 % 2147483647;
        append(&orders, make_order(index, seed));
    }
    int64_t made = now_nanoseconds();
    Orders by_time = {0};
    by_time.count = orders.count;
    by_time.capacity = orders.count;
    by_time.items = malloc(sizeof(Order*) * orders.count);
    memcpy(by_time.items, orders.items, sizeof(Order*) * orders.count);
    qsort(by_time.items, by_time.count, sizeof(Order*), by_placed_at);
    int64_t sorted = now_nanoseconds();
    int64_t running = 0;
    for (int32_t index = 0; index < by_time.count; index = index + 1) {
        Order* order = by_time.items[index];
        running = (running * 31 + order->quantity * order->price) % 1000000007;
    }
    int64_t audits = 0;
    for (int32_t index = 0; index < by_time.count; index = index + 1) audits = audits + audit(by_time.items[index]);
    int64_t finished = now_nanoseconds();
    printf("running %lld audits %lld first %d last %d\n", (long long)running, (long long)audits, by_time.items[0]->id,
        by_time.items[by_time.count - 1]->id);
    fprintf(stderr, "microseconds %lld\n", (long long)((finished - start) / 1000));
    fprintf(stderr, "phases make %lld sort %lld walk %lld\n", (long long)((made - start) / 1000),
        (long long)((sorted - made) / 1000), (long long)((finished - sorted) / 1000));
    return 0;
}
