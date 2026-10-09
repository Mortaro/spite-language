/* The same work tuned by hand: each column's components as columns of numbers, each sparse set an array from entity
 * to dense place made once at its size (-1 where the entity has no such component), and each tick one loop over the
 * entities that looks up both places and moves the entity in place. No list of places and no row. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define ENTITY_COUNT 100000

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t* position_place = malloc(sizeof(int32_t) * ENTITY_COUNT);
    int32_t* lefts = calloc(ENTITY_COUNT, sizeof(int32_t));
    int32_t* tops = calloc(ENTITY_COUNT, sizeof(int32_t));
    int32_t* velocity_place = malloc(sizeof(int32_t) * ENTITY_COUNT);
    int32_t* acrosses = malloc(sizeof(int32_t) * ENTITY_COUNT);
    int32_t* downs = malloc(sizeof(int32_t) * ENTITY_COUNT);
    for (int32_t entity = 0; entity < ENTITY_COUNT; entity++) {
        position_place[entity] = entity;
        velocity_place[entity] = -1;
    }
    int32_t velocity_count = 0;
    for (int32_t backwards = ENTITY_COUNT - 1; backwards >= 0; backwards--) {
        if (backwards % 3 != 0) {
            velocity_place[backwards] = velocity_count;
            acrosses[velocity_count] = backwards % 13 - 6;
            downs[velocity_count] = backwards % 7 - 3;
            velocity_count++;
        }
    }
    int32_t marked = 0;
    for (int32_t tick = 0; tick < 50; tick++) {
        for (int32_t entity = 0; entity < ENTITY_COUNT; entity++) {
            int32_t velocity = velocity_place[entity];
            if (velocity < 0) continue;
            int32_t position = position_place[entity];
            lefts[position] += acrosses[velocity];
            tops[position] += downs[velocity];
            marked += entity % 1000 == 0;
        }
    }
    int64_t places = 0;
    for (int32_t index = 0; index < ENTITY_COUNT; index++) places += (int64_t)lefts[index] + tops[index];
    int64_t microseconds = microseconds_since(start);
    printf("places %lld marked %d\n", (long long)places, marked);
    print_microseconds(microseconds);
    free(position_place);
    free(lefts);
    free(tops);
    free(velocity_place);
    free(acrosses);
    free(downs);
    return 0;
}
