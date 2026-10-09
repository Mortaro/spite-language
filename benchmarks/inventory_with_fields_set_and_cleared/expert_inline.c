/* The smallest step from what Spite writes today: each optional part lives inside the record that owns it, with a
 * byte saying which parts are there, instead of being an object of its own behind a pointer. Setting a reservation
 * writes its fields and sets a bit, clearing one clears the bit: nothing is allocated or freed. The records still
 * come from a pool of blocks side by side and the list still holds pointers to them, and the loops are naive/'s,
 * with the same tests.
 * --items=N, --density=P, --passes=N and --churn=N, as naive/ reads them. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

enum { has_supplier = 1, has_reservation = 2, block = 4096 };

typedef struct Record {
    int32_t price;
    int32_t stock;
    int32_t lead_days;
    int32_t quantity;
    int32_t age;
    uint8_t parts;
} Record;

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
    int32_t capacity = 64;
    Record** records = malloc(sizeof(Record*) * capacity);
    Record* pool = NULL;
    int32_t left_in_block = 0;
    int64_t seed = 42;
    for (int32_t index = 0; index < items; index++) {
        seed = seed * 48271 % 2147483647;
        if (left_in_block == 0) {
            pool = malloc(sizeof(Record) * block);
            left_in_block = block;
        }
        Record* record = pool++;
        left_in_block--;
        record->price = (int32_t)(seed % 1000 + 1);
        record->stock = (int32_t)(seed / 1000 % 100);
        record->parts = 0;
        if (seed / 1048576 % 100 < density) {
            record->parts |= has_supplier;
            record->lead_days = (int32_t)(seed / 99 % 30 + 1);
        }
        if (seed / 3 % 100 < density) {
            record->parts |= has_reservation;
            record->quantity = (int32_t)(seed % 50 + 1);
            record->age = 0;
        }
        if (index == capacity) {
            capacity = capacity * 2;
            records = realloc(records, sizeof(Record*) * capacity);
        }
        records[index] = record;
    }
    int64_t made = now_nanoseconds();
    int64_t reserved = 0;
    int64_t late = 0;
    for (int32_t pass = 0; pass < passes; pass++) {
        for (int32_t turn = 0; turn < churn; turn++) {
            seed = seed * 48271 % 2147483647;
            Record* record = records[seed / 7 % items];
            if (record->parts & has_reservation) {
                record->stock = record->stock + record->quantity;
            } else {
                record->quantity = (int32_t)(seed % 50 + 1);
                record->age = 0;
            }
            record->parts ^= has_reservation;
        }
        for (int32_t index = 0; index < items; index++) {
            Record* record = records[index];
            if (record->parts & has_reservation) reserved = reserved + (int64_t)record->price * record->quantity;
        }
        for (int32_t index = 0; index < items; index++) {
            Record* record = records[index];
            if ((record->parts & (has_supplier | has_reservation)) == (has_supplier | has_reservation)) {
                late = late + (int64_t)record->lead_days * record->quantity;
            }
        }
        for (int32_t index = 0; index < items; index++) {
            Record* record = records[index];
            if (record->parts & has_reservation) record->age = record->age + 1;
        }
    }
    int64_t ages = 0;
    int64_t stock = 0;
    for (int32_t index = 0; index < items; index++) {
        Record* record = records[index];
        if (record->parts & has_reservation) ages = ages + record->age;
        stock = stock + record->stock;
    }
    int64_t finished = now_nanoseconds();
    printf("reserved %lld late %lld ages %lld stock %lld\n", (long long)reserved, (long long)late, (long long)ages,
           (long long)stock);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld passes %lld\n", (long long)((made - start) / 1000), (long long)((finished - made) / 1000));
    return 0;
}
