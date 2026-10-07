/* The same work tuned by hand: the bytes in one array, one output buffer kept across the rounds, and each whole
 * group of three bytes encoded as four table lookups with no test, the last partial group apart. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define BYTE_COUNT 100000

static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int64_t encode(const uint8_t* bytes, int32_t count, char* output) {
    int32_t whole = count - count % 3;
    char* written = output;
    for (int32_t index = 0; index < whole; index += 3) {
        uint32_t group = (uint32_t)bytes[index] << 16 | (uint32_t)bytes[index + 1] << 8 | bytes[index + 2];
        written[0] = alphabet[group >> 18];
        written[1] = alphabet[group >> 12 & 63];
        written[2] = alphabet[group >> 6 & 63];
        written[3] = alphabet[group & 63];
        written += 4;
    }
    int32_t left = count - whole;
    if (left > 0) {
        uint32_t group = (uint32_t)bytes[whole] << 16;
        if (left > 1) group |= (uint32_t)bytes[whole + 1] << 8;
        written[0] = alphabet[group >> 18];
        written[1] = alphabet[group >> 12 & 63];
        written[2] = left > 1 ? alphabet[group >> 6 & 63] : '=';
        written[3] = '=';
        written += 4;
    }
    return written - output;
}

int main(void) {
    int64_t start = now_nanoseconds();
    uint8_t* bytes = malloc(BYTE_COUNT);
    for (int32_t index = 0; index < BYTE_COUNT; index++) bytes[index] = (uint8_t)(index * 7 % 256);
    char* output = malloc((BYTE_COUNT + 2) / 3 * 4 + 1);
    int64_t total = 0;
    for (int32_t round = 0; round < 100; round++) {
        bytes[round] = (uint8_t)round;
        int64_t length = encode(bytes, BYTE_COUNT, output);
        /* keeps every digit written, as the program makes the whole text */
        __asm__ volatile("" : : "r"(output) : "memory");
        total += length + output[round];
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(bytes);
    free(output);
    return 0;
}
