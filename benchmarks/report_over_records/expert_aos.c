/* The first step an expert takes: the sales held inline in one array of structs instead of one malloc each, the
 * same loops as naive.c (the units, 64 passes for the profit of each region in each quarter, the fingerprints). It shows what
 * the array of objects costs by itself, apart from the columns of expert_soa.c and the single pass of expert.c.
 * --records=N sets how many sales. */
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

static int32_t records_of(int argument_count, char** arguments, int32_t otherwise) {
    for (int index = 1; index < argument_count; index++) {
        if (strncmp(arguments[index], "--records=", 10) == 0) return atoi(arguments[index] + 10);
    }
    return otherwise;
}

int main(int argument_count, char** arguments) {
    int32_t records = records_of(argument_count, arguments, 2000000);
    int64_t start = now_nanoseconds();
    Sale* sales = malloc(sizeof(Sale) * records);
    int64_t seed = 42;
    for (int32_t index = 0; index < records; index++) {
        seed = seed * 48271 % 2147483647;
        Sale* sale = &sales[index];
        sale->region = (int32_t)(seed % 16);
        sale->product = (int32_t)(seed / 16 % 1000);
        sale->quantity = (int32_t)(seed / 16000 % 20 + 1);
        sale->price = (int32_t)(seed / 320000 % 500 + 100);
        sale->cost = sale->price * 3 / 4 - (int32_t)(seed % 37);
        sale->discount = (int32_t)(seed % 101);
        sale->day = (int32_t)(seed / 7 % 365);
        sale->customer = (int32_t)(seed / 2555 % 100000);
    }
    int64_t made = now_nanoseconds();
    int64_t units = 0;
    for (int32_t index = 0; index < records; index++) units += sales[index].quantity;
    int64_t summed = now_nanoseconds();
    int64_t by_cell = 0;
    for (int32_t cell = 0; cell < 64; cell++) {
        int32_t region = cell / 4;
        int32_t quarter = cell % 4;
        int64_t profit = 0;
        for (int32_t index = 0; index < records; index++) {
            Sale* sale = &sales[index];
            if (sale->region == region && sale->day / 92 == quarter) profit += sale->quantity * (sale->price - sale->cost);
        }
        by_cell += profit * (cell + 1);
    }
    int64_t profited = now_nanoseconds();
    int64_t fingerprints = 0;
    for (int32_t index = 0; index < records; index++) {
        Sale* sale = &sales[index];
        fingerprints += (int64_t)sale->region + sale->product * 3 + sale->quantity * 5 + sale->price * 7 + sale->cost * 11
            + sale->discount * 13 + sale->day * 17 + sale->customer * 19;
    }
    int64_t finished = now_nanoseconds();
    printf("units %lld by region and quarter %lld fingerprints %lld first %d\n", (long long)units, (long long)by_cell,
        (long long)fingerprints, sales[0].customer);
    fprintf(stderr, "microseconds %lld\n", (long long)((finished - start) / 1000));
    fprintf(stderr, "phases make %lld one %lld five %lld all %lld\n", (long long)((made - start) / 1000),
        (long long)((summed - made) / 1000), (long long)((profited - summed) / 1000), (long long)((finished - profited) / 1000));
    return 0;
}
