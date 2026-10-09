/* The shapes kept in the order they were made, with every optional part in a sparse set of its own: the positions
 * are columns over all shapes, and each part is a dense array of the part's values with the shape that owns each
 * row, plus a map from shape to row (-1 when the shape has none). A loop that needs one part walks that part's set
 * and reaches the positions by the owner (a gather and a scatter); a loop that needs two walks the smaller set and
 * probes the other's map. Adding or removing a part never moves anything else.
 * --items=N, --density=P and --passes=N, as naive/ reads them. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

typedef struct Set {
    int32_t count;
    int32_t capacity;
    int32_t* restrict owner;
    int32_t* restrict first;
    int32_t* restrict second;
    int32_t* restrict row_of;
} Set;

static int32_t setting(int argument_count, char** arguments, const char* name, int32_t otherwise) {
    size_t length = strlen(name);
    for (int index = 1; index < argument_count; index++) {
        if (strncmp(arguments[index], name, length) == 0 && arguments[index][length] == '=') {
            return atoi(arguments[index] + length + 1);
        }
    }
    return otherwise;
}

static void add(Set* set, int32_t shape, int32_t first, int32_t second) {
    if (set->count == set->capacity) {
        set->capacity = set->capacity ? set->capacity * 2 : 64;
        set->owner = realloc(set->owner, sizeof(int32_t) * set->capacity);
        set->first = realloc(set->first, sizeof(int32_t) * set->capacity);
        set->second = realloc(set->second, sizeof(int32_t) * set->capacity);
    }
    set->owner[set->count] = shape;
    set->first[set->count] = first;
    set->second[set->count] = second;
    set->row_of[shape] = set->count;
    set->count++;
}

int main(int argument_count, char** arguments) {
    int32_t items = setting(argument_count, arguments, "--items", 200000);
    int32_t density = setting(argument_count, arguments, "--density", 50);
    int32_t passes = setting(argument_count, arguments, "--passes", 20);
    int64_t start = now_nanoseconds();
    int32_t capacity = 64;
    int32_t* x = malloc(sizeof(int32_t) * capacity);
    int32_t* y = malloc(sizeof(int32_t) * capacity);
    Set textures = {0};
    Set velocities = {0};
    textures.row_of = malloc(sizeof(int32_t) * capacity);
    velocities.row_of = malloc(sizeof(int32_t) * capacity);
    int64_t seed = 42;
    for (int32_t index = 0; index < items; index++) {
        seed = seed * 48271 % 2147483647;
        if (index == capacity) {
            capacity = capacity * 2;
            x = realloc(x, sizeof(int32_t) * capacity);
            y = realloc(y, sizeof(int32_t) * capacity);
            textures.row_of = realloc(textures.row_of, sizeof(int32_t) * capacity);
            velocities.row_of = realloc(velocities.row_of, sizeof(int32_t) * capacity);
        }
        x[index] = (int32_t)(seed % 1024);
        y[index] = (int32_t)(seed / 1024 % 1024);
        textures.row_of[index] = -1;
        velocities.row_of[index] = -1;
        if (seed / 1048576 % 100 < density) add(&textures, index, (int32_t)(seed / 251 % 64), 0);
        if (seed / 3 % 100 < density) add(&velocities, index, (int32_t)(seed % 16), (int32_t)(seed / 16 % 16));
    }
    int64_t made = now_nanoseconds();
    int64_t drawn = 0;
    for (int32_t pass = 0; pass < passes; pass++) {
        for (int32_t row = 0; row < velocities.count; row++) {
            int32_t shape = velocities.owner[row];
            x[shape] = (x[shape] + velocities.first[row]) & 1023;
            y[shape] = (y[shape] + velocities.second[row]) & 1023;
        }
        if (textures.count <= velocities.count) {
            for (int32_t row = 0; row < textures.count; row++) {
                int32_t shape = textures.owner[row];
                if (velocities.row_of[shape] >= 0) drawn = drawn + (int64_t)(x[shape] + y[shape]) * textures.first[row];
            }
        } else {
            for (int32_t row = 0; row < velocities.count; row++) {
                int32_t shape = velocities.owner[row];
                int32_t texture = textures.row_of[shape];
                if (texture >= 0) drawn = drawn + (int64_t)(x[shape] + y[shape]) * textures.first[texture];
            }
        }
        for (int32_t row = 0; row < textures.count; row++) {
            textures.first[row] = (textures.first[row] * 5 + 3) & 63;
        }
    }
    int64_t positions = 0;
    int64_t scales = 0;
    for (int32_t index = 0; index < items; index++) positions = positions + x[index] + y[index];
    for (int32_t row = 0; row < textures.count; row++) scales = scales + textures.first[row];
    int64_t finished = now_nanoseconds();
    printf("drawn %lld positions %lld scales %lld\n", (long long)drawn, (long long)positions, (long long)scales);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld passes %lld\n", (long long)((made - start) / 1000), (long long)((finished - made) / 1000));
    return 0;
}
