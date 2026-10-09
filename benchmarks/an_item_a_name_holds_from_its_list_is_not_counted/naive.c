/* naive/ written in C the way it reads: a list of pointers to columns and a growable array of weights; visit
 * checks the place, names the column, calls hit on it and stores what weight answers for it. C keeps no counts, so
 * the name is the pointer from the slot and nothing else. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Column {
    int32_t hits;
    int32_t step;
} Column;

typedef struct ColumnList {
    Column** items;
    int32_t count;
    int32_t capacity;
} ColumnList;

typedef struct IntegerList {
    int32_t* items;
    int32_t count;
    int32_t capacity;
} IntegerList;

static ColumnList* columns;
static IntegerList* weights;

static ColumnList* column_list_make(void) {
    ColumnList* list = malloc(sizeof(ColumnList));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void column_list_append(ColumnList* list, Column* item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(Column*) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static IntegerList* integer_list_make(void) {
    IntegerList* list = malloc(sizeof(IntegerList));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void integer_list_append(IntegerList* list, int32_t item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(int32_t) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static void integer_list_set(IntegerList* list, int32_t index, int32_t value) {
    if (index < 0 || index >= list->count) abort();
    list->items[index] = value;
}

static Column* column_make(int32_t step) {
    Column* column = malloc(sizeof(Column));
    column->hits = 0;
    column->step = step;
    return column;
}

static void column_hit(Column* column) {
    column->hits = column->hits + column->step;
}

static int32_t weight(Column* column) {
    return column->hits * 10;
}

static void visit(int32_t index) {
    if (index < 0 || index >= columns->count) abort();
    Column* column = columns->items[index];
    column_hit(column);
    integer_list_set(weights, index, weight(column));
}

static void visit_all(void) {
    for (int32_t index = 0; index < columns->count; index = index + 1) {
        visit(index);
    }
}

static int32_t sum_hits(ColumnList* list) {
    int32_t total = 0;
    for (int32_t index = 0; index < list->count; index = index + 1) {
        total = total + list->items[index]->hits;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    columns = column_list_make();
    weights = integer_list_make();
    for (int32_t index = 0; index < 100000; index = index + 1) {
        Column* made = column_make(index % 3 + 1);
        column_list_append(columns, made);
        integer_list_append(weights, 0);
    }
    for (int32_t round = 0; round < 100; round = round + 1) {
        visit_all();
    }
    int32_t hits = sum_hits(columns);
    if (weights->count <= 99999) abort();
    int32_t last = weights->items[99999];
    int64_t microseconds = microseconds_since(start);
    printf("hits %d last weight %d\n", hits, last);
    print_microseconds(microseconds);
    for (int32_t index = 0; index < columns->count; index = index + 1) {
        free(columns->items[index]);
    }
    free(columns->items);
    free(columns);
    free(weights->items);
    free(weights);
    return 0;
}
