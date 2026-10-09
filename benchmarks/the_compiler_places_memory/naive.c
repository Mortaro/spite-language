/* naive/ written in C the way it reads: heap.allocate is malloc and heap.free is free, so every call of
 * sum_of_squares asks the C library for its 256 bytes and gives them back. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

static void write_value(int64_t* address, int32_t index, int64_t value) {
    address[index] = value;
}

static int64_t read_value(int64_t* address, int32_t index) {
    return address[index];
}

static int64_t sum_of_squares(int32_t count, int32_t offset) {
    int64_t* squares = malloc(count * 8);
    for (int32_t index = 0; index < count; index = index + 1) {
        int64_t value = index + offset;
        write_value(squares, index, value * value);
    }
    int64_t total = 0;
    for (int32_t index = 0; index < count; index = index + 1) {
        total = total + read_value(squares, index);
    }
    free(squares);
    return total;
}

static int64_t sum_rounds(int32_t rounds) {
    int64_t total = 0;
    for (int32_t round = 0; round < rounds; round = round + 1) {
        int64_t squares = sum_of_squares(32, round % 1000);
        total = total + squares;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int64_t total = sum_rounds(1000000);
    int64_t microseconds = microseconds_since(start);
    printf("total %lld\n", (long long)total);
    print_microseconds(microseconds);
    return 0;
}
