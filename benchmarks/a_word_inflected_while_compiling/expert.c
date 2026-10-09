/* The same work tuned by hand: the plural written as the constant "cacti", and each label written once into a
 * buffer on the stack, its digits by hand. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../clock.h"

static int32_t write_label(char* line, int32_t count) {
    if (count == 1) {
        memcpy(line, "1 cactus", 8);
        return 8;
    }
    char digits[12];
    int32_t digit_count = 0;
    int32_t left = count;
    do {
        digits[digit_count++] = (char)('0' + left % 10);
        left /= 10;
    } while (left > 0);
    int32_t length = 0;
    while (digit_count > 0) line[length++] = digits[--digit_count];
    memcpy(line + length, " cacti", 6);
    return length + 6;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int64_t total = 0;
    char line[32];
    for (int32_t index = 0; index < 1000000; index++) {
        int32_t length = write_label(line, index % 50);
        __asm__ volatile("" : : "r"(line) : "memory");   /* the label is made, as the program asks, not only measured */
        total += length;
    }
    int64_t microseconds = microseconds_since(start);
    printf("characters %lld\n", (long long)total);
    print_microseconds(microseconds);
    return 0;
}
