/* The same work tuned by hand: each name's length kept beside it, and each line written once into a buffer on the
 * stack at its final length. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

#define NAME_COUNT 1000

static char names[NAME_COUNT][32];
static int32_t lengths[NAME_COUNT];

static int32_t meetings(void) {
    int32_t total = 0;
    char line[96];
    for (int32_t index = 0; index + 1 < NAME_COUNT; index++) {
        char* at = line;
        memcpy(at, names[index], (size_t)lengths[index]);
        at += lengths[index];
        memcpy(at, " meets ", 7);
        at += 7;
        memcpy(at, names[index + 1], (size_t)lengths[index + 1]);
        at += lengths[index + 1];
        memcpy(at, " at the gate.", 13);
        at += 13;
        __asm__ volatile("" : : "r"(line) : "memory");   /* the line is made, as the program asks, not only measured */
        total += (int32_t)(at - line);
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    for (int32_t index = 0; index < NAME_COUNT; index++) {
        lengths[index] = snprintf(names[index], 32, "traveller number %d", index);
    }
    int64_t total = 0;
    for (int32_t round = 0; round < 1000; round++) {
        total += meetings();
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("characters %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
