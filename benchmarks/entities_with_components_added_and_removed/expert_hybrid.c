/* The hybrid: the components that never change after an entity is made (its velocity and its health) decide its
 * table, and the one that comes and goes (burning) lives in a sparse set beside the tables. There are four tables of
 * columns, one per set of a velocity and health, and an entity never moves between them, so its place is fixed when
 * it is made. The fires are a dense array of ticks and damage with the place of the entity that owns each row, plus
 * a map from entity to row; catching fire appends a row and going out moves the set's last row into the gap.
 * --items=N, --density=P, --passes=N and --churn=N, as naive/ reads them. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

enum { has_velocity = 1, has_health = 2, kinds = 4 };

typedef struct Table {
    int32_t count;
    int32_t capacity;
    int32_t* x;
    int32_t* y;
    int32_t* across;
    int32_t* down;
    int32_t* points;
} Table;

typedef struct Fires {
    int32_t count;
    int32_t capacity;
    int32_t* owner;   /* the entity */
    int32_t* place;   /* its row in its table, times four, plus its table */
    int32_t* ticks;
    int32_t* damage;
    int32_t* row_of;
} Fires;

static int32_t setting(int argument_count, char** arguments, const char* name, int32_t otherwise) {
    size_t length = strlen(name);
    for (int index = 1; index < argument_count; index++) {
        if (strncmp(arguments[index], name, length) == 0 && arguments[index][length] == '=') {
            return atoi(arguments[index] + length + 1);
        }
    }
    return otherwise;
}

static int32_t add_row(Table* table, int32_t kind) {
    if (table->count == table->capacity) {
        table->capacity = table->capacity ? table->capacity * 2 : 64;
        size_t bytes = sizeof(int32_t) * table->capacity;
        table->x = realloc(table->x, bytes);
        table->y = realloc(table->y, bytes);
        if (kind & has_velocity) {
            table->across = realloc(table->across, bytes);
            table->down = realloc(table->down, bytes);
        }
        if (kind & has_health) table->points = realloc(table->points, bytes);
    }
    return table->count++;
}

static void ignite(Fires* fires, int32_t entity, int32_t place, int64_t seed) {
    if (fires->count == fires->capacity) {
        fires->capacity = fires->capacity ? fires->capacity * 2 : 64;
        size_t bytes = sizeof(int32_t) * fires->capacity;
        fires->owner = realloc(fires->owner, bytes);
        fires->place = realloc(fires->place, bytes);
        fires->ticks = realloc(fires->ticks, bytes);
        fires->damage = realloc(fires->damage, bytes);
    }
    int32_t row = fires->count++;
    fires->owner[row] = entity;
    fires->place[row] = place;
    fires->ticks[row] = (int32_t)(seed / 5 % 8 + 1);
    fires->damage[row] = (int32_t)(seed / 40 % 5 + 1);
    fires->row_of[entity] = row;
}

static void put_out(Fires* fires, int32_t row) {
    int32_t entity = fires->owner[row];
    int32_t last = --fires->count;
    int32_t moved = fires->owner[last];
    fires->owner[row] = moved;
    fires->place[row] = fires->place[last];
    fires->ticks[row] = fires->ticks[last];
    fires->damage[row] = fires->damage[last];
    fires->row_of[moved] = row;
    fires->row_of[entity] = -1;
}

int main(int argument_count, char** arguments) {
    int32_t items = setting(argument_count, arguments, "--items", 200000);
    int32_t density = setting(argument_count, arguments, "--density", 50);
    int32_t passes = setting(argument_count, arguments, "--passes", 20);
    int32_t churn = setting(argument_count, arguments, "--churn", 2000);
    int64_t start = now_nanoseconds();
    Table tables[kinds];
    memset(tables, 0, sizeof(tables));
    Fires fires = {0};
    int32_t capacity = 64;
    int32_t* place_of = malloc(sizeof(int32_t) * capacity);
    fires.row_of = malloc(sizeof(int32_t) * capacity);
    int64_t seed = 42;
    for (int32_t index = 0; index < items; index++) {
        seed = seed * 48271 % 2147483647;
        if (index == capacity) {
            capacity = capacity * 2;
            place_of = realloc(place_of, sizeof(int32_t) * capacity);
            fires.row_of = realloc(fires.row_of, sizeof(int32_t) * capacity);
        }
        int32_t kind = 0;
        if (seed / 1048576 % 100 < density) kind |= has_velocity;
        if (seed / 3 % 100 < density) kind |= has_health;
        Table* table = &tables[kind];
        int32_t row = add_row(table, kind);
        table->x[row] = (int32_t)(seed % 4096);
        table->y[row] = (int32_t)(seed / 4096 % 4096);
        if (kind & has_velocity) {
            table->across[row] = (int32_t)(seed % 16);
            table->down[row] = (int32_t)(seed / 16 % 16);
        }
        if (kind & has_health) table->points[row] = (int32_t)(seed / 99 % 100 + 50);
        place_of[index] = row * kinds + kind;
        fires.row_of[index] = -1;
    }
    int64_t made = now_nanoseconds();
    int64_t dealt = 0;
    for (int32_t pass = 0; pass < passes; pass++) {
        for (int32_t turn = 0; turn < churn; turn++) {
            seed = seed * 48271 % 2147483647;
            int32_t entity = (int32_t)(seed / 7 % items);
            if (fires.row_of[entity] < 0) ignite(&fires, entity, place_of[entity], seed);
        }
        for (int32_t kind = has_velocity; kind < kinds; kind += 2) {
            Table* table = &tables[kind];
            int32_t* restrict column_x = table->x;
            int32_t* restrict column_y = table->y;
            const int32_t* restrict column_across = table->across;
            const int32_t* restrict column_down = table->down;
            for (int32_t row = 0; row < table->count; row++) {
                column_x[row] = (column_x[row] + column_across[row]) & 4095;
                column_y[row] = (column_y[row] + column_down[row]) & 4095;
            }
        }
        for (int32_t row = 0; row < fires.count; row++) {
            int32_t place = fires.place[row];
            int32_t kind = place & (kinds - 1);
            if (kind & has_health) {
                int32_t* points = &tables[kind].points[place / kinds];
                *points = *points - fires.damage[row];
                dealt = dealt + fires.damage[row];
            }
        }
        for (int32_t row = fires.count - 1; row >= 0; row--) {
            fires.ticks[row] = fires.ticks[row] - 1;
            if (fires.ticks[row] == 0) put_out(&fires, row);
        }
        for (int32_t kind = has_health; kind < kinds; kind++) {
            Table* table = &tables[kind];
            int32_t* restrict column_points = table->points;
            for (int32_t row = 0; row < table->count; row++) column_points[row] = column_points[row] + 1;
        }
    }
    int64_t positions = 0;
    int64_t points = 0;
    for (int32_t kind = 0; kind < kinds; kind++) {
        Table* table = &tables[kind];
        for (int32_t row = 0; row < table->count; row++) positions = positions + table->x[row] + table->y[row];
        if (kind & has_health) {
            for (int32_t row = 0; row < table->count; row++) points = points + table->points[row];
        }
    }
    int64_t finished = now_nanoseconds();
    printf("dealt %lld positions %lld points %lld burning %d\n", (long long)dealt, (long long)positions, (long long)points,
           fires.count);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld passes %lld\n", (long long)((made - start) / 1000), (long long)((finished - made) / 1000));
    return 0;
}
