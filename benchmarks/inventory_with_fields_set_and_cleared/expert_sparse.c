/* The records kept in the order they were made, with each optional part in a sparse set of its own: the price and
 * the stock are columns over all records, and the supplier and the reservation are each a dense array of values
 * with the record that owns each row, plus a map from record to row (-1 when it has none). Setting a reservation
 * appends a row; clearing one moves the set's last row into the gap. Nothing else moves. A loop over reservations
 * walks the set and reaches the price by the owner; the late quantity walks the smaller of the two sets and probes
 * the other's map.
 * --items=N, --density=P, --passes=N and --churn=N, as naive/ reads them. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

typedef struct Set {
    int32_t count;
    int32_t capacity;
    int32_t* owner;
    int32_t* first;
    int32_t* second;
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

static void add(Set* set, int32_t record, int32_t first, int32_t second) {
    if (set->count == set->capacity) {
        set->capacity = set->capacity ? set->capacity * 2 : 64;
        set->owner = realloc(set->owner, sizeof(int32_t) * set->capacity);
        set->first = realloc(set->first, sizeof(int32_t) * set->capacity);
        set->second = realloc(set->second, sizeof(int32_t) * set->capacity);
    }
    set->owner[set->count] = record;
    set->first[set->count] = first;
    set->second[set->count] = second;
    set->row_of[record] = set->count;
    set->count++;
}

static void take(Set* set, int32_t record) {
    int32_t row = set->row_of[record];
    int32_t last = --set->count;
    int32_t moved = set->owner[last];
    set->owner[row] = moved;
    set->first[row] = set->first[last];
    set->second[row] = set->second[last];
    set->row_of[moved] = row;
    set->row_of[record] = -1;
}

int main(int argument_count, char** arguments) {
    int32_t items = setting(argument_count, arguments, "--items", 200000);
    int32_t density = setting(argument_count, arguments, "--density", 50);
    int32_t passes = setting(argument_count, arguments, "--passes", 20);
    int32_t churn = setting(argument_count, arguments, "--churn", 2000);
    int64_t start = now_nanoseconds();
    int32_t capacity = 64;
    int32_t* price = malloc(sizeof(int32_t) * capacity);
    int32_t* stock = malloc(sizeof(int32_t) * capacity);
    Set suppliers = {0};
    Set reservations = {0};
    suppliers.row_of = malloc(sizeof(int32_t) * capacity);
    reservations.row_of = malloc(sizeof(int32_t) * capacity);
    int64_t seed = 42;
    for (int32_t index = 0; index < items; index++) {
        seed = seed * 48271 % 2147483647;
        if (index == capacity) {
            capacity = capacity * 2;
            price = realloc(price, sizeof(int32_t) * capacity);
            stock = realloc(stock, sizeof(int32_t) * capacity);
            suppliers.row_of = realloc(suppliers.row_of, sizeof(int32_t) * capacity);
            reservations.row_of = realloc(reservations.row_of, sizeof(int32_t) * capacity);
        }
        price[index] = (int32_t)(seed % 1000 + 1);
        stock[index] = (int32_t)(seed / 1000 % 100);
        suppliers.row_of[index] = -1;
        reservations.row_of[index] = -1;
        if (seed / 1048576 % 100 < density) add(&suppliers, index, (int32_t)(seed / 99 % 30 + 1), 0);
        if (seed / 3 % 100 < density) add(&reservations, index, (int32_t)(seed % 50 + 1), 0);
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
                stock[record] = stock[record] + reservations.first[row];
                take(&reservations, record);
            } else {
                add(&reservations, record, (int32_t)(seed % 50 + 1), 0);
            }
        }
        for (int32_t row = 0; row < reservations.count; row++) {
            reserved = reserved + (int64_t)price[reservations.owner[row]] * reservations.first[row];
        }
        if (suppliers.count <= reservations.count) {
            for (int32_t row = 0; row < suppliers.count; row++) {
                int32_t reservation = reservations.row_of[suppliers.owner[row]];
                if (reservation >= 0) late = late + (int64_t)suppliers.first[row] * reservations.first[reservation];
            }
        } else {
            for (int32_t row = 0; row < reservations.count; row++) {
                int32_t supplier = suppliers.row_of[reservations.owner[row]];
                if (supplier >= 0) late = late + (int64_t)suppliers.first[supplier] * reservations.first[row];
            }
        }
        int32_t* restrict age = reservations.second;
        for (int32_t row = 0; row < reservations.count; row++) age[row] = age[row] + 1;
    }
    int64_t ages = 0;
    int64_t total_stock = 0;
    for (int32_t row = 0; row < reservations.count; row++) ages = ages + reservations.second[row];
    for (int32_t index = 0; index < items; index++) total_stock = total_stock + stock[index];
    int64_t finished = now_nanoseconds();
    printf("reserved %lld late %lld ages %lld stock %lld\n", (long long)reserved, (long long)late, (long long)ages,
           (long long)total_stock);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld passes %lld\n", (long long)((made - start) / 1000), (long long)((finished - made) / 1000));
    return 0;
}
