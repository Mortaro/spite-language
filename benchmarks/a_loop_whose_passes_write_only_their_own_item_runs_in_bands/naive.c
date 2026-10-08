/* naive/ written in C the way it reads: 100 000 orbits, each a struct allocated on its own and kept in an array of
 * pointers, advanced one after the other on the one thread the program has, ten times. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Orbit {
    float angle;
    float speed;
    int32_t turns;
} Orbit;

static void orbit_advance(Orbit* orbit) {
    for (int32_t step = 0; step < 1000; step = step + 1) {
        orbit->angle = orbit->angle + orbit->speed * 0.001f;
        if (orbit->angle > 6.28318f) {
            orbit->angle = orbit->angle - 6.28318f;
            orbit->turns = orbit->turns + 1;
        }
    }
}

int main(void) {
    int32_t count = 100000;
    Orbit** orbits = malloc(sizeof(Orbit*) * count);
    for (int32_t index = 0; index < count; index = index + 1) {
        Orbit* orbit = malloc(sizeof(Orbit));
        orbit->angle = 0.0f;
        orbit->speed = (float)(index % 17 + 1);
        orbit->turns = 0;
        orbits[index] = orbit;
    }
    int64_t start = now_nanoseconds();
    for (int32_t tick = 0; tick < 10; tick = tick + 1) {
        for (int32_t index = 0; index < count; index = index + 1) orbit_advance(orbits[index]);
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    int32_t turns = 0;
    for (int32_t index = 0; index < count; index = index + 1) turns = turns + orbits[index]->turns;
    printf("turns %d\n", turns);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    for (int32_t index = 0; index < count; index = index + 1) free(orbits[index]);
    free(orbits);
    return 0;
}
