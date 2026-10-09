/* naive/ written in C the way it reads: a struct for Point, and origin.copy() a new malloc with the attributes copied
 * into it, moved, measured and freed at the end of its trial. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

typedef struct Point {
    int32_t x;
    int32_t y;
} Point;

static Point* point_make(int32_t x, int32_t y) {
    Point* point = malloc(sizeof(Point));
    point->x = x;
    point->y = y;
    return point;
}

static Point* point_copy(Point* point) {
    Point* copy = malloc(sizeof(Point));
    memcpy(copy, point, sizeof(Point));
    return copy;
}

static void move_by(Point* point, int32_t across, int32_t down) {
    point->x = point->x + across;
    point->y = point->y + down;
}

static int32_t distance_from_origin(Point* point) {
    return abs(point->x) + abs(point->y);
}

static int64_t all_trials(Point* origin, int32_t trials) {
    int64_t total = 0;
    for (int32_t trial = 0; trial < trials; trial = trial + 1) {
        Point* moved = point_copy(origin);
        move_by(moved, trial % 7 - 3, trial % 5 - 2);
        total = total + distance_from_origin(moved);
        free(moved);
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    Point* origin = point_make(3, -4);
    int64_t total = all_trials(origin, 20000000);
    int64_t microseconds = microseconds_since(start);
    printf("total %lld origin %d %d\n", (long long)total, origin->x, origin->y);
    print_microseconds(microseconds);
    free(origin);
    return 0;
}
