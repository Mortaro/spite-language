/* The same work tuned by hand: the three points written as constants where they are used, and the results as three
 * columns of plain values that one loop per round multiplies and adds, which the C compiler vectorises. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define RESULT_COUNT 200000

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t* golds = malloc(sizeof(int32_t) * RESULT_COUNT);
    int32_t* silvers = malloc(sizeof(int32_t) * RESULT_COUNT);
    int32_t* bronzes = malloc(sizeof(int32_t) * RESULT_COUNT);
    for (int32_t index = 0; index < RESULT_COUNT; index++) {
        golds[index] = index % 4;
        silvers[index] = index % 7;
        bronzes[index] = index % 11;
    }
    int64_t total = 0;
    for (int32_t round = 0; round < 40; round++) {
        __asm__ volatile("" : : "r"(golds), "r"(silvers), "r"(bronzes) : "memory");   /* every round reads the results again, as the program asks */
        int64_t round_total = 0;
        for (int32_t index = 0; index < RESULT_COUNT; index++) {
            round_total += golds[index] * 50 + silvers[index] * 20 + bronzes[index] * 5;
        }
        total += round_total;
    }
    int64_t microseconds = microseconds_since(start);
    printf("points %lld\n", (long long)total);
    print_microseconds(microseconds);
    free(golds);
    free(silvers);
    free(bronzes);
    return 0;
}
