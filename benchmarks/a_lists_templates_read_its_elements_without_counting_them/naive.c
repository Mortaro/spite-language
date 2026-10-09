/* naive/ written in C the way it reads: each body made with malloc beside its label, so the bodies lie spread
 * through memory, a list of pointers to them, and each template a function that walks the list: each_advance
 * calls advance on every body, sum_position adds up their positions. C keeps no counts, so each element is the
 * pointer from its slot. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Body {
    int32_t position;
    int32_t speed;
} Body;

typedef struct Label {
    int32_t number;
} Label;

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
    for (int32_t index = 0; index < list->count; index = index + 1) {
        free(list->items[index]);
    }
    free(list->items);
    free(list);
}

static Body* body_make(int32_t speed) {
    Body* body = malloc(sizeof(Body));
    body->position = 0;
    body->speed = speed;
    return body;
}

static Label* label_make(int32_t number) {
    Label* label = malloc(sizeof(Label));
    label->number = number;
    return label;
}

static void body_advance(Body* body) {
    body->position = body->position + body->speed;
}

static void each_advance(List* bodies) {
    for (int32_t index = 0; index < bodies->count; index = index + 1) {
        body_advance(bodies->items[index]);
    }
}

static int32_t sum_position(List* bodies) {
    int32_t total = 0;
    for (int32_t index = 0; index < bodies->count; index = index + 1) {
        Body* body = bodies->items[index];
        total = total + body->position;
    }
    return total;
}

static int32_t sum_number(List* labels) {
    int32_t total = 0;
    for (int32_t index = 0; index < labels->count; index = index + 1) {
        Label* label = labels->items[index];
        total = total + label->number;
    }
    return total;
}

static int64_t simulate(List* bodies) {
    int64_t total = 0;
    for (int32_t round = 0; round < 100; round = round + 1) {
        each_advance(bodies);
        int32_t positions = sum_position(bodies);
        total = total + positions;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    List* bodies = list_make();
    List* labels = list_make();
    for (int32_t index = 0; index < 200000; index = index + 1) {
        Body* body = body_make(index % 5 + 1);
        list_append(bodies, body);
        Label* label = label_make(index % 10);
        list_append(labels, label);
    }
    int64_t total = simulate(bodies);
    int32_t numbers = sum_number(labels);
    int64_t microseconds = microseconds_since(start);
    printf("total %lld labels %d\n", (long long)total, numbers);
    print_microseconds(microseconds);
    list_free(bodies);
    list_free(labels);
    return 0;
}
