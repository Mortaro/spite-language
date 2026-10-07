/* The same work tuned by hand: the schema written as the constant it is, the headers in one plain array, and each
 * round one branchless count over it the C compiler vectorises. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

#define HEADER_COUNT 200000
#define READING_SCHEMA 4647632506827894408LL   /* Reading{sensor:Integer;level:Float;label:String} */

static int32_t sent_and_read_back(int32_t sensor) {
    uint8_t bytes[12];
    float level = 0.0f;
    int32_t length = 0;
    memcpy(bytes, &sensor, 4);
    memcpy(bytes + 4, &level, 4);
    memcpy(bytes + 8, &length, 4);
    __asm__ volatile("" : : "r"(bytes) : "memory");   /* the bytes are written, as the program asks */
    int32_t read = 0;
    memcpy(&read, bytes, 4);
    return read;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int64_t* headers = malloc(sizeof(int64_t) * HEADER_COUNT);
    for (int32_t index = 0; index < HEADER_COUNT; index++) {
        headers[index] = index % 10 == 0 ? index : READING_SCHEMA;
    }
    int32_t accepted = 0;
    for (int32_t round = 0; round < 10; round++) {
        __asm__ volatile("" : : "r"(headers) : "memory");   /* every round reads the headers again, as the program asks */
        int32_t count = 0;
        for (int32_t index = 0; index < HEADER_COUNT; index++) {
            count += headers[index] == READING_SCHEMA;
        }
        accepted += count;
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    int32_t sensor = sent_and_read_back(7);
    printf("accepted %d sensor %d\n", accepted, sensor);
    printf("schema %lld\n", (long long)READING_SCHEMA);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(headers);
    return 0;
}
