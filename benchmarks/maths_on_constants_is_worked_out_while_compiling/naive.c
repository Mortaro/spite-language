/* naive/ written in C the way it reads: a struct per point, malloc per point, a growable array of pointers, and
 * turn calling the C library's cosf and sinf on the constant half a radian each time it needs them. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Point {
    float across;
    float down;
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

static void list_free(List* list) {
    free(list->items);
    free(list);
}

static Point* point_make(float across, float down) {
    Point* point = malloc(sizeof(Point));
    point->across = across;
    point->down = down;
    return point;
}

static List* make_points(int32_t count) {
    List* points = list_make();
    for (int32_t index = 0; index < count; index = index + 1) {
        float across = (float)(index % 100);
        float down = (float)(index % 37);
        Point* point = point_make(across, down);
        list_append(points, point);
    }
    return points;
}

static void turn(Point* point) {
    float turned_across = point->across * cosf(0.5f) - point->down * sinf(0.5f);
    float turned_down = point->across * sinf(0.5f) + point->down * cosf(0.5f);
    point->across = turned_across;
    point->down = turned_down;
}

static float sum_across(List* points) {
    float total = 0.0f;
    for (int32_t index = 0; index < points->count; index = index + 1) {
        Point* point = points->items[index];
        total = total + point->across;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    List* points = make_points(100000);
    for (int32_t round = 0; round < 200; round = round + 1) {
        for (int32_t index = 0; index < points->count; index = index + 1) {
            turn(points->items[index]);
        }
    }
    float total = sum_across(points);
    int32_t whole = (int32_t)roundf(total);
    int64_t microseconds = microseconds_since(start);
    printf("across %d\n", whole);
    print_microseconds(microseconds);
    for (int32_t index = 0; index < points->count; index = index + 1) {
        free(points->items[index]);
    }
    list_free(points);
    return 0;
}
