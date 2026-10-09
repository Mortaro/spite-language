/* The records grouped by the parts they have (archetype tables): one table of columns for each set of present parts
 * (none, a supplier, a reservation, both). A loop that needs a part walks only the tables that have it, with no
 * test. Setting or clearing a reservation moves the record to the other table: its columns are copied to the end of
 * that table and the last row of its old table moves into the gap, so each table also keeps the record of each row
 * and each record its table and row.
 * --items=N, --density=P, --passes=N and --churn=N, as naive/ reads them. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

enum { has_supplier = 1, has_reservation = 2, kinds = 4 };

typedef struct Table {
    int32_t count;
    int32_t capacity;
    int32_t* owner;
    int32_t* price;
    int32_t* stock;
    int32_t* lead_days;
    int32_t* quantity;
    int32_t* age;
} Table;

static Table tables[kinds];
static uint8_t* kind_of;
static int32_t* row_of;

static int32_t setting(int argument_count, char** arguments, const char* name, int32_t otherwise) {
    size_t length = strlen(name);
    for (int index = 1; index < argument_count; index++) {
        if (strncmp(arguments[index], name, length) == 0 && arguments[index][length] == '=') {
            return atoi(arguments[index] + length + 1);
        }
    }
    return otherwise;
}

static int32_t add_row(int32_t kind, int32_t record) {
    Table* table = &tables[kind];
    if (table->count == table->capacity) {
        table->capacity = table->capacity ? table->capacity * 2 : 64;
        size_t bytes = sizeof(int32_t) * table->capacity;
        table->owner = realloc(table->owner, bytes);
        table->price = realloc(table->price, bytes);
        table->stock = realloc(table->stock, bytes);
        if (kind & has_supplier) table->lead_days = realloc(table->lead_days, bytes);
        if (kind & has_reservation) {
            table->quantity = realloc(table->quantity, bytes);
            table->age = realloc(table->age, bytes);
        }
    }
    int32_t row = table->count++;
    table->owner[row] = record;
    kind_of[record] = (uint8_t)kind;
    row_of[record] = row;
    return row;
}

/* the last row of the table moves into the gap the record leaves */
static void remove_row(int32_t kind, int32_t row) {
    Table* table = &tables[kind];
    int32_t last = --table->count;
    if (row == last) return;
    int32_t moved = table->owner[last];
    table->owner[row] = moved;
    table->price[row] = table->price[last];
    table->stock[row] = table->stock[last];
    if (kind & has_supplier) table->lead_days[row] = table->lead_days[last];
    if (kind & has_reservation) {
        table->quantity[row] = table->quantity[last];
        table->age[row] = table->age[last];
    }
    row_of[moved] = row;
}

static void toggle(int32_t record, int64_t seed) {
    int32_t kind = kind_of[record];
    int32_t row = row_of[record];
    Table* from = &tables[kind];
    int32_t to_kind = kind ^ has_reservation;
    int32_t to_row = add_row(to_kind, record);
    Table* to = &tables[to_kind];
    to->price[to_row] = from->price[row];
    to->stock[to_row] = from->stock[row];
    if (kind & has_supplier) to->lead_days[to_row] = from->lead_days[row];
    if (kind & has_reservation) {
        to->stock[to_row] = to->stock[to_row] + from->quantity[row];
    } else {
        to->quantity[to_row] = (int32_t)(seed % 50 + 1);
        to->age[to_row] = 0;
    }
    remove_row(kind, row);
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
        if (seed / 1048576 % 100 < density) kind |= has_supplier;
        if (seed / 3 % 100 < density) kind |= has_reservation;
        int32_t row = add_row(kind, index);
        Table* table = &tables[kind];
        table->price[row] = (int32_t)(seed % 1000 + 1);
        table->stock[row] = (int32_t)(seed / 1000 % 100);
        if (kind & has_supplier) table->lead_days[row] = (int32_t)(seed / 99 % 30 + 1);
        if (kind & has_reservation) {
            table->quantity[row] = (int32_t)(seed % 50 + 1);
            table->age[row] = 0;
        }
    }
    int64_t made = now_nanoseconds();
    int64_t reserved = 0;
    int64_t late = 0;
    for (int32_t pass = 0; pass < passes; pass++) {
        for (int32_t turn = 0; turn < churn; turn++) {
            seed = seed * 48271 % 2147483647;
            toggle((int32_t)(seed / 7 % items), seed);
        }
        for (int32_t kind = has_reservation; kind < kinds; kind++) {
            const Table* table = &tables[kind];
            const int32_t* restrict price = table->price;
            const int32_t* restrict quantity = table->quantity;
            for (int32_t row = 0; row < table->count; row++) reserved = reserved + (int64_t)price[row] * quantity[row];
        }
        const Table* both = &tables[has_supplier | has_reservation];
        const int32_t* restrict lead_days = both->lead_days;
        const int32_t* restrict quantity = both->quantity;
        for (int32_t row = 0; row < both->count; row++) late = late + (int64_t)lead_days[row] * quantity[row];
        for (int32_t kind = has_reservation; kind < kinds; kind++) {
            Table* table = &tables[kind];
            int32_t* restrict age = table->age;
            for (int32_t row = 0; row < table->count; row++) age[row] = age[row] + 1;
        }
    }
    int64_t ages = 0;
    int64_t stock = 0;
    for (int32_t kind = 0; kind < kinds; kind++) {
        const Table* table = &tables[kind];
        for (int32_t row = 0; row < table->count; row++) stock = stock + table->stock[row];
        if (kind & has_reservation) {
            for (int32_t row = 0; row < table->count; row++) ages = ages + table->age[row];
        }
    }
    int64_t finished = now_nanoseconds();
    printf("reserved %lld late %lld ages %lld stock %lld\n", (long long)reserved, (long long)late, (long long)ages,
           (long long)stock);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld passes %lld\n", (long long)((made - start) / 1000), (long long)((finished - made) / 1000));
    return 0;
}
