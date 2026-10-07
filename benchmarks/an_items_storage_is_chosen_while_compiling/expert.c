/* The same work tuned by hand: each component's numbers in columns made once at their size, a trail's empty list of
 * marks held inline beside it (nothing allocated for it until it holds a mark), and each tick one loop per
 * component that integrates and adds up in the same pass, which the C compiler vectorises. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define ITEM_COUNT 100000

typedef struct IntegerList {
    int32_t* items;
    int32_t count;
    int32_t capacity;
} IntegerList;

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t* velocity_acrosses = malloc(sizeof(int32_t) * ITEM_COUNT);
    int32_t* velocity_downs = malloc(sizeof(int32_t) * ITEM_COUNT);
    int32_t* trail_acrosses = malloc(sizeof(int32_t) * ITEM_COUNT);
    int32_t* trail_downs = malloc(sizeof(int32_t) * ITEM_COUNT);
    IntegerList* trail_marks = calloc(ITEM_COUNT, sizeof(IntegerList));
    for (int32_t index = 0; index < ITEM_COUNT; index++) {
        velocity_acrosses[index] = index % 100;
        velocity_downs[index] = index % 7;
        trail_acrosses[index] = index % 50;
        trail_downs[index] = index % 5;
    }
    int64_t total = 0;
    for (int32_t tick = 0; tick < 100; tick++) {
        int32_t moved = 0;
        for (int32_t index = 0; index < ITEM_COUNT; index++) {
            velocity_acrosses[index] += velocity_downs[index];
            moved += velocity_acrosses[index];
        }
        int32_t trailed = 0;
        for (int32_t index = 0; index < ITEM_COUNT; index++) {
            trail_acrosses[index] += trail_downs[index];
            trailed += trail_acrosses[index];
        }
        total += (int64_t)moved + trailed;
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(velocity_acrosses);
    free(velocity_downs);
    free(trail_acrosses);
    free(trail_downs);
    free(trail_marks);
    return 0;
}
