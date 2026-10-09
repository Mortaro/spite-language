/* The smallest step from what Spite writes today: each optional part lives inside the shape that owns it, with a
 * byte saying which parts are there, instead of being an object of its own behind a pointer. The shapes still come
 * from a pool of blocks side by side and the list still holds pointers to them, and the loops are naive/'s, with the
 * same tests; only the hop from a shape to its parts, and their allocations, are gone.
 * --items=N, --density=P and --passes=N, as naive/ reads them. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

enum { has_texture = 1, has_velocity = 2, block = 4096 };

typedef struct Shape {
    int32_t x;
    int32_t y;
    int32_t scale;
    int32_t across;
    int32_t down;
    uint8_t parts;
} Shape;

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
    Shape** shapes = malloc(sizeof(Shape*) * capacity);
    Shape* pool = NULL;
    int32_t left_in_block = 0;
    int64_t seed = 42;
    for (int32_t index = 0; index < items; index++) {
        seed = seed * 48271 % 2147483647;
        if (left_in_block == 0) {
            pool = malloc(sizeof(Shape) * block);
            left_in_block = block;
        }
        Shape* shape = pool++;
        left_in_block--;
        shape->x = (int32_t)(seed % 1024);
        shape->y = (int32_t)(seed / 1024 % 1024);
        shape->parts = 0;
        if (seed / 1048576 % 100 < density) {
            shape->parts |= has_texture;
            shape->scale = (int32_t)(seed / 251 % 64);
        }
        if (seed / 3 % 100 < density) {
            shape->parts |= has_velocity;
            shape->across = (int32_t)(seed % 16);
            shape->down = (int32_t)(seed / 16 % 16);
        }
        if (index == capacity) {
            capacity = capacity * 2;
            shapes = realloc(shapes, sizeof(Shape*) * capacity);
        }
        shapes[index] = shape;
    }
    int64_t made = now_nanoseconds();
    int64_t drawn = 0;
    for (int32_t pass = 0; pass < passes; pass++) {
        for (int32_t index = 0; index < items; index++) {
            Shape* shape = shapes[index];
            if (shape->parts & has_velocity) {
                shape->x = (shape->x + shape->across) % 1024;
                shape->y = (shape->y + shape->down) % 1024;
            }
        }
        for (int32_t index = 0; index < items; index++) {
            Shape* shape = shapes[index];
            if ((shape->parts & (has_texture | has_velocity)) == (has_texture | has_velocity)) {
                drawn = drawn + (int64_t)(shape->x + shape->y) * shape->scale;
            }
        }
        for (int32_t index = 0; index < items; index++) {
            Shape* shape = shapes[index];
            if (shape->parts & has_texture) shape->scale = (shape->scale * 5 + 3) % 64;
        }
    }
    int64_t positions = 0;
    int64_t scales = 0;
    for (int32_t index = 0; index < items; index++) {
        Shape* shape = shapes[index];
        positions = positions + shape->x + shape->y;
        if (shape->parts & has_texture) scales = scales + shape->scale;
    }
    int64_t finished = now_nanoseconds();
    printf("drawn %lld positions %lld scales %lld\n", (long long)drawn, (long long)positions, (long long)scales);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld passes %lld\n", (long long)((made - start) / 1000), (long long)((finished - made) / 1000));
    return 0;
}
