/* The second step: the cells as eight columns (a structure of arrays) with the same steps as naive.c. A
 * recalculation reads its two operands from the value column alone, 8 bytes a cell, so eight cells' values share a
 * cache line and far more of the gathered reads hit the cache. --cells=N sets how many cells. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

enum { CONSTANT, SUM, PRODUCT, SCALED };

static int32_t setting_of(int argument_count, char** arguments, const char* flag, int32_t otherwise) {
    size_t flag_length = strlen(flag);
    for (int index = 1; index < argument_count; index++) {
        if (strncmp(arguments[index], flag, flag_length) == 0) return atoi(arguments[index] + flag_length);
    }
    return otherwise;
}

int main(int argument_count, char** arguments) {
    int32_t count = setting_of(argument_count, arguments, "--cells=", 1000000);
    int64_t start = now_nanoseconds();
    int32_t* restrict formulas = malloc(sizeof(int32_t) * count);
    int32_t* restrict firsts = malloc(sizeof(int32_t) * count);
    int32_t* restrict seconds = malloc(sizeof(int32_t) * count);
    int64_t* restrict constants = malloc(sizeof(int64_t) * count);
    int64_t* restrict values = calloc(count, sizeof(int64_t));
    int32_t* restrict rows = malloc(sizeof(int32_t) * count);
    int32_t* restrict columns = malloc(sizeof(int32_t) * count);
    int32_t* restrict styles = malloc(sizeof(int32_t) * count);
    int64_t seed = 5;
    for (int32_t index = 0; index < count; index++) {
        seed = seed * 48271 % 2147483647;
        int32_t formula = CONSTANT;
        int32_t first = 0;
        int32_t second = 0;
        if (index % 10 != 0) {
            int32_t back = index - 1 - (int32_t)(seed % 64);
            first = back > 0 ? back : 0;
            second = (int32_t)(seed / 64 % index);
            int64_t choice = seed / 7 % 3;
            formula = choice == 0 ? SUM : choice == 1 ? PRODUCT : SCALED;
        }
        formulas[index] = formula;
        firsts[index] = first;
        seconds[index] = second;
        constants[index] = seed % 1000;
        rows[index] = index / 64;
        columns[index] = index % 64;
        styles[index] = (int32_t)(seed % 5);
    }
    int64_t made = now_nanoseconds();
    int64_t total = 0;
    for (int32_t round = 0; round < 20; round++) {
        constants[round * 7919 % count / 10 * 10] = round + 1;
        for (int32_t index = 0; index < count; index++) {
            int64_t left = values[firsts[index]];
            int64_t right = values[seconds[index]];
            int64_t value;
            switch (formulas[index]) {
                case CONSTANT: value = constants[index]; break;
                case SUM: value = (left + right) % 1000003; break;
                case PRODUCT: value = left * right % 1000003; break;
                default: value = left * constants[index] % 1000003; break;
            }
            values[index] = value;
        }
        int64_t sum = 0;
        for (int32_t index = 0; index < count; index++) sum += values[index];
        total += sum;
    }
    int64_t recalculated = now_nanoseconds();
    int64_t placements = 0;
    for (int32_t index = 0; index < count; index++) {
        placements += (int64_t)rows[index] * 64 + columns[index] + styles[index] * 100000000;
    }
    int64_t finished = now_nanoseconds();
    printf("total %lld last %lld placements %lld\n", (long long)total, (long long)values[count - 1], (long long)placements);
    fprintf(stderr, "microseconds %lld\n", (long long)((finished - start) / 1000));
    fprintf(stderr, "phases make %lld recalculate %lld place %lld\n", (long long)((made - start) / 1000),
        (long long)((recalculated - made) / 1000), (long long)((finished - recalculated) / 1000));
    return 0;
}
