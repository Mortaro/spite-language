/* naive/ written in C the way a C programmer writes it from the Spite: a struct per cell, one malloc each, a growable
 * array of pointers, and the same steps: twenty rounds of an edit, a recalculation of every cell from the values of
 * the two cells it refers to (one near it, one anywhere before it), and a sum of the values; then the sum of each
 * cell's placement. --cells=N sets how many cells. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

typedef enum Formula { CONSTANT, SUM, PRODUCT, SCALED } Formula;

typedef struct Cell {
    Formula formula;
    int32_t first;
    int32_t second;
    int64_t constant;
    int64_t value;
    int32_t row;
    int32_t column;
    int32_t style;
} Cell;

typedef struct Cells {
    Cell** items;
    int32_t count;
    int32_t capacity;
} Cells;

static void append(Cells* cells, Cell* cell) {
    if (cells->count == cells->capacity) {
        cells->capacity = cells->capacity ? cells->capacity * 2 : 8;
        cells->items = realloc(cells->items, sizeof(Cell*) * cells->capacity);
    }
    cells->items[cells->count] = cell;
    cells->count = cells->count + 1;
}

static Cell* make_cell(int32_t index, int64_t seed) {
    Cell* cell = calloc(1, sizeof(Cell));
    cell->formula = CONSTANT;
    cell->row = index / 64;
    cell->column = index % 64;
    cell->style = (int32_t)(seed % 5);
    cell->constant = seed % 1000;
    if (index % 10 != 0) {
        int32_t back = index - 1 - (int32_t)(seed % 64);
        cell->first = back > 0 ? back : 0;
        cell->second = (int32_t)(seed / 64 % index);
        int64_t choice = seed / 7 % 3;
        cell->formula = choice == 0 ? SUM : choice == 1 ? PRODUCT : SCALED;
    }
    return cell;
}

static int64_t computed(Cell* cell, int64_t left, int64_t right) {
    switch (cell->formula) {
        case CONSTANT: return cell->constant;
        case SUM: return (left + right) % 1000003;
        case PRODUCT: return left * right % 1000003;
        default: return left * cell->constant % 1000003;
    }
}

static void recalculate(Cells* cells) {
    for (int32_t index = 0; index < cells->count; index = index + 1) {
        Cell* cell = cells->items[index];
        int64_t left = cells->items[cell->first]->value;
        int64_t right = cells->items[cell->second]->value;
        cell->value = computed(cell, left, right);
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
    Cells cells = {0};
    int64_t seed = 5;
    for (int32_t index = 0; index < count; index = index + 1) {
        seed = seed * 48271 % 2147483647;
        append(&cells, make_cell(index, seed));
    }
    int64_t made = now_nanoseconds();
    int64_t total = 0;
    for (int32_t round = 0; round < 20; round = round + 1) {
        int32_t edited = round * 7919 % cells.count / 10 * 10;
        cells.items[edited]->constant = round + 1;
        recalculate(&cells);
        int64_t values = 0;
        for (int32_t index = 0; index < cells.count; index = index + 1) values = values + cells.items[index]->value;
        total = total + values;
    }
    int64_t recalculated = now_nanoseconds();
    int64_t placements = 0;
    for (int32_t index = 0; index < cells.count; index = index + 1) {
        Cell* cell = cells.items[index];
        placements = placements + (int64_t)cell->row * 64 + cell->column + cell->style * 100000000;
    }
    int64_t finished = now_nanoseconds();
    printf("total %lld last %lld placements %lld\n", (long long)total, (long long)cells.items[cells.count - 1]->value,
        (long long)placements);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld recalculate %lld place %lld\n", (long long)((made - start) / 1000),
        (long long)((recalculated - made) / 1000), (long long)((finished - recalculated) / 1000));
    return 0;
}
