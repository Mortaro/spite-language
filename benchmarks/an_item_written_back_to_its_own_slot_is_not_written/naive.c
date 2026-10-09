/* naive/ written in C the way it reads: a list of pointers to particles, each read into a name, changed through
 * it and stored back into its slot. C keeps no counts, so the store is one plain write of the same pointer. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Particle {
    int32_t left;
    int32_t speed;
} Particle;

typedef struct List {
    Particle** items;
    int32_t count;
    int32_t capacity;
} List;

static List* particles;

static List* list_make(void) {
    List* list = malloc(sizeof(List));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void list_append(List* list, Particle* item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(Particle*) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static Particle* particle_make(int32_t starting_speed) {
    Particle* particle = malloc(sizeof(Particle));
    particle->left = 0;
    particle->speed = starting_speed;
    return particle;
}

static void step(void) {
    for (int32_t index = 0; index < particles->count; index = index + 1) {
        Particle* particle = particles->items[index];
        particle->left = particle->left + particle->speed;
        particles->items[index] = particle;
    }
}

static int32_t sum_left(List* list) {
    int32_t total = 0;
    for (int32_t index = 0; index < list->count; index = index + 1) {
        total = total + list->items[index]->left;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    particles = list_make();
    for (int32_t index = 0; index < 200000; index = index + 1) {
        Particle* made = particle_make(index % 7 + 1);
        list_append(particles, made);
    }
    for (int32_t tick = 0; tick < 200; tick = tick + 1) {
        step();
    }
    int32_t total = sum_left(particles);
    int64_t microseconds = microseconds_since(start);
    printf("total %d\n", total);
    print_microseconds(microseconds);
    for (int32_t index = 0; index < particles->count; index = index + 1) {
        free(particles->items[index]);
    }
    free(particles->items);
    free(particles);
    return 0;
}
