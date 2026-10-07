/* naive/ written in C the way it reads: a program of one thread that counts its objects, so every count is a plain
 * addition and subtraction, as a person writing a reference-counted program with no threads in C writes it. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Point {
    int32_t ref_count;
    int32_t across;
    int32_t down;
} Point;

typedef struct List {
    void** items;
    int32_t count;
    int32_t capacity;
} List;

static List* list_make(void) {
    List* list = malloc(sizeof(List));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void list_append(List* list, void* item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(void*) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static Point* point_make(int32_t across, int32_t down) {
    Point* point = malloc(sizeof(Point));
    point->ref_count = 1;
    point->across = across;
    point->down = down;
    return point;
}

static Point* point_retain(Point* point) {
    point->ref_count = point->ref_count + 1;
    return point;
}

static void point_release(Point* point) {
    point->ref_count = point->ref_count - 1;
    if (point->ref_count == 0) free(point);
}

static void point_list_free(List* list) {
    for (int32_t index = 0; index < list->count; index = index + 1) point_release(list->items[index]);
    free(list->items);
    free(list);
}

static List* make_points(int32_t count) {
    List* points = list_make();
    for (int32_t index = 0; index < count; index = index + 1) {
        Point* point = point_make(index % 1000, index % 700);
        list_append(points, point);
    }
    return points;
}

static int64_t count_near(List* points) {
    int64_t near_count = 0;
    for (int32_t round = 0; round < 20; round = round + 1) {
        List* kept = list_make();
        for (int32_t index = 0; index < points->count; index = index + 1) {
            Point* point = points->items[index];
            if (point->across + point->down > round * 80) list_append(kept, point_retain(point));
        }
        near_count = near_count + kept->count;
        point_list_free(kept);
    }
    return near_count;
}

int main(void) {
    int64_t start = now_nanoseconds();
    List* points = make_points(200000);
    int64_t near_count = count_near(points);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("near %lld\n", (long long)near_count);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    point_list_free(points);
    return 0;
}
