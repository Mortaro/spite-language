/* The same work tuned by hand: the orders as two columns, and the buckets a counting sort into one array of amounts
 * with a start per customer, built again each round into the same memory, so a question reads two neighbouring
 * starts and a run of amounts side by side. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

#define ORDERS 400000
#define CUSTOMERS 65536

int main(void) {
    int32_t* customers = malloc(sizeof(int32_t) * ORDERS);
    int32_t* amounts = malloc(sizeof(int32_t) * ORDERS);
    int64_t seed = 12345;
    for (int32_t index = 0; index < ORDERS; index++) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        customers[index] = (int32_t)(seed % CUSTOMERS);
        amounts[index] = (int32_t)(seed % 1000);
    }
    int32_t* starts = malloc(sizeof(int32_t) * (CUSTOMERS + 1));
    int32_t* cursors = malloc(sizeof(int32_t) * CUSTOMERS);
    int32_t* grouped = malloc(sizeof(int32_t) * ORDERS);
    int64_t start = now_nanoseconds();
    int64_t total = 0;
    for (int32_t round = 0; round < 10; round++) {
        memset(starts, 0, sizeof(int32_t) * (CUSTOMERS + 1));
        for (int32_t index = 0; index < ORDERS; index++) starts[customers[index] + 1]++;
        for (int32_t customer = 0; customer < CUSTOMERS; customer++) starts[customer + 1] += starts[customer];
        memcpy(cursors, starts, sizeof(int32_t) * CUSTOMERS);
        for (int32_t index = 0; index < ORDERS; index++) grouped[cursors[customers[index]]++] = amounts[index];
        int64_t asked = 0;
        int32_t customer = round;
        for (int32_t query = 0; query < 400000; query++) {
            customer = (customer * 7919 + 13) % CUSTOMERS;
            for (int32_t at = starts[customer]; at < starts[customer + 1]; at++) asked += grouped[at];
        }
        total += asked;
    }
    int64_t microseconds = microseconds_since(start);
    printf("total %lld\n", (long long)total);
    print_microseconds(microseconds);
    free(customers);
    free(amounts);
    free(starts);
    free(cursors);
    free(grouped);
    return 0;
}
