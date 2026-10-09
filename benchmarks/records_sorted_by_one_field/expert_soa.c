/* The orders as eight columns (the time narrowed to 32 bits: every time is below 2^31), sorted by a radix sort of
 * (time, position) pairs of 8 bytes (three passes of 11 bits, stable), so the orders themselves never move; then the
 * same walks as naive.c read each sorted order's fields from the columns through its position, gathered. It is the
 * structure-of-arrays form of the sort: it moves a fifth of the bytes, then pays a random read per field walked.
 * --orders=N sets how many orders. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

/* least significant digit first, 11 bits a pass: three stable passes cover the 31 bits of a time */
#define DIGIT_BITS 11
#define BUCKETS (1 << DIGIT_BITS)

typedef struct Keyed {
    uint32_t key;
    int32_t index;
} Keyed;

/* sorts (time, position) pairs of 8 bytes: the orders themselves never move */
static Keyed* sorted_positions(const int32_t* times, int32_t count) {
    Keyed* from = malloc(sizeof(Keyed) * count);
    Keyed* to = malloc(sizeof(Keyed) * count);
    for (int32_t index = 0; index < count; index++) {
        from[index].key = (uint32_t)times[index];
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
    int32_t* restrict ids = malloc(sizeof(int32_t) * count);
    int32_t* restrict times = malloc(sizeof(int32_t) * count);
    int32_t* restrict customers = malloc(sizeof(int32_t) * count);
    int32_t* restrict products = malloc(sizeof(int32_t) * count);
    int32_t* restrict quantities = malloc(sizeof(int32_t) * count);
    int32_t* restrict prices = malloc(sizeof(int32_t) * count);
    int32_t* restrict regions = malloc(sizeof(int32_t) * count);
    int32_t* restrict statuses = malloc(sizeof(int32_t) * count);
    int64_t seed = 3;
    for (int32_t index = 0; index < count; index++) {
        seed = seed * 48271 % 2147483647;
        ids[index] = index;
        times[index] = (int32_t)seed;
        customers[index] = (int32_t)(seed / 3 % 50000);
        products[index] = (int32_t)(seed / 7 % 2000);
        quantities[index] = (int32_t)(seed % 9 + 1);
        prices[index] = (int32_t)(seed / 11 % 500 + 100);
        regions[index] = (int32_t)(seed % 16);
        statuses[index] = (int32_t)(seed / 13 % 4);
    }
    int64_t made = now_nanoseconds();
    Keyed* by_time = sorted_positions(times, count);
    int64_t sorted = now_nanoseconds();
    int64_t running = 0;
    for (int32_t index = 0; index < count; index++) {
        int32_t at = by_time[index].index;
        running = (running * 31 + quantities[at] * prices[at]) % 1000000007;
    }
    int64_t audits = 0;
    for (int32_t index = 0; index < count; index++) {
        int32_t at = by_time[index].index;
        audits += (int64_t)customers[at] + products[at] * 3 + regions[at] * 5 + statuses[at] * 7;
    }
    int64_t finished = now_nanoseconds();
    printf("running %lld audits %lld first %d last %d\n", (long long)running, (long long)audits, ids[by_time[0].index], ids[by_time[count - 1].index]);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld sort %lld walk %lld\n", (long long)((made - start) / 1000),
        (long long)((sorted - made) / 1000), (long long)((finished - sorted) / 1000));
    return 0;
}
