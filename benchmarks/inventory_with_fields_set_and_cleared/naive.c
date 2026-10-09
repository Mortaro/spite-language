/* naive/ written in C the way a C programmer writes it: a struct per record with a pointer to its supplier and to
 * its reservation (NULL when it has none), one malloc per object, a reservation freed when it is cleared, a growable
 * array of pointers, and the same loops: every pass sets or clears the reservation of --churn records chosen at
 * random, then adds up the reserved value, the late quantity of the records with both parts, and ages every
 * reservation.
 * --items=N, --density=P (the percent of records with each part at the start), --passes=N and --churn=N. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

typedef struct Supplier {
    int32_t lead_days;
} Supplier;

typedef struct Reservation {
    int32_t quantity;
    int32_t age;
} Reservation;

typedef struct Record {
    int32_t price;
    int32_t stock;
    Supplier* supplier;
    Reservation* reservation;
} Record;

typedef struct Records {
    Record** items;
    int32_t count;
    int32_t capacity;
} Records;

static void append(Records* records, Record* record) {
    if (records->count == records->capacity) {
        records->capacity = records->capacity ? records->capacity * 2 : 8;
        records->items = realloc(records->items, sizeof(Record*) * records->capacity);
    }
    records->items[records->count] = record;
    records->count = records->count + 1;
}

static Reservation* make_reservation(int64_t seed) {
    Reservation* reservation = malloc(sizeof(Reservation));
    reservation->quantity = (int32_t)(seed % 50 + 1);
    reservation->age = 0;
    return reservation;
}

static Record* make_record(int64_t seed, int32_t density) {
    Record* record = malloc(sizeof(Record));
    record->price = (int32_t)(seed % 1000 + 1);
    record->stock = (int32_t)(seed / 1000 % 100);
    record->supplier = NULL;
    record->reservation = NULL;
    if (seed / 1048576 % 100 < density) {
        record->supplier = malloc(sizeof(Supplier));
        record->supplier->lead_days = (int32_t)(seed / 99 % 30 + 1);
    }
    if (seed / 3 % 100 < density) record->reservation = make_reservation(seed);
    return record;
}

static void toggle(Record* record, int64_t seed) {
    if (record->reservation) {
        record->stock = record->stock + record->reservation->quantity;
        free(record->reservation);
        record->reservation = NULL;
    } else {
        record->reservation = make_reservation(seed);
    }
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

int main(int argument_count, char** arguments) {
    int32_t items = setting(argument_count, arguments, "--items", 200000);
    int32_t density = setting(argument_count, arguments, "--density", 50);
    int32_t passes = setting(argument_count, arguments, "--passes", 20);
    int32_t churn = setting(argument_count, arguments, "--churn", 2000);
    int64_t start = now_nanoseconds();
    Records records = {0};
    int64_t seed = 42;
    for (int32_t index = 0; index < items; index++) {
        seed = seed * 48271 % 2147483647;
        append(&records, make_record(seed, density));
    }
    int64_t made = now_nanoseconds();
    int64_t reserved = 0;
    int64_t late = 0;
    for (int32_t pass = 0; pass < passes; pass++) {
        for (int32_t turn = 0; turn < churn; turn++) {
            seed = seed * 48271 % 2147483647;
            toggle(records.items[seed / 7 % records.count], seed);
        }
        for (int32_t index = 0; index < records.count; index++) {
            Record* record = records.items[index];
            if (record->reservation) reserved = reserved + (int64_t)record->price * record->reservation->quantity;
        }
        for (int32_t index = 0; index < records.count; index++) {
            Record* record = records.items[index];
            if (record->supplier && record->reservation) {
                late = late + (int64_t)record->supplier->lead_days * record->reservation->quantity;
            }
        }
        for (int32_t index = 0; index < records.count; index++) {
            Record* record = records.items[index];
            if (record->reservation) record->reservation->age = record->reservation->age + 1;
        }
    }
    int64_t ages = 0;
    int64_t stock = 0;
    for (int32_t index = 0; index < records.count; index++) {
        Record* record = records.items[index];
        if (record->reservation) ages = ages + record->reservation->age;
        stock = stock + record->stock;
    }
    int64_t finished = now_nanoseconds();
    printf("reserved %lld late %lld ages %lld stock %lld\n", (long long)reserved, (long long)late, (long long)ages,
           (long long)stock);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld passes %lld\n", (long long)((made - start) / 1000), (long long)((finished - made) / 1000));
    return 0;
}
