/* naive/ written in C the way it reads: a program that starts a thread and counts its objects, so every object's
 * count is an atomic one, as a person writing a threaded reference-counted program in C writes it. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct Summer {
    int32_t ref_count;
    int32_t limit;
    int64_t result;
} Summer;

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

static Summer* summer_make(int32_t limit) {
    Summer* summer = malloc(sizeof(Summer));
    summer->ref_count = 1;
    summer->limit = limit;
    summer->result = 0;
    return summer;
}

static Summer* summer_retain(Summer* summer) {
    __atomic_add_fetch(&summer->ref_count, 1, __ATOMIC_RELAXED);
    return summer;
}

static void summer_release(Summer* summer) {
    if (__atomic_sub_fetch(&summer->ref_count, 1, __ATOMIC_ACQ_REL) == 0) free(summer);
}

static Point* point_make(int32_t across, int32_t down) {
    Point* point = malloc(sizeof(Point));
    point->ref_count = 1;
    point->across = across;
    point->down = down;
    return point;
}

static Point* point_retain(Point* point) {
    __atomic_add_fetch(&point->ref_count, 1, __ATOMIC_RELAXED);
    return point;
}

static void point_release(Point* point) {
    if (__atomic_sub_fetch(&point->ref_count, 1, __ATOMIC_ACQ_REL) == 0) free(point);
}

static void point_list_free(List* list) {
    for (int32_t index = 0; index < list->count; index = index + 1) point_release(list->items[index]);
    free(list->items);
    free(list);
}

static int64_t summer_total(Summer* summer) {
    int64_t sum = 0;
    for (int32_t index = 0; index < summer->limit; index = index + 1) sum = sum + index % 7;
    return sum;
}

#ifdef _WIN32
static DWORD WINAPI summer_thread(LPVOID argument) {
#else
static void* summer_thread(void* argument) {
#endif
    Summer* summer = argument;
    summer->result = summer_total(summer);
    summer_release(summer);
    return 0;
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
    Summer* summer = summer_make(20000000);
    summer_retain(summer);
#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, summer_thread, summer, 0, NULL);
#else
    pthread_t thread;
    pthread_create(&thread, NULL, summer_thread, summer);
#endif
    List* points = make_points(200000);
    int64_t near_count = count_near(points);
#ifdef _WIN32
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    pthread_join(thread, NULL);
#endif
    int64_t sum = summer->result;
    summer_release(summer);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("sum %lld near %lld\n", (long long)sum, (long long)near_count);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    point_list_free(points);
    return 0;
}
