/* The entities grouped by the components they have (archetype tables): one table of columns for each of the eight
 * sets of a velocity, health and burning, every column holding one field. A system walks only the tables that have
 * its components, with no test. Catching fire or going out moves the entity to the table with or without burning:
 * its fields are copied to the end of that table and the last row of its old table moves into the gap, so each
 * table keeps the entity of each row and each entity its table and row. Cooling walks each burning table from its
 * end, so the row that moves into a gap has already been cooled.
 * --items=N, --density=P, --passes=N and --churn=N, as naive/ reads them. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

enum { has_velocity = 1, has_health = 2, has_burning = 4, kinds = 8 };
enum { owner, x, y, across, down, points, ticks, damage, fields };

typedef struct Table {
    int32_t count;
    int32_t capacity;
    int32_t* column[fields];
} Table;

static Table tables[kinds];
static uint8_t* kind_of;
static int32_t* row_of;

static int field_in(int32_t kind, int field) {
    if (field == across || field == down) return (kind & has_velocity) != 0;
    if (field == points) return (kind & has_health) != 0;
    if (field == ticks || field == damage) return (kind & has_burning) != 0;
    return 1;
}

static int32_t setting(int argument_count, char** arguments, const char* name, int32_t otherwise) {
    size_t length = strlen(name);
    for (int index = 1; index < argument_count; index++) {
        if (strncmp(arguments[index], name, length) == 0 && arguments[index][length] == '=') {
            return atoi(arguments[index] + length + 1);
        }
    }
    return otherwise;
}

static int32_t add_row(int32_t kind, int32_t entity) {
    Table* table = &tables[kind];
    if (table->count == table->capacity) {
        table->capacity = table->capacity ? table->capacity * 2 : 64;
        for (int field = 0; field < fields; field++) {
            if (field_in(kind, field)) table->column[field] = realloc(table->column[field], sizeof(int32_t) * table->capacity);
        }
    }
    int32_t row = table->count++;
    table->column[owner][row] = entity;
    kind_of[entity] = (uint8_t)kind;
    row_of[entity] = row;
    return row;
}

static void remove_row(int32_t kind, int32_t row) {
    Table* table = &tables[kind];
    int32_t last = --table->count;
    if (row == last) return;
    for (int field = 0; field < fields; field++) {
        if (field_in(kind, field)) table->column[field][row] = table->column[field][last];
    }
    row_of[table->column[owner][row]] = row;
}

/* moves an entity to another table, copying the fields both tables have; answers its new row */
static int32_t move_entity(int32_t entity, int32_t to_kind) {
    int32_t kind = kind_of[entity];
    int32_t row = row_of[entity];
    int32_t to_row = add_row(to_kind, entity);
    Table* from = &tables[kind];
    Table* to = &tables[to_kind];
    for (int field = 1; field < fields; field++) {
        if (field_in(kind, field) && field_in(to_kind, field)) to->column[field][to_row] = from->column[field][row];
    }
    remove_row(kind, row);
    return to_row;
}

int main(int argument_count, char** arguments) {
    int32_t items = setting(argument_count, arguments, "--items", 200000);
    int32_t density = setting(argument_count, arguments, "--density", 50);
    int32_t passes = setting(argument_count, arguments, "--passes", 20);
    int32_t churn = setting(argument_count, arguments, "--churn", 2000);
    int64_t start = now_nanoseconds();
    int32_t capacity = 64;
    kind_of = malloc(capacity);
    row_of = malloc(sizeof(int32_t) * capacity);
    int64_t seed = 42;
    for (int32_t index = 0; index < items; index++) {
        seed = seed * 48271 % 2147483647;
        if (index == capacity) {
            capacity = capacity * 2;
            kind_of = realloc(kind_of, capacity);
            row_of = realloc(row_of, sizeof(int32_t) * capacity);
        }
        int32_t kind = 0;
        if (seed / 1048576 % 100 < density) kind |= has_velocity;
        if (seed / 3 % 100 < density) kind |= has_health;
        int32_t row = add_row(kind, index);
        int32_t** column = tables[kind].column;
        column[x][row] = (int32_t)(seed % 4096);
        column[y][row] = (int32_t)(seed / 4096 % 4096);
        if (kind & has_velocity) {
            column[across][row] = (int32_t)(seed % 16);
            column[down][row] = (int32_t)(seed / 16 % 16);
        }
        if (kind & has_health) column[points][row] = (int32_t)(seed / 99 % 100 + 50);
    }
    int64_t made = now_nanoseconds();
    int64_t dealt = 0;
    for (int32_t pass = 0; pass < passes; pass++) {
        for (int32_t turn = 0; turn < churn; turn++) {
            seed = seed * 48271 % 2147483647;
            int32_t entity = (int32_t)(seed / 7 % items);
            int32_t kind = kind_of[entity];
            if (kind & has_burning) continue;
            int32_t row = move_entity(entity, kind | has_burning);
            int32_t** column = tables[kind | has_burning].column;
            column[ticks][row] = (int32_t)(seed / 5 % 8 + 1);
            column[damage][row] = (int32_t)(seed / 40 % 5 + 1);
        }
        for (int32_t kind = 0; kind < kinds; kind++) {
            if (!(kind & has_velocity)) continue;
            Table* table = &tables[kind];
            int32_t* restrict column_x = table->column[x];
            int32_t* restrict column_y = table->column[y];
            const int32_t* restrict column_across = table->column[across];
            const int32_t* restrict column_down = table->column[down];
            for (int32_t row = 0; row < table->count; row++) {
                column_x[row] = (column_x[row] + column_across[row]) & 4095;
                column_y[row] = (column_y[row] + column_down[row]) & 4095;
            }
        }
        for (int32_t kind = 0; kind < kinds; kind++) {
            if ((kind & (has_health | has_burning)) != (has_health | has_burning)) continue;
            Table* table = &tables[kind];
            int32_t* restrict column_points = table->column[points];
            const int32_t* restrict column_damage = table->column[damage];
            for (int32_t row = 0; row < table->count; row++) {
                column_points[row] = column_points[row] - column_damage[row];
                dealt = dealt + column_damage[row];
            }
        }
        for (int32_t kind = 0; kind < kinds; kind++) {
            if (!(kind & has_burning)) continue;
            Table* table = &tables[kind];
            for (int32_t row = table->count - 1; row >= 0; row--) {
                int32_t left = --table->column[ticks][row];
                if (left == 0) move_entity(table->column[owner][row], kind & ~has_burning);
            }
        }
        for (int32_t kind = 0; kind < kinds; kind++) {
            if (!(kind & has_health)) continue;
            Table* table = &tables[kind];
            int32_t* restrict column_points = table->column[points];
            for (int32_t row = 0; row < table->count; row++) column_points[row] = column_points[row] + 1;
        }
    }
    int64_t positions = 0;
    int64_t total_points = 0;
    int32_t burning = 0;
    for (int32_t kind = 0; kind < kinds; kind++) {
        Table* table = &tables[kind];
        for (int32_t row = 0; row < table->count; row++) positions = positions + table->column[x][row] + table->column[y][row];
        if (kind & has_health) {
            for (int32_t row = 0; row < table->count; row++) total_points = total_points + table->column[points][row];
        }
        if (kind & has_burning) burning = burning + table->count;
    }
    int64_t finished = now_nanoseconds();
    printf("dealt %lld positions %lld points %lld burning %d\n", (long long)dealt, (long long)positions,
           (long long)total_points, burning);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld passes %lld\n", (long long)((made - start) / 1000), (long long)((finished - made) / 1000));
    return 0;
}
