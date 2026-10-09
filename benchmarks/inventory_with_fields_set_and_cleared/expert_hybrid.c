/* The hybrid: the part that never changes after a record is made (its supplier) decides its table, and the part that
 * is set and cleared (its reservation) lives in a sparse set beside the tables. There are two tables of columns,
 * records without a supplier and records with one, and a record never moves between them, so its place is fixed when
 * it is made. The reservations are a dense array of values with the place of the record that owns each row, plus a
 * map from record to row; setting one appends a row and clearing one moves the set's last row into the gap.
 * --items=N, --density=P, --passes=N and --churn=N, as naive/ reads them. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

typedef struct Table {
    int32_t count;
    int32_t capacity;
    int32_t* price;
    int32_t* stock;
    int32_t* lead_days;
} Table;

typedef struct Set {
    int32_t count;
    int32_t capacity;
    int32_t* owner;   /* the record */
    int32_t* place;   /* its row in its table, times two, plus one when the table is the one with a supplier */
    int32_t* quantity;
    int32_t* age;
    int32_t* row_of;
} Set;

static int32_t setting(int argument_count, char** arguments, const char* name, int32_t otherwise) {
    size_t length = strlen(name);
    for (int index = 1; index < argument_count; index++) {
        if (strncmp(arguments[index], name, length) == 0 && arguments[index][length] == '=') {
            return atoi(arguments[index] + length + 1);
        }
    }
    return otherwise;
}

static int32_t add_row(Table* table, int32_t with_supplier) {
    if (table->count == table->capacity) {
        table->capacity = table->capacity ? table->capacity * 2 : 64;
        size_t bytes = sizeof(int32_t) * table->capacity;
        table->price = realloc(table->price, bytes);
        table->stock = realloc(table->stock, bytes);
        if (with_supplier) table->lead_days = realloc(table->lead_days, bytes);
    }
    return table->count++;
}

static void add(Set* set, int32_t record, int32_t place, int32_t quantity) {
    if (set->count == set->capacity) {
        set->capacity = set->capacity ? set->capacity * 2 : 64;
        size_t bytes = sizeof(int32_t) * set->capacity;
        set->owner = realloc(set->owner, bytes);
        set->place = realloc(set->place, bytes);
        set->quantity = realloc(set->quantity, bytes);
        set->age = realloc(set->age, bytes);
    }
    set->owner[set->count] = record;
    set->place[set->count] = place;
    set->quantity[set->count] = quantity;
    set->age[set->count] = 0;
    set->row_of[record] = set->count;
    set->count++;
}

static void take(Set* set, int32_t record) {
    int32_t row = set->row_of[record];
    int32_t last = --set->count;
    int32_t moved = set->owner[last];
    set->owner[row] = moved;
    set->place[row] = set->place[last];
    set->quantity[row] = set->quantity[last];
    set->age[row] = set->age[last];
    set->row_of[moved] = row;
    set->row_of[record] = -1;
}

int main(int argument_count, char** arguments) {
    int32_t items = setting(argument_count, arguments, "--items", 200000);
    int32_t density = setting(argument_count, arguments, "--density", 50);
    int32_t passes = setting(argument_count, arguments, "--passes", 20);
    int32_t churn = setting(argument_count, arguments, "--churn", 2000);
    int64_t start = now_nanoseconds();
    Table tables[2];
    memset(tables, 0, sizeof(tables));
    Set reservations = {0};
    int32_t capacity = 64;
    int32_t* place_of = malloc(sizeof(int32_t) * capacity);
    reservations.row_of = malloc(sizeof(int32_t) * capacity);
    int64_t seed = 42;
    for (int32_t index = 0; index < items; index++) {
        seed = seed * 48271 % 2147483647;
        if (index == capacity) {
            capacity = capacity * 2;
            place_of = realloc(place_of, sizeof(int32_t) * capacity);
            reservations.row_of = realloc(reservations.row_of, sizeof(int32_t) * capacity);
        }
        int32_t with_supplier = seed / 1048576 % 100 < density;
        Table* table = &tables[with_supplier];
        int32_t row = add_row(table, with_supplier);
        table->price[row] = (int32_t)(seed % 1000 + 1);
        table->stock[row] = (int32_t)(seed / 1000 % 100);
        if (with_supplier) table->lead_days[row] = (int32_t)(seed / 99 % 30 + 1);
        place_of[index] = row * 2 + with_supplier;
        reservations.row_of[index] = -1;
        if (seed / 3 % 100 < density) add(&reservations, index, place_of[index], (int32_t)(seed % 50 + 1));
    }
    int64_t made = now_nanoseconds();
    int64_t reserved = 0;
    int64_t late = 0;
    for (int32_t pass = 0; pass < passes; pass++) {
        for (int32_t turn = 0; turn < churn; turn++) {
            seed = seed * 48271 % 2147483647;
            int32_t record = (int32_t)(seed / 7 % items);
            int32_t row = reservations.row_of[record];
            if (row >= 0) {
                int32_t place = place_of[record];
                tables[place & 1].stock[place >> 1] += reservations.quantity[row];
                take(&reservations, record);
            } else {
                add(&reservations, record, place_of[record], (int32_t)(seed % 50 + 1));
            }
        }
        for (int32_t row = 0; row < reservations.count; row++) {
            int32_t place = reservations.place[row];
            int64_t quantity = reservations.quantity[row];
            reserved = reserved + tables[place & 1].price[place >> 1] * quantity;
        }
        for (int32_t row = 0; row < reservations.count; row++) {
            int32_t place = reservations.place[row];
            if (place & 1) late = late + (int64_t)tables[1].lead_days[place >> 1] * reservations.quantity[row];
        }
        int32_t* restrict age = reservations.age;
        for (int32_t row = 0; row < reservations.count; row++) age[row] = age[row] + 1;
    }
    int64_t ages = 0;
    int64_t stock = 0;
    for (int32_t row = 0; row < reservations.count; row++) ages = ages + reservations.age[row];
    for (int32_t table = 0; table < 2; table++) {
        for (int32_t row = 0; row < tables[table].count; row++) stock = stock + tables[table].stock[row];
    }
    int64_t finished = now_nanoseconds();
    printf("reserved %lld late %lld ages %lld stock %lld\n", (long long)reserved, (long long)late, (long long)ages,
           (long long)stock);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld passes %lld\n", (long long)((made - start) / 1000), (long long)((finished - made) / 1000));
    return 0;
}
