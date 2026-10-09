/* The recalculation tuned by hand: the cells split hot and cold, the formula and its two references and constant in
 * one 16-byte struct the recalculation walks in order, the values in a column of their own narrowed to 32 bits (every
 * value is a remainder of 1000003, or a constant below 1000, so it fits), and the placement fields in a cold array
 * only the last pass reads. The gathered reads of the two operands then land in a 4 MB column instead of 48 MB of
 * cells, and the sum of the values is folded into the recalculation's own pass. --cells=N sets how many cells. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

enum { CONSTANT, SUM, PRODUCT, SCALED };

typedef struct Formula {
    int32_t first;
    int32_t second;
    int32_t constant;
    int32_t formula;
} Formula;

typedef struct Placement {
    int32_t row;
    int32_t column;
    int32_t style;
} Placement;

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
    Formula* restrict formulas = malloc(sizeof(Formula) * count);
    int32_t* restrict values = calloc(count, sizeof(int32_t));
    Placement* restrict placed = malloc(sizeof(Placement) * count);
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
        formulas[index].formula = formula;
        formulas[index].first = first;
        formulas[index].second = second;
        formulas[index].constant = (int32_t)(seed % 1000);
        placed[index].row = index / 64;
        placed[index].column = index % 64;
        placed[index].style = (int32_t)(seed % 5);
    }
    int64_t made = now_nanoseconds();
    int64_t total = 0;
    for (int32_t round = 0; round < 20; round++) {
        formulas[round * 7919 % count / 10 * 10].constant = round + 1;
        int64_t sum = 0;
        for (int32_t index = 0; index < count; index++) {
            const Formula* cell = &formulas[index];
            int64_t left = values[cell->first];
            int64_t right = values[cell->second];
            int64_t value;
            switch (cell->formula) {
                case CONSTANT: value = cell->constant; break;
                case SUM: value = (left + right) % 1000003; break;
                case PRODUCT: value = left * right % 1000003; break;
                default: value = left * cell->constant % 1000003; break;
            }
            values[index] = (int32_t)value;
            sum += value;
        }
        total += sum;
    }
    int64_t recalculated = now_nanoseconds();
    int64_t placements = 0;
    for (int32_t index = 0; index < count; index++) {
        placements += (int64_t)placed[index].row * 64 + placed[index].column + placed[index].style * 100000000;
    }
    int64_t finished = now_nanoseconds();
    printf("total %lld last %lld placements %lld\n", (long long)total, (long long)values[count - 1], (long long)placements);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld recalculate %lld place %lld\n", (long long)((made - start) / 1000),
        (long long)((recalculated - made) / 1000), (long long)((finished - recalculated) / 1000));
    return 0;
}
