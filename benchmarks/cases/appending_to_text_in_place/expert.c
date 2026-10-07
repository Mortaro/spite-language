/* The same work tuned by hand: one buffer per text, grown by doubling, each piece copied once onto its end. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

static char* words(int32_t count, int64_t* length) {
    int64_t capacity = 64;
    int64_t used = 0;
    char* bytes = malloc((size_t)capacity);
    for (int32_t index = 0; index < count; index++) {
        if (used + 5 > capacity) {
            capacity *= 2;
            bytes = realloc(bytes, (size_t)capacity);
        }
        memcpy(bytes + used, "word ", 5);
        used += 5;
    }
    *length = used;
    return bytes;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t total = 0;
    for (int32_t round = 0; round < 8; round++) {
        int64_t length;
        char* text = words(10000, &length);
        total += (int32_t)length;
        free(text);
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("characters %d\n", total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
