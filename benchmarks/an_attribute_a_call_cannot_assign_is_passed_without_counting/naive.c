/* naive/ written in C the way it reads: a meter struct holding a growable array of readings and a pointer to its
 * settings, which point to their limits; measure() hands the array and the limits to two small functions each time.
 * Nothing is counted, since C has no counts to write. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Readings {
    int32_t* items;
    int32_t count;
    int32_t capacity;
} Readings;

typedef struct Limits {
    int32_t offset;
} Limits;

typedef struct Settings {
    Limits* limits;
} Settings;

typedef struct Meter {
    Readings* readings;
    Settings* settings;
} Meter;

static void readings_append(Readings* self, int32_t value) {
    if (self->count == self->capacity) {
        self->capacity = self->capacity == 0 ? 4 : self->capacity * 2;
        self->items = realloc(self->items, (size_t)self->capacity * sizeof(int32_t));
    }
    self->items[self->count] = value;
    self->count = self->count + 1;
}

static int32_t reading_at(Meter* self, Readings* values, int32_t at) {
    (void)self;
    if (at < 0 || at >= values->count) abort();
    return values->items[at];
}

static int32_t offset_of(Meter* self, Limits* limits) {
    (void)self;
    return limits->offset;
}

static int32_t measure(Meter* self, int32_t at) {
    return reading_at(self, self->readings, at % 16) + offset_of(self, self->settings->limits);
}

static int32_t measure_all(Meter* self, int32_t count) {
    int32_t total = 0;
    for (int32_t index = 0; index < count; index = index + 1) total = total + measure(self, index);
    return total;
}

int main(void) {
    Meter* meter = malloc(sizeof(Meter));
    meter->readings = calloc(1, sizeof(Readings));
    meter->settings = malloc(sizeof(Settings));
    meter->settings->limits = malloc(sizeof(Limits));
    meter->settings->limits->offset = 3;
    for (int32_t index = 0; index < 16; index = index + 1) readings_append(meter->readings, index);
    int64_t start = now_nanoseconds();
    int32_t total = measure_all(meter, 10000000);
    int64_t microseconds = microseconds_since(start);
    printf("total %d\n", total);
    print_microseconds(microseconds);
    free(meter->settings->limits);
    free(meter->settings);
    free(meter->readings->items);
    free(meter->readings);
    free(meter);
    return 0;
}
