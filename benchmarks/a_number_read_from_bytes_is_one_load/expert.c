/* The same work tuned by hand: one plain block of records, each number one unaligned load and one byte swap, the
 * loop over whole records of a restrict pointer so the C compiler can vectorise the sum. Each round still decodes
 * every record. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

#define RECORD_COUNT 100000
#define RECORD_BYTES 16

static inline int32_t load_integer_big_endian(const uint8_t* at) {
    uint32_t bits;
    memcpy(&bits, at, sizeof(bits));
    return (int32_t)__builtin_bswap32(bits);
}

static inline int16_t load_short_big_endian(const uint8_t* at) {
    uint16_t bits;
    memcpy(&bits, at, sizeof(bits));
    return (int16_t)__builtin_bswap16(bits);
}

static inline void store_integer_big_endian(uint8_t* at, int32_t value) {
    uint32_t bits = __builtin_bswap32((uint32_t)value);
    memcpy(at, &bits, sizeof(bits));
}

static inline void store_short_big_endian(uint8_t* at, int16_t value) {
    uint16_t bits = __builtin_bswap16((uint16_t)value);
    memcpy(at, &bits, sizeof(bits));
}

static int64_t decode(const uint8_t* restrict records) {
    int64_t total = 0;
    for (int32_t record = 0; record < RECORD_COUNT; record++) {
        const uint8_t* at = records + (int64_t)record * RECORD_BYTES;
        total += (int64_t)load_integer_big_endian(at) + load_integer_big_endian(at + 4) +
                 load_integer_big_endian(at + 8) * load_short_big_endian(at + 12) + load_short_big_endian(at + 14);
    }
    return total;
}

int main(void) {
    uint8_t* records = malloc(RECORD_COUNT * RECORD_BYTES);
    for (int32_t index = 0; index < RECORD_COUNT; index++) {
        uint8_t* at = records + (int64_t)index * RECORD_BYTES;
        store_integer_big_endian(at, index);
        store_integer_big_endian(at + 4, index * 7 - 50000);
        store_integer_big_endian(at + 8, index % 1000);
        store_short_big_endian(at + 12, (int16_t)(index % 5));
        store_short_big_endian(at + 14, (int16_t)(index % 3));
    }
    int64_t start = now_nanoseconds();
    int64_t total = 0;
    for (int32_t round = 0; round < 300; round++) {
        total += decode(records) + round;
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(records);
    return 0;
}
