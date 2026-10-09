/* The sort tuned by hand: a radix sort of (time, position) pairs of 8 bytes (three passes of 11 bits, stable), so
 * only a fifth of the bytes move while sorting, then one pass that gathers each order, inline in an array of structs,
 * into sorted order (one random read of one cache line per order instead of one per field), and the walks over the
 * sorted array in order. Sorting the keys and gathering whole records once beats both moving the records every pass
 * (expert_aos.c) and gathering each field from its column on every walk (expert_soa.c). --orders=N sets how many
 * orders. */
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

/* least significant digit first, 11 bits a pass: three stable passes cover the 31 bits of a time */
#define DIGIT_BITS 11
#define BUCKETS (1 << DIGIT_BITS)

typedef struct Keyed {
    uint32_t key;
    int32_t index;
} Keyed;

static Keyed* sorted_positions(const Order* orders, int32_t count) {
    Keyed* from = malloc(sizeof(Keyed) * count);
    Keyed* to = malloc(sizeof(Keyed) * count);
    for (int32_t index = 0; index < count; index++) {
        from[index].key = (uint32_t)orders[index].placed_at;
        from[index].index = index;
    }
    for (int shift = 0; shift < 31; shift += DIGIT_BITS) {
        int32_t starts[BUCKETS] = {0};
        for (int32_t index = 0; index < count; index++) starts[(from[index].key >> shift) & (BUCKETS - 1)]++;
        int32_t position = 0;
        for (int32_t bucket = 0; bucket < BUCKETS; bucket++) {
            int32_t size = starts[bucket];
            starts[bucket] = position;
            position += size;
        }
        for (int32_t index = 0; index < count; index++) to[starts[(from[index].key >> shift) & (BUCKETS - 1)]++] = from[index];
        Keyed* swapped = from;
        from = to;
        to = swapped;
    }
    free(to);
    return from;
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
    Order* orders = malloc(sizeof(Order) * count);
    int64_t seed = 3;
    for (int32_t index = 0; index < count; index++) {
        seed = seed * 48271 % 2147483647;
        Order* order = &orders[index];
        order->id = index;
        order->placed_at = seed;
        order->customer = (int32_t)(seed / 3 % 50000);
        order->product = (int32_t)(seed / 7 % 2000);
        order->quantity = (int32_t)(seed % 9 + 1);
        order->price = (int32_t)(seed / 11 % 500 + 100);
        order->region = (int32_t)(seed % 16);
        order->status = (int32_t)(seed / 13 % 4);
    }
    int64_t made = now_nanoseconds();
    Keyed* positions = sorted_positions(orders, count);
    Order* by_time = malloc(sizeof(Order) * count);
    for (int32_t index = 0; index < count; index++) by_time[index] = orders[positions[index].index];
    int64_t sorted = now_nanoseconds();
    int64_t running = 0;
    for (int32_t index = 0; index < count; index++) running = (running * 31 + by_time[index].quantity * by_time[index].price) % 1000000007;
    int64_t audits = 0;
    for (int32_t index = 0; index < count; index++) {
        Order* order = &by_time[index];
        audits += (int64_t)order->customer + order->product * 3 + order->region * 5 + order->status * 7;
    }
    int64_t finished = now_nanoseconds();
    printf("running %lld audits %lld first %d last %d\n", (long long)running, (long long)audits, by_time[0].id, by_time[count - 1].id);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld sort %lld walk %lld\n", (long long)((made - start) / 1000),
        (long long)((sorted - made) / 1000), (long long)((finished - sorted) / 1000));
    return 0;
}
