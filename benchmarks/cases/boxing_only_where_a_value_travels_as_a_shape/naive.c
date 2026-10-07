/* naive/ written in C the way it reads: a List<Printable> holds values of any class, so each is an object, the way a
 * C programmer writes "anything printable": a malloc'd box holding the value's class and the value (a number, a
 * Boolean, or a pointer to its malloc'd text), a growable array of box pointers, and to_string a switch on the
 * class that makes a new malloc'd text. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

typedef enum PrintableClass {
    PRINTABLE_INTEGER,
    PRINTABLE_BOOLEAN,
    PRINTABLE_TEXT,
} PrintableClass;

typedef struct Printable {
    PrintableClass class;
    union {
        int32_t integer;
        bool boolean;
        char* text;
    } value;
} Printable;

typedef struct List {
    Printable** items;
    int32_t count;
    int32_t capacity;
} List;

static void list_append(List* list, Printable* item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(Printable*) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static Printable* box_integer(int32_t value) {
    Printable* box = malloc(sizeof(Printable));
    box->class = PRINTABLE_INTEGER;
    box->value.integer = value;
    return box;
}

static Printable* box_boolean(bool value) {
    Printable* box = malloc(sizeof(Printable));
    box->class = PRINTABLE_BOOLEAN;
    box->value.boolean = value;
    return box;
}

static Printable* box_text(char* value) {
    Printable* box = malloc(sizeof(Printable));
    box->class = PRINTABLE_TEXT;
    box->value.text = value;
    return box;
}

static char* to_string(Printable* value) {
    char* text = malloc(32);
    switch (value->class) {
        case PRINTABLE_INTEGER: snprintf(text, 32, "%d", value->value.integer); break;
        case PRINTABLE_BOOLEAN: strcpy(text, value->value.boolean ? "true" : "false"); break;
        case PRINTABLE_TEXT: strcpy(text, value->value.text); break;
    }
    return text;
}

static void add_value(List* values, int32_t index) {
    int32_t kind = index % 3;
    if (kind == 0) {
        list_append(values, box_integer(index));
    } else if (kind == 1) {
        list_append(values, box_boolean(index % 2 == 0));
    } else {
        char* text = malloc(32);
        snprintf(text, 32, "item %d", index);
        list_append(values, box_text(text));
    }
}

static List* make_values(int32_t count) {
    List* values = calloc(1, sizeof(List));
    for (int32_t index = 0; index < count; index = index + 1) add_value(values, index);
    return values;
}

static int32_t measure(List* values) {
    int32_t total = 0;
    for (int32_t index = 0; index < values->count; index = index + 1) {
        char* text = to_string(values->items[index]);
        total = total + (int32_t)strlen(text);
        free(text);
    }
    return total;
}

static int64_t measure_rounds(List* values) {
    int64_t total = 0;
    for (int32_t round = 0; round < 20; round = round + 1) {
        int32_t length = measure(values);
        total = total + length;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    List* values = make_values(90000);
    int64_t total = measure_rounds(values);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    for (int32_t index = 0; index < values->count; index = index + 1) {
        if (values->items[index]->class == PRINTABLE_TEXT) free(values->items[index]->value.text);
        free(values->items[index]);
    }
    free(values->items);
    free(values);
    return 0;
}
