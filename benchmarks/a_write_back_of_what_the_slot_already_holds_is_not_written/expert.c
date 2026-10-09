/* The same work tuned by hand: only the word counts are ever read, so the pages are one array of numbers, nothing
 * is copied out or written back, and the hundred passes add one to every number, which the C compiler vectorises. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

int main(void) {
    int32_t count = 100000;
    int32_t* words = malloc(sizeof(int32_t) * count);
    for (int32_t index = 0; index < count; index = index + 1) words[index] = index % 7;
    int64_t start = now_nanoseconds();
    for (int32_t round = 0; round < 100; round = round + 1) {
        for (int32_t index = 0; index < count; index = index + 1) words[index] = words[index] + 1;
    }
    int32_t total = 0;
    for (int32_t index = 0; index < count; index = index + 1) total = total + words[index];
    int64_t microseconds = microseconds_since(start);
    printf("words %d\n", total);
    print_microseconds(microseconds);
    free(words);
    return 0;
}
