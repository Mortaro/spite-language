/* The C twin of particles.spite: an array of particle structs, stepped in place by a plain loop. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* The same clock the Spite program reads: the time of the work goes to the error output and the answer to the
 * standard output, so run.sh compares the answers and times the work without the process's start. */
#ifdef _WIN32
#include <windows.h>
static int64_t now_nanoseconds(void) {
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    return (int64_t)((double)counter.QuadPart * 1000000000.0 / (double)frequency.QuadPart);
}
#else
#include <time.h>
static int64_t now_nanoseconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)now.tv_sec * 1000000000 + now.tv_nsec;
}
#endif

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
    int64_t start = now_nanoseconds();
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
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("height %lld spread %lld\n", (long long)(int64_t)height, (long long)(int64_t)spread);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(particles);
    return 0;
}
