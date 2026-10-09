/* The same work tuned by hand: a bump allocator per round, one block asked of the C library for the round's
 * particles and its list of pointers, each particle a bump of the pointer, and the whole block given back at once. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define PARTICLE_COUNT 10000

typedef struct Particle {
    int32_t position;
    int32_t speed;
} Particle;

typedef struct Arena {
    char* block;
    size_t used;
} Arena;

static void* arena_allocate(Arena* arena, size_t bytes) {
    void* address = arena->block + arena->used;
    arena->used += (bytes + 7) & ~(size_t)7;
    return address;
}

static int32_t one_round(int32_t round) {
    Arena arena = {malloc(sizeof(Particle*) * PARTICLE_COUNT + sizeof(Particle) * PARTICLE_COUNT), 0};
    Particle** particles = arena_allocate(&arena, sizeof(Particle*) * PARTICLE_COUNT);
    for (int32_t index = 0; index < PARTICLE_COUNT; index++) {
        Particle* particle = arena_allocate(&arena, sizeof(Particle));
        particle->position = index + round;
        particle->speed = index % 5;
        particles[index] = particle;
    }
    int32_t sum = 0;
    for (int32_t index = 0; index < PARTICLE_COUNT; index++) sum += particles[index]->position + particles[index]->speed;
    free(arena.block);
    return sum;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int64_t total = 0;
    for (int32_t round = 0; round < 200; round++) total += one_round(round);
    int64_t microseconds = microseconds_since(start);
    printf("total %lld\n", (long long)total);
    print_microseconds(microseconds);
    return 0;
}
