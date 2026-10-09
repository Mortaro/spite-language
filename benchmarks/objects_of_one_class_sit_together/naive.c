/* naive/ written in C the way it reads: a point and a note made in turn with malloc, each kept in its own growable
 * list of pointers, and the points summed through their pointers wherever malloc put them. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Point {
    int32_t across;
    int32_t down;
} Point;

typedef struct Note {
    const char* text;
} Note;

typedef struct List {
    void** items;
    int32_t count;
    int32_t capacity;
} List;

static List points = {NULL, 0, 0};
static List notes = {NULL, 0, 0};

static void list_append(List* list, void* item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(void*) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static void list_free(List* list) {
    for (int32_t index = 0; index < list->count; index = index + 1) free(list->items[index]);
    free(list->items);
}

static Point* point_make(int32_t across, int32_t down) {
    Point* point = malloc(sizeof(Point));
    point->across = across;
    point->down = down;
    return point;
}

static Note* note_make(const char* text) {
    Note* note = malloc(sizeof(Note));
    note->text = text;
    return note;
}

static void make_both(int32_t count) {
    for (int32_t index = 0; index < count; index = index + 1) {
        Point* point = point_make(index % 1000, index % 30);
        list_append(&points, point);
        Note* note = note_make("made");
        list_append(&notes, note);
    }
}

static int64_t sum_points(void) {
    int64_t total = 0;
    for (int32_t round = 0; round < 30; round = round + 1) {
        for (int32_t position = 0; position < points.count; position = position + 1) {
            Point* point = points.items[position];
            if (point->down >= round) total = total + point->across + point->down;
        }
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    make_both(300000);
    int64_t total = sum_points();
    int32_t noted = notes.count;
    int64_t microseconds = microseconds_since(start);
    printf("total %lld notes %d\n", (long long)total, noted);
    print_microseconds(microseconds);
    list_free(&points);
    list_free(&notes);
    return 0;
}
