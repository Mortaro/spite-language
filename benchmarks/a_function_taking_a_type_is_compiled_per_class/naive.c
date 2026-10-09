/* naive/ written in C the way it reads: a function that takes any value of the shape Measured takes a tagged value
 * (which class it holds, and the value or a pointer to the object), and a switch on the tag finds that class's
 * to_double. A struct and a malloc per object. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Crate {
    float width;
    float height;
    float depth;
} Crate;

typedef struct Ball {
    float radius;
} Ball;

typedef enum MeasuredClass {
    MEASURED_INTEGER,
    MEASURED_FLOAT,
    MEASURED_CRATE,
    MEASURED_BALL,
} MeasuredClass;

typedef struct Measured {
    MeasuredClass class;
    union {
        int32_t integer;
        float decimal;
        void* object;
    } value;
} Measured;

static Crate* crate_make(float width, float height, float depth) {
    Crate* crate = malloc(sizeof(Crate));
    crate->width = width;
    crate->height = height;
    crate->depth = depth;
    return crate;
}

static Ball* ball_make(float radius) {
    Ball* ball = malloc(sizeof(Ball));
    ball->radius = radius;
    return ball;
}

static double crate_to_double(Crate* crate) {
    return (double)(crate->width * crate->height * crate->depth);
}

static double ball_to_double(Ball* ball) {
    return (double)(ball->radius + ball->radius);
}

static double measured_to_double(Measured value) {
    switch (value.class) {
        case MEASURED_INTEGER: return (double)value.value.integer;
        case MEASURED_FLOAT: return (double)value.value.decimal;
        case MEASURED_CRATE: return crate_to_double(value.value.object);
        case MEASURED_BALL: return ball_to_double(value.value.object);
    }
    abort();
}

static double doubled(Measured value) {
    double measure = measured_to_double(value);
    return measure + measure;
}

static double measure_all(int32_t steps) {
    double total = 0.0;
    Crate* crate = crate_make(1.0f, 2.0f, 3.0f);
    Ball* ball = ball_make(1.0f);
    for (int32_t step = 0; step < steps; step = step + 1) {
        Measured count = {MEASURED_INTEGER, {.integer = step % 10}};
        total = total + doubled(count);
        Measured share = {MEASURED_FLOAT, {.decimal = (float)(step % 4)}};
        total = total + doubled(share);
        crate->width = (float)(step % 7);
        Measured crate_value = {MEASURED_CRATE, {.object = crate}};
        total = total + doubled(crate_value);
        ball->radius = (float)(step % 5);
        Measured ball_value = {MEASURED_BALL, {.object = ball}};
        total = total + doubled(ball_value);
    }
    free(crate);
    free(ball);
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    double total = measure_all(20000000);
    int64_t microseconds = microseconds_since(start);
    int64_t whole = (int64_t)total;
    printf("total %lld\n", (long long)whole);
    print_microseconds(microseconds);
    return 0;
}
