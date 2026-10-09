/* naive/ written in C the way a C programmer writes it: a struct per shape with a pointer to each optional part
 * (NULL when the shape has none), one malloc per object, a growable array of pointers, and the same loops: every
 * pass moves the shapes that have a velocity, adds up what the shapes with both parts draw, and fades every texture.
 * --items=N, --density=P (the percent of shapes with each part) and --passes=N, as naive/ reads them. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

typedef struct Texture {
    int32_t scale;
} Texture;

typedef struct Velocity {
    int32_t across;
    int32_t down;
} Velocity;

typedef struct Shape {
    int32_t x;
    int32_t y;
    Texture* texture;
    Velocity* velocity;
} Shape;

typedef struct Shapes {
    Shape** items;
    int32_t count;
    int32_t capacity;
} Shapes;

static void append(Shapes* shapes, Shape* shape) {
    if (shapes->count == shapes->capacity) {
        shapes->capacity = shapes->capacity ? shapes->capacity * 2 : 8;
        shapes->items = realloc(shapes->items, sizeof(Shape*) * shapes->capacity);
    }
    shapes->items[shapes->count] = shape;
    shapes->count = shapes->count + 1;
}

static Shape* make_shape(int64_t seed, int32_t density) {
    Shape* shape = malloc(sizeof(Shape));
    shape->x = (int32_t)(seed % 1024);
    shape->y = (int32_t)(seed / 1024 % 1024);
    shape->texture = NULL;
    shape->velocity = NULL;
    if (seed / 1048576 % 100 < density) {
        shape->texture = malloc(sizeof(Texture));
        shape->texture->scale = (int32_t)(seed / 251 % 64);
    }
    if (seed / 3 % 100 < density) {
        shape->velocity = malloc(sizeof(Velocity));
        shape->velocity->across = (int32_t)(seed % 16);
        shape->velocity->down = (int32_t)(seed / 16 % 16);
    }
    return shape;
}

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
    Shapes shapes = {0};
    int64_t seed = 42;
    for (int32_t index = 0; index < items; index++) {
        seed = seed * 48271 % 2147483647;
        append(&shapes, make_shape(seed, density));
    }
    int64_t made = now_nanoseconds();
    int64_t drawn = 0;
    for (int32_t pass = 0; pass < passes; pass++) {
        for (int32_t index = 0; index < shapes.count; index++) {
            Shape* shape = shapes.items[index];
            if (shape->velocity) {
                shape->x = (shape->x + shape->velocity->across) % 1024;
                shape->y = (shape->y + shape->velocity->down) % 1024;
            }
        }
        for (int32_t index = 0; index < shapes.count; index++) {
            Shape* shape = shapes.items[index];
            if (shape->texture && shape->velocity) {
                drawn = drawn + (int64_t)(shape->x + shape->y) * shape->texture->scale;
            }
        }
        for (int32_t index = 0; index < shapes.count; index++) {
            Shape* shape = shapes.items[index];
            if (shape->texture) {
                shape->texture->scale = (shape->texture->scale * 5 + 3) % 64;
            }
        }
    }
    int64_t positions = 0;
    int64_t scales = 0;
    for (int32_t index = 0; index < shapes.count; index++) {
        Shape* shape = shapes.items[index];
        positions = positions + shape->x + shape->y;
        if (shape->texture) scales = scales + shape->texture->scale;
    }
    int64_t finished = now_nanoseconds();
    printf("drawn %lld positions %lld scales %lld\n", (long long)drawn, (long long)positions, (long long)scales);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld passes %lld\n", (long long)((made - start) / 1000), (long long)((finished - made) / 1000));
    return 0;
}
