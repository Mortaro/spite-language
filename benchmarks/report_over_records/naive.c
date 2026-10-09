/* naive/ written in C the way a C programmer writes it: a struct per sale, one malloc each, a growable array of
 * pointers, and the same loops: the units, 64 passes for the profit of each region in each quarter, the fingerprints.
 * --records=N sets how many sales, as naive/ reads it. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

typedef struct Sale {
    int32_t region;
    int32_t product;
    int32_t quantity;
    int32_t price;
    int32_t cost;
    int32_t discount;
    int32_t day;
    int32_t customer;
} Sale;

typedef struct Sales {
    Sale** items;
    int32_t count;
    int32_t capacity;
} Sales;

static Sale* make_sale(int64_t seed) {
    Sale* sale = malloc(sizeof(Sale));
    sale->region = (int32_t)(seed % 16);
    sale->product = (int32_t)(seed / 16 % 1000);
    sale->quantity = (int32_t)(seed / 16000 % 20 + 1);
    sale->price = (int32_t)(seed / 320000 % 500 + 100);
    sale->cost = sale->price * 3 / 4 - (int32_t)(seed % 37);
    sale->discount = (int32_t)(seed % 101);
    sale->day = (int32_t)(seed / 7 % 365);
    sale->customer = (int32_t)(seed / 2555 % 100000);
    return sale;
}

static void append(Sales* sales, Sale* sale) {
    if (sales->count == sales->capacity) {
        sales->capacity = sales->capacity ? sales->capacity * 2 : 8;
        sales->items = realloc(sales->items, sizeof(Sale*) * sales->capacity);
    }
    sales->items[sales->count] = sale;
    sales->count = sales->count + 1;
}

static int64_t fingerprint(Sale* sale) {
    int64_t total = sale->region;
    total = total + sale->product * 3 + sale->quantity * 5 + sale->price * 7 + sale->cost * 11;
    total = total + sale->discount * 13 + sale->day * 17 + sale->customer * 19;
    return total;
}

static int64_t profit_of(Sales* sales, int32_t region, int32_t quarter) {
    int64_t profit = 0;
    for (int32_t index = 0; index < sales->count; index = index + 1) {
        Sale* sale = sales->items[index];
        if (sale->region == region && sale->day / 92 == quarter) {
            profit = profit + sale->quantity * (sale->price - sale->cost);
        }
    }
    return profit;
}

static int32_t records_of(int argument_count, char** arguments, int32_t otherwise) {
    for (int index = 1; index < argument_count; index++) {
        if (strncmp(arguments[index], "--records=", 10) == 0) return atoi(arguments[index] + 10);
    }
    return otherwise;
}

int main(int argument_count, char** arguments) {
    int32_t records = records_of(argument_count, arguments, 2000000);
    int64_t start = now_nanoseconds();
    Sales sales = {0};
    int64_t seed = 42;
    for (int32_t index = 0; index < records; index = index + 1) {
        seed = seed * 48271 % 2147483647;
        append(&sales, make_sale(seed));
    }
    int64_t made = now_nanoseconds();
    int64_t units = 0;
    for (int32_t index = 0; index < sales.count; index = index + 1) {
        units = units + sales.items[index]->quantity;
    }
    int64_t summed = now_nanoseconds();
    int64_t by_cell = 0;
    for (int32_t cell = 0; cell < 64; cell = cell + 1) {
        by_cell = by_cell + profit_of(&sales, cell / 4, cell % 4) * (cell + 1);
    }
    int64_t profited = now_nanoseconds();
    int64_t fingerprints = 0;
    for (int32_t index = 0; index < sales.count; index = index + 1) {
        fingerprints = fingerprints + fingerprint(sales.items[index]);
    }
    int64_t finished = now_nanoseconds();
    printf("units %lld by region and quarter %lld fingerprints %lld first %d\n", (long long)units, (long long)by_cell,
        (long long)fingerprints, sales.items[0]->customer);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld one %lld five %lld all %lld\n", (long long)((made - start) / 1000),
        (long long)((summed - made) / 1000), (long long)((profited - summed) / 1000), (long long)((finished - profited) / 1000));
    return 0;
}
