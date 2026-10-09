/* The same work tuned by hand: only the size of each bucket is ever asked, so each round counts the orders per
 * customer into one array of counts, cleared in place, and takes the largest; no bucket is built at all. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

#define ORDERS 200000
#define CUSTOMERS 32768

int main(void) {
    int32_t* customers = malloc(sizeof(int32_t) * ORDERS);
    int64_t seed = 777;
    for (int32_t index = 0; index < ORDERS; index++) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        customers[index] = (int32_t)(seed % CUSTOMERS);
    }
    int32_t* counts = malloc(sizeof(int32_t) * CUSTOMERS);
    int64_t start = now_nanoseconds();
    int64_t total = 0;
    for (int32_t round = 0; round < 40; round++) {
        memset(counts, 0, sizeof(int32_t) * CUSTOMERS);
        for (int32_t index = 0; index < ORDERS; index++) counts[(customers[index] + round) % CUSTOMERS]++;
        int32_t most = 0;
        for (int32_t customer = 0; customer < CUSTOMERS; customer++) {
            if (counts[customer] > most) most = counts[customer];
        }
        total += most;
    }
    int64_t microseconds = microseconds_since(start);
    printf("total %lld\n", (long long)total);
    print_microseconds(microseconds);
    free(customers);
    free(counts);
    return 0;
}
