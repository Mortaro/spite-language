/* The same work tuned by hand: each text written into one buffer reserved once at a size it cannot outgrow, the
 * digits of each number written straight into place, and the joined words written where the join puts them, with
 * no word made on its own. Both texts are still built in full, byte by byte. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

/* writes the digits of a number, and answers how many */
static inline size_t write_digits(uint32_t number, char* into) {
    char reversed[12];
    size_t count = 0;
    do {
        reversed[count++] = (char)('0' + number % 10);
        number /= 10;
    } while (number != 0);
    for (size_t index = 0; index < count; index++) {
        into[index] = reversed[count - 1 - index];
    }
    return count;
}

int main(void) {
    int64_t start = now_nanoseconds();
    char* built = malloc((size_t)3000000 * 13); /* "item ", at most 7 digits and "," */
    size_t built_length = 0;
    for (uint32_t index = 0; index < 3000000; index++) {
        memcpy(built + built_length, "item ", 5);
        built_length += 5;
        built_length += write_digits(index, built + built_length);
        built[built_length++] = ',';
    }
    char* joined = malloc((size_t)1000000 * 12 + 1); /* ", ", "word" and at most 6 digits */
    size_t joined_length = 0;
    for (uint32_t index = 0; index < 1000000; index++) {
        if (index > 0) {
            memcpy(joined + joined_length, ", ", 2);
            joined_length += 2;
        }
        memcpy(joined + joined_length, "word", 4);
        joined_length += 4;
        joined_length += write_digits(index, joined + joined_length);
    }
    joined[joined_length] = 0;
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("built %lld joined %lld\n", (long long)built_length, (long long)joined_length);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(built);
    free(joined);
    return 0;
}
