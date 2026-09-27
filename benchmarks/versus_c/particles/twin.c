/* The C twin of particles.spite: an array of particle structs, stepped in place by a plain loop. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Particle {
    float position_x;
    float position_y;
    float position_z;
    float velocity_x;
    float velocity_y;
    float velocity_z;
} Particle;

static void step(Particle* particle) {
    particle->velocity_y = particle->velocity_y - 9.8 * 0.016;
    particle->position_x = particle->position_x + particle->velocity_x * 0.016;
    particle->position_y = particle->position_y + particle->velocity_y * 0.016;
    particle->position_z = particle->position_z + particle->velocity_z * 0.016;
    if (particle->position_y < 0.0) {
        particle->position_y = -particle->position_y;
        particle->velocity_y = -particle->velocity_y * 0.8;
    }
}

int main(void) {
    int32_t count = 100000;
    Particle* particles = malloc(sizeof(Particle) * count);
    for (int32_t index = 0; index < count; index = index + 1) {
        Particle* particle = &particles[index];
        particle->position_x = index % 100;
        particle->position_y = index % 37 + 1;
        particle->position_z = index % 53;
        particle->velocity_x = index % 7 - 3;
        particle->velocity_y = index % 11;
        particle->velocity_z = index % 5 - 2;
    }
    for (int32_t tick = 0; tick < 300; tick = tick + 1) {
        for (int32_t index = 0; index < count; index = index + 1) {
            step(&particles[index]);
        }
    }
    float height = 0.0;
    float spread = 0.0;
    for (int32_t index = 0; index < count; index = index + 1) {
        height = height + particles[index].position_y;
        spread = spread + particles[index].position_x + particles[index].position_z;
    }
    printf("height %lld spread %lld\n", (long long)(int64_t)height, (long long)(int64_t)spread);
    free(particles);
    return 0;
}
