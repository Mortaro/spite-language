/* The same work tuned by hand: each line is written into one buffer on the stack, its numbers as digits straight
 * into it, and nothing is allocated. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../clock.h"

static char* digits_of(char* at, int32_t value) {
    char reversed[12];
    int32_t count = 0;
    do {
        reversed[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value > 0);
    while (count > 0) *at++ = reversed[--count];
    return at;
}

static int32_t lines(int32_t count, int32_t round) {
    int32_t total = 0;
    char line[48];
    for (int32_t index = 0; index < count; index++) {
        char* at = line;
        memcpy(at, "line ", 5);
        at = digits_of(at + 5, index);
        memcpy(at, " of ", 4);
        at = digits_of(at + 4, round);
        *at++ = ';';
        __asm__ volatile("" : : "r"(line) : "memory");   /* the line is made, as the program asks, not only measured */
        total += (int32_t)(at - line);
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int64_t total = 0;
    for (int32_t round = 0; round < 20; round++) {
        total += lines(100000, round);
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("characters %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
