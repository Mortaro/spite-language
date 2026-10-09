/* The records kept in the order they were made, as columns over all records, every optional part included, with a
 * byte per record saying which parts it has (a column with holes). Setting or clearing a reservation writes one
 * record's columns and flips a bit; a loop that needs a part walks every record and tests its byte, which the C
 * compiler turns into a masked select.
 * --items=N, --density=P, --passes=N and --churn=N, as naive/ reads them. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

enum { has_supplier = 1, has_reservation = 2 };

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
    int32_t* restrict price = malloc(sizeof(int32_t) * capacity);
    int32_t* restrict stock = malloc(sizeof(int32_t) * capacity);
    int32_t* restrict lead_days = malloc(sizeof(int32_t) * capacity);
    int32_t* restrict quantity = malloc(sizeof(int32_t) * capacity);
    int32_t* restrict age = malloc(sizeof(int32_t) * capacity);
    uint8_t* restrict parts = malloc(capacity);
    int64_t seed = 42;
    for (int32_t index = 0; index < items; index++) {
        seed = seed * 48271 % 2147483647;
        if (index == capacity) {
            capacity = capacity * 2;
            price = realloc(price, sizeof(int32_t) * capacity);
            stock = realloc(stock, sizeof(int32_t) * capacity);
            lead_days = realloc(lead_days, sizeof(int32_t) * capacity);
            quantity = realloc(quantity, sizeof(int32_t) * capacity);
            age = realloc(age, sizeof(int32_t) * capacity);
            parts = realloc(parts, capacity);
        }
        price[index] = (int32_t)(seed % 1000 + 1);
        stock[index] = (int32_t)(seed / 1000 % 100);
        uint8_t kind = 0;
        lead_days[index] = 0;
        quantity[index] = 0;
        age[index] = 0;
        if (seed / 1048576 % 100 < density) {
            kind |= has_supplier;
            lead_days[index] = (int32_t)(seed / 99 % 30 + 1);
        }
        if (seed / 3 % 100 < density) {
            kind |= has_reservation;
            quantity[index] = (int32_t)(seed % 50 + 1);
        }
        parts[index] = kind;
    }
    int64_t made = now_nanoseconds();
    int64_t reserved = 0;
    int64_t late = 0;
    for (int32_t pass = 0; pass < passes; pass++) {
        for (int32_t turn = 0; turn < churn; turn++) {
            seed = seed * 48271 % 2147483647;
            int32_t record = (int32_t)(seed / 7 % items);
            if (parts[record] & has_reservation) {
                stock[record] = stock[record] + quantity[record];
                quantity[record] = 0;
                age[record] = 0;
            } else {
                quantity[record] = (int32_t)(seed % 50 + 1);
                age[record] = 0;
            }
            parts[record] ^= has_reservation;
        }
        /* an absent reservation is stored as a quantity of zero, so it adds nothing: no test */
        for (int32_t index = 0; index < items; index++) reserved = reserved + (int64_t)price[index] * quantity[index];
        /* and an absent supplier as a lead of zero */
        for (int32_t index = 0; index < items; index++) late = late + (int64_t)lead_days[index] * quantity[index];
        for (int32_t index = 0; index < items; index++) age[index] = age[index] + ((parts[index] & has_reservation) ? 1 : 0);
    }
    int64_t ages = 0;
    int64_t total_stock = 0;
    for (int32_t index = 0; index < items; index++) {
        ages = ages + age[index];
        total_stock = total_stock + stock[index];
    }
    int64_t finished = now_nanoseconds();
    printf("reserved %lld late %lld ages %lld stock %lld\n", (long long)reserved, (long long)late, (long long)ages,
           (long long)total_stock);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld passes %lld\n", (long long)((made - start) / 1000), (long long)((finished - made) / 1000));
    return 0;
}
