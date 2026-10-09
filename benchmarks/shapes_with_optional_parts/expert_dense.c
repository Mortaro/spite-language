/* The shapes kept in the order they were made, as columns over all shapes, every optional part included, with a
 * byte per shape saying which parts it has (a column with holes): what the columns of the data-oriented study give
 * when parts are optional. A loop that needs a part walks every shape and tests its byte; the C compiler turns the
 * tests into masked selects, so the loop is vector code with no branch, but it reads every shape's columns whether
 * the shape has the part or not.
 * --items=N, --density=P and --passes=N, as naive/ reads them. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

enum { has_texture = 1, has_velocity = 2 };

static int32_t setting(int argument_count, char** arguments, const char* name, int32_t otherwise) {
    size_t length = strlen(name);
    for (int index = 1; index < argument_count; index++) {
        if (strncmp(arguments[index], name, length) == 0 && arguments[index][length] == '=') {
            return atoi(arguments[index] + length + 1);
        }
    }
    return otherwise;
}

int main(int argument_count, char** arguments) {
    int32_t items = setting(argument_count, arguments, "--items", 200000);
    int32_t density = setting(argument_count, arguments, "--density", 50);
    int32_t passes = setting(argument_count, arguments, "--passes", 20);
    int64_t start = now_nanoseconds();
    int32_t capacity = 64;
    int32_t* restrict x = malloc(sizeof(int32_t) * capacity);
    int32_t* restrict y = malloc(sizeof(int32_t) * capacity);
    int32_t* restrict scale = malloc(sizeof(int32_t) * capacity);
    int32_t* restrict across = malloc(sizeof(int32_t) * capacity);
    int32_t* restrict down = malloc(sizeof(int32_t) * capacity);
    uint8_t* restrict parts = malloc(capacity);
    int64_t seed = 42;
    for (int32_t index = 0; index < items; index++) {
        seed = seed * 48271 % 2147483647;
        if (index == capacity) {
            capacity = capacity * 2;
            x = realloc(x, sizeof(int32_t) * capacity);
            y = realloc(y, sizeof(int32_t) * capacity);
            scale = realloc(scale, sizeof(int32_t) * capacity);
            across = realloc(across, sizeof(int32_t) * capacity);
            down = realloc(down, sizeof(int32_t) * capacity);
            parts = realloc(parts, capacity);
        }
        x[index] = (int32_t)(seed % 1024);
        y[index] = (int32_t)(seed / 1024 % 1024);
        uint8_t kind = 0;
        scale[index] = 0;
        across[index] = 0;
        down[index] = 0;
        if (seed / 1048576 % 100 < density) {
            kind |= has_texture;
            scale[index] = (int32_t)(seed / 251 % 64);
        }
        if (seed / 3 % 100 < density) {
            kind |= has_velocity;
            across[index] = (int32_t)(seed % 16);
            down[index] = (int32_t)(seed / 16 % 16);
        }
        parts[index] = kind;
    }
    int64_t made = now_nanoseconds();
    int64_t drawn = 0;
    for (int32_t pass = 0; pass < passes; pass++) {
        /* an absent velocity is stored as zero, so moving by it changes nothing: no test at all */
        for (int32_t index = 0; index < items; index++) {
            x[index] = (x[index] + across[index]) & 1023;
            y[index] = (y[index] + down[index]) & 1023;
        }
        for (int32_t index = 0; index < items; index++) {
            int64_t term = (int64_t)(x[index] + y[index]) * scale[index];
            drawn = drawn + (term & -(int64_t)((parts[index] >> 1) & 1));   /* masks, not branches: parts are there at random */
        }
        for (int32_t index = 0; index < items; index++) {
            int32_t faded = (scale[index] * 5 + 3) & 63;
            int32_t mask = -(int32_t)(parts[index] & has_texture);
            scale[index] = (faded & mask) | (scale[index] & ~mask);
        }
    }
    int64_t positions = 0;
    int64_t scales = 0;
    for (int32_t index = 0; index < items; index++) {
        positions = positions + x[index] + y[index];
        scales = scales + scale[index];
    }
    int64_t finished = now_nanoseconds();
    printf("drawn %lld positions %lld scales %lld\n", (long long)drawn, (long long)positions, (long long)scales);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld passes %lld\n", (long long)((made - start) / 1000), (long long)((finished - made) / 1000));
    return 0;
}
