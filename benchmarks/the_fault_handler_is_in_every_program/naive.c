/* naive/ written in C the way it reads: a growable array of numbers and a loop that adds up each times its place.
 * A C program installs no fault handler unless its author writes one, so a null read or a stack overflow in it
 * ends with whatever the system prints, often nothing; every Spite program reports it. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct IntegerList {
    int32_t* items;
    int32_t count;
    int32_t capacity;
} IntegerList;

static IntegerList* integer_list_make(void) {
    IntegerList* list = malloc(sizeof(IntegerList));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void integer_list_append(IntegerList* list, int32_t value) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(int32_t) * list->capacity);
    }
    list->items[list->count] = value;
    list->count = list->count + 1;
}

static int32_t weighted_sum(IntegerList* numbers) {
    int32_t total = 0;
    for (int32_t index = 0; index < numbers->count; index = index + 1) {
        int32_t number = numbers->items[index];
        total = total + number * index;
    }
    return total;
}

int main(void) {
    IntegerList* numbers = integer_list_make();
    for (int32_t index = 0; index < 100; index = index + 1) {
        integer_list_append(numbers, index % 9);
    }
    int32_t total = weighted_sum(numbers);
    printf("weighted %d\n", total);
    free(numbers->items);
    free(numbers);
    return 0;
}
