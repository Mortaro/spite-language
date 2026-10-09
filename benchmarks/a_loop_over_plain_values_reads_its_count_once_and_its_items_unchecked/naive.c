/* naive/ written in C the way it reads: a growable array of floats per List<Float>, and a function per Spite
 * function with the same loops. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct FloatList {
    float* items;
    int32_t count;
    int32_t capacity;
} FloatList;

static FloatList* list_make(void) {
    FloatList* list = malloc(sizeof(FloatList));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void list_append(FloatList* list, float value) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(float) * list->capacity);
    }
    list->items[list->count] = value;
    list->count = list->count + 1;
}

static void list_free(FloatList* list) {
    free(list->items);
    free(list);
}

static void scale_list(FloatList* from, FloatList* into, float offset) {
    for (int32_t index = 0; index < from->count; index = index + 1) {
        into->items[index] = from->items[index] * 1.5f + offset;
    }
}

static float add_up(FloatList* values) {
    float total = 0.0f;
    for (int32_t index = 0; index < values->count; index = index + 1) {
        total = total + values->items[index];
    }
    return total;
}

static int64_t rounds(FloatList* from, FloatList* into) {
    int64_t total = 0;
    for (int32_t round = 0; round < 2000; round = round + 1) {
        float offset = (float)(round % 4);
        offset = offset * 0.25f;
        scale_list(from, into, offset);
        float sum = add_up(into);
        int64_t quarters = (int64_t)(sum * 4.0f);
        total = total + quarters;
    }
    return total;
}

int main(void) {
    FloatList* from = list_make();
    FloatList* into = list_make();
    for (int32_t index = 0; index < 100000; index = index + 1) {
        list_append(from, (float)(index % 7));
        list_append(into, 0.0f);
    }
    int64_t start = now_nanoseconds();
    int64_t total = rounds(from, into);
    int64_t microseconds = microseconds_since(start);
    printf("total in quarters %lld\n", (long long)total);
    print_microseconds(microseconds);
    list_free(from);
    list_free(into);
    return 0;
}
