/* The shapes grouped by the parts they have (archetype tables): one table for each set of present parts (none, a
 * texture, a velocity, both), each table a set of columns holding only the parts its shapes have. A loop that needs
 * a part walks only the tables that have it, contiguously and with no test: moving walks two tables, drawing one,
 * fading two. The order of the shapes is lost, which nothing the program prints can see (every answer is a sum).
 * --items=N, --density=P and --passes=N, as naive/ reads them. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

enum { has_texture = 1, has_velocity = 2, kinds = 4 };

typedef struct Table {
    int32_t count;
    int32_t capacity;
    int32_t* restrict x;
    int32_t* restrict y;
    int32_t* restrict scale;
    int32_t* restrict across;
    int32_t* restrict down;
} Table;

static int32_t setting(int argument_count, char** arguments, const char* name, int32_t otherwise) {
    size_t length = strlen(name);
    for (int index = 1; index < argument_count; index++) {
        if (strncmp(arguments[index], name, length) == 0 && arguments[index][length] == '=') {
            return atoi(arguments[index] + length + 1);
        }
    }
    return otherwise;
}

static void grow(Table* table, int32_t kind) {
    table->capacity = table->capacity ? table->capacity * 2 : 64;
    size_t bytes = sizeof(int32_t) * table->capacity;
    table->x = realloc(table->x, bytes);
    table->y = realloc(table->y, bytes);
    if (kind & has_texture) table->scale = realloc(table->scale, bytes);
    if (kind & has_velocity) {
        table->across = realloc(table->across, bytes);
        table->down = realloc(table->down, bytes);
    }
}

static int32_t kind_of(int64_t seed, int32_t density) {
    int32_t kind = 0;
    if (seed / 1048576 % 100 < density) kind |= has_texture;
    if (seed / 3 % 100 < density) kind |= has_velocity;
    return kind;
}

int main(int argument_count, char** arguments) {
    int32_t items = setting(argument_count, arguments, "--items", 200000);
    int32_t density = setting(argument_count, arguments, "--density", 50);
    int32_t passes = setting(argument_count, arguments, "--passes", 20);
    int64_t start = now_nanoseconds();
    /* each table grows as shapes arrive, the way a list does: a compiler cannot count them in advance */
    Table tables[kinds];
    memset(tables, 0, sizeof(tables));
    int64_t seed = 42;
    for (int32_t index = 0; index < items; index++) {
        seed = seed * 48271 % 2147483647;
        int32_t kind = kind_of(seed, density);
        Table* table = &tables[kind];
        if (table->count == table->capacity) grow(table, kind);
        int32_t row = table->count++;
        table->x[row] = (int32_t)(seed % 1024);
        table->y[row] = (int32_t)(seed / 1024 % 1024);
        if (kind & has_texture) table->scale[row] = (int32_t)(seed / 251 % 64);
        if (kind & has_velocity) {
            table->across[row] = (int32_t)(seed % 16);
            table->down[row] = (int32_t)(seed / 16 % 16);
        }
    }
    int64_t made = now_nanoseconds();
    int64_t drawn = 0;
    for (int32_t pass = 0; pass < passes; pass++) {
        for (int32_t kind = has_velocity; kind < kinds; kind++) {
            Table* table = &tables[kind];
            for (int32_t row = 0; row < table->count; row++) {
                table->x[row] = (table->x[row] + table->across[row]) & 1023;
                table->y[row] = (table->y[row] + table->down[row]) & 1023;
            }
        }
        Table* both = &tables[has_texture | has_velocity];
        for (int32_t row = 0; row < both->count; row++) {
            drawn = drawn + (int64_t)(both->x[row] + both->y[row]) * both->scale[row];
        }
        for (int32_t kind = has_texture; kind < kinds; kind += 2) {
            Table* table = &tables[kind];
            for (int32_t row = 0; row < table->count; row++) {
                table->scale[row] = (table->scale[row] * 5 + 3) & 63;
            }
        }
    }
    int64_t positions = 0;
    int64_t scales = 0;
    for (int32_t kind = 0; kind < kinds; kind++) {
        Table* table = &tables[kind];
        for (int32_t row = 0; row < table->count; row++) positions = positions + table->x[row] + table->y[row];
        if (kind & has_texture) {
            for (int32_t row = 0; row < table->count; row++) scales = scales + table->scale[row];
        }
    }
    int64_t finished = now_nanoseconds();
    printf("drawn %lld positions %lld scales %lld\n", (long long)drawn, (long long)positions, (long long)scales);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld passes %lld\n", (long long)((made - start) / 1000), (long long)((finished - made) / 1000));
    return 0;
}
