/* The first step an expert takes: the cells held inline in one array of structs (48 bytes a cell) instead of one
 * malloc each, with the same steps as naive.c. Each recalculation still reads two other cells' values, so each such
 * read brings a whole 64-byte line in for its 8 bytes. --cells=N sets how many cells. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

enum { CONSTANT, SUM, PRODUCT, SCALED };

typedef struct Cell {
    int32_t formula;
    int32_t first;
    int32_t second;
    int64_t constant;
    int64_t value;
    int32_t row;
    int32_t column;
    int32_t style;
} Cell;

static inline int64_t computed(const Cell* cell, int64_t left, int64_t right) {
    switch (cell->formula) {
        case CONSTANT: return cell->constant;
        case SUM: return (left + right) % 1000003;
        case PRODUCT: return left * right % 1000003;
        default: return left * cell->constant % 1000003;
    }
}

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
    Cell* cells = calloc(count, sizeof(Cell));
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
        Cell* cell = &cells[index];
        cell->formula = formula;
        cell->first = first;
        cell->second = second;
        cell->constant = seed % 1000;
        cell->row = index / 64;
        cell->column = index % 64;
        cell->style = (int32_t)(seed % 5);
    }
    int64_t made = now_nanoseconds();
    int64_t total = 0;
    for (int32_t round = 0; round < 20; round++) {
        cells[round * 7919 % count / 10 * 10].constant = round + 1;
        for (int32_t index = 0; index < count; index++) {
            Cell* cell = &cells[index];
            cell->value = computed(cell, cells[cell->first].value, cells[cell->second].value);
        }
        int64_t values = 0;
        for (int32_t index = 0; index < count; index++) values += cells[index].value;
        total += values;
    }
    int64_t recalculated = now_nanoseconds();
    int64_t placements = 0;
    for (int32_t index = 0; index < count; index++) {
        placements += (int64_t)cells[index].row * 64 + cells[index].column + cells[index].style * 100000000;
    }
    int64_t finished = now_nanoseconds();
    printf("total %lld last %lld placements %lld\n", (long long)total, (long long)cells[count - 1].value, (long long)placements);
    fprintf(stderr, "microseconds %lld\n", (long long)((finished - start) / 1000));
    fprintf(stderr, "phases make %lld recalculate %lld place %lld\n", (long long)((made - start) / 1000),
        (long long)((recalculated - made) / 1000), (long long)((finished - recalculated) / 1000));
    return 0;
}
