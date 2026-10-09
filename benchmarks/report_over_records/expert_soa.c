/* The second step: the sales as eight columns of 32-bit numbers (a structure of arrays), with the same loops as
 * naive.c (the units, 64 passes for the profit of each region in each quarter, the fingerprints), each a plain loop over the
 * columns it reads that the C compiler vectorises. It shows what the columns buy by themselves, apart from the
 * single fused pass and the narrowed columns of expert.c. --records=N sets how many sales. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

static int32_t records_of(int argument_count, char** arguments, int32_t otherwise) {
    for (int index = 1; index < argument_count; index++) {
        if (strncmp(arguments[index], "--records=", 10) == 0) return atoi(arguments[index] + 10);
    }
    return otherwise;
}

int main(int argument_count, char** arguments) {
    int32_t records = records_of(argument_count, arguments, 2000000);
    int64_t start = now_nanoseconds();
    int32_t* restrict region = malloc(sizeof(int32_t) * records);
    int32_t* restrict product = malloc(sizeof(int32_t) * records);
    int32_t* restrict quantity = malloc(sizeof(int32_t) * records);
    int32_t* restrict price = malloc(sizeof(int32_t) * records);
    int32_t* restrict cost = malloc(sizeof(int32_t) * records);
    int32_t* restrict discount = malloc(sizeof(int32_t) * records);
    int32_t* restrict day = malloc(sizeof(int32_t) * records);
    int32_t* restrict customer = malloc(sizeof(int32_t) * records);
    int64_t seed = 42;
    for (int32_t index = 0; index < records; index++) {
        seed = seed * 48271 % 2147483647;
        region[index] = (int32_t)(seed % 16);
        product[index] = (int32_t)(seed / 16 % 1000);
        quantity[index] = (int32_t)(seed / 16000 % 20 + 1);
        price[index] = (int32_t)(seed / 320000 % 500 + 100);
        cost[index] = price[index] * 3 / 4 - (int32_t)(seed % 37);
        discount[index] = (int32_t)(seed % 101);
        day[index] = (int32_t)(seed / 7 % 365);
        customer[index] = (int32_t)(seed / 2555 % 100000);
    }
    int64_t made = now_nanoseconds();
    int64_t units = 0;
    for (int32_t index = 0; index < records; index++) units += quantity[index];
    int64_t summed = now_nanoseconds();
    int64_t by_cell = 0;
    for (int32_t cell = 0; cell < 64; cell++) {
        int32_t wanted_region = cell / 4;
        int32_t wanted_quarter = cell % 4;
        int64_t profit = 0;
        for (int32_t index = 0; index < records; index++) {
            int32_t margin = quantity[index] * (price[index] - cost[index]);
            profit += (region[index] == wanted_region) & (day[index] / 92 == wanted_quarter) ? margin : 0;
        }
        by_cell += profit * (cell + 1);
    }
    int64_t profited = now_nanoseconds();
    int64_t fingerprints = 0;
    for (int32_t index = 0; index < records; index++) {
        fingerprints += (int64_t)region[index] + product[index] * 3 + quantity[index] * 5 + price[index] * 7
            + cost[index] * 11 + discount[index] * 13 + day[index] * 17 + customer[index] * 19;
    }
    int64_t finished = now_nanoseconds();
    printf("units %lld by region and quarter %lld fingerprints %lld first %d\n", (long long)units, (long long)by_cell,
        (long long)fingerprints, customer[0]);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld one %lld five %lld all %lld\n", (long long)((made - start) / 1000),
        (long long)((summed - made) / 1000), (long long)((profited - summed) / 1000), (long long)((finished - profited) / 1000));
    return 0;
}
