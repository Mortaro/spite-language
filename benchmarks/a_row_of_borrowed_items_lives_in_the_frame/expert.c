/* The same work tuned by hand: no rows at all, the components as six columns of numbers, and each tick one loop
 * that moves and heals every entity, which the C compiler vectorises. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define ENTITY_COUNT 100000

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t* lefts = calloc(ENTITY_COUNT, sizeof(int32_t));
    int32_t* tops = calloc(ENTITY_COUNT, sizeof(int32_t));
    int32_t* acrosses = malloc(sizeof(int32_t) * ENTITY_COUNT);
    int32_t* downs = malloc(sizeof(int32_t) * ENTITY_COUNT);
    int32_t* amounts = calloc(ENTITY_COUNT, sizeof(int32_t));
    int32_t* per_ticks = malloc(sizeof(int32_t) * ENTITY_COUNT);
    for (int32_t entity = 0; entity < ENTITY_COUNT; entity++) {
        acrosses[entity] = entity % 13 - 6;
        downs[entity] = entity % 7 - 3;
        per_ticks[entity] = entity % 4;
    }
    for (int32_t tick = 0; tick < 100; tick++) {
        for (int32_t entity = 0; entity < ENTITY_COUNT; entity++) {
            lefts[entity] += acrosses[entity];
            tops[entity] += downs[entity];
            amounts[entity] += per_ticks[entity];
        }
    }
    int64_t places = 0;
    int32_t health = 0;
    for (int32_t entity = 0; entity < ENTITY_COUNT; entity++) {
        places += (int64_t)lefts[entity] + tops[entity];
        health += amounts[entity];
    }
    int64_t microseconds = microseconds_since(start);
    printf("places %lld health %d\n", (long long)places, health);
    print_microseconds(microseconds);
    free(lefts);
    free(tops);
    free(acrosses);
    free(downs);
    free(amounts);
    free(per_ticks);
    return 0;
}
