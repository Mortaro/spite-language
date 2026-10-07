/* naive/ written in C the way it reads without an arena: a struct per class, malloc per particle, a growable array
 * of pointers for the list, and every particle freed one by one at the end of its round. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Particle {
    int32_t position;
    int32_t speed;
} Particle;

typedef struct List {
    Particle** items;
    int32_t count;
    int32_t capacity;
} List;

static Particle* particle_make(int32_t position, int32_t speed) {
    Particle* particle = malloc(sizeof(Particle));
    particle->position = position;
    particle->speed = speed;
    return particle;
}

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

static void list_free(List* list) {
    for (int32_t index = 0; index < list->count; index = index + 1) free(list->items[index]);
    free(list->items);
    free(list);
}

static int32_t sum_position(List* list) {
    int32_t total = 0;
    for (int32_t index = 0; index < list->count; index = index + 1) total = total + list->items[index]->position;
    return total;
}

static int32_t sum_speed(List* list) {
    int32_t total = 0;
    for (int32_t index = 0; index < list->count; index = index + 1) total = total + list->items[index]->speed;
    return total;
}

static int32_t one_round(int32_t round) {
    List* particles = list_make();
    for (int32_t index = 0; index < 10000; index = index + 1) {
        Particle* particle = particle_make(index + round, index % 5);
        list_append(particles, particle);
    }
    int32_t sum = sum_position(particles) + sum_speed(particles);
    list_free(particles);
    return sum;
}

static int64_t all_rounds(int32_t rounds) {
    int64_t total = 0;
    for (int32_t round = 0; round < rounds; round = round + 1) {
        int32_t sum = one_round(round);
        total = total + sum;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int64_t total = all_rounds(200);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
