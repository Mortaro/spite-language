/* naive/ written in C the way it reads: a struct per particle, a growable array of pointers to them, twenty rounds
 * of stepping each one and a sum of where they ended. A C struct holds its attributes and nothing else, which is
 * what a production Spite build's object holds too; only a --hot-reload build adds two words to each. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Particle {
    int32_t position;
    int32_t speed;
} Particle;

typedef struct ParticleList {
    Particle** items;
    int32_t count;
    int32_t capacity;
} ParticleList;

static ParticleList* particle_list_make(void) {
    ParticleList* list = malloc(sizeof(ParticleList));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void particle_list_append(ParticleList* list, Particle* particle) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(Particle*) * list->capacity);
    }
    list->items[list->count] = particle;
    list->count = list->count + 1;
}

static Particle* particle_make(int32_t position, int32_t speed) {
    Particle* particle = malloc(sizeof(Particle));
    particle->position = position;
    particle->speed = speed;
    return particle;
}

static void particle_step(Particle* particle) {
    particle->position = particle->position + particle->speed;
}

static void each_step(ParticleList* particles) {
    for (int32_t index = 0; index < particles->count; index = index + 1) {
        particle_step(particles->items[index]);
    }
}

static int32_t sum_position(ParticleList* particles) {
    int32_t total = 0;
    for (int32_t index = 0; index < particles->count; index = index + 1) {
        total = total + particles->items[index]->position;
    }
    return total;
}

static void move_all(ParticleList* particles) {
    for (int32_t round = 0; round < 20; round = round + 1) {
        each_step(particles);
    }
}

int main(void) {
    ParticleList* particles = particle_list_make();
    for (int32_t index = 0; index < 1000; index = index + 1) {
        Particle* particle = particle_make(index % 10, index % 3);
        particle_list_append(particles, particle);
    }
    move_all(particles);
    int32_t total = sum_position(particles);
    printf("position %d\n", total);
    for (int32_t index = 0; index < particles->count; index = index + 1) {
        free(particles->items[index]);
    }
    free(particles->items);
    free(particles);
    return 0;
}
