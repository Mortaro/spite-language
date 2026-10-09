/* The same report tuned by hand as far as it goes: the sales as columns narrowed to the bytes their values need
 * (the region and the quantity in a byte, the product, price, cost and day in two, the customer in four: 15 bytes a
 * sale instead of 32), and the three reports fused into one pass the C compiler vectorises. The 64 passes for the
 * profit of each region in each quarter become one, since the answer the program prints, the sum over the 64 cells of
 * each cell's profit times its number plus one, is the sum over sales of each sale's profit times its cell's number
 * plus one.
 * --records=N sets how many sales. */
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
    uint8_t* restrict region = malloc(records);
    uint16_t* restrict product = malloc(sizeof(uint16_t) * records);
    uint8_t* restrict quantity = malloc(records);
    uint16_t* restrict price = malloc(sizeof(uint16_t) * records);
    uint16_t* restrict cost = malloc(sizeof(uint16_t) * records);
    uint8_t* restrict discount = malloc(records);
    uint16_t* restrict day = malloc(sizeof(uint16_t) * records);
    int32_t* restrict customer = malloc(sizeof(int32_t) * records);
    int64_t seed = 42;
    for (int32_t index = 0; index < records; index++) {
        seed = seed * 48271 % 2147483647;
        region[index] = (uint8_t)(seed % 16);
        product[index] = (uint16_t)(seed / 16 % 1000);
        quantity[index] = (uint8_t)(seed / 16000 % 20 + 1);
        int32_t made_price = (int32_t)(seed / 320000 % 500 + 100);
        price[index] = (uint16_t)made_price;
        cost[index] = (uint16_t)(made_price * 3 / 4 - (int32_t)(seed % 37));
        discount[index] = (uint8_t)(seed % 101);
        day[index] = (uint16_t)(seed / 7 % 365);
        customer[index] = (int32_t)(seed / 2555 % 100000);
    }
    int64_t made = now_nanoseconds();
    int64_t units = 0;
    int64_t by_cell = 0;
    int64_t fingerprints = 0;
    for (int32_t index = 0; index < records; index++) {
        int32_t margin = quantity[index] * (price[index] - cost[index]);
        units += quantity[index];
        by_cell += (int64_t)margin * (region[index] * 4 + day[index] / 92 + 1);
        fingerprints += (int64_t)region[index] + product[index] * 3 + quantity[index] * 5 + price[index] * 7
            + cost[index] * 11 + discount[index] * 13 + day[index] * 17 + customer[index] * 19;
    }
    int64_t finished = now_nanoseconds();
    printf("units %lld by region and quarter %lld fingerprints %lld first %d\n", (long long)units, (long long)by_cell,
        (long long)fingerprints, customer[0]);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld fused %lld\n", (long long)((made - start) / 1000), (long long)((finished - made) / 1000));
    return 0;
}
