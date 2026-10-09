/* naive/ written in C the way it reads: `tally.count_value` is a function value, so a C programmer makes one (a
 * pointer to the function and the object it belongs to, allocated), and `each` calls it through the pointer. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Tally {
    int64_t total;
    int32_t large;
} Tally;

typedef struct IntegerList {
    int32_t* items;
    int32_t count;
    int32_t capacity;
} IntegerList;

typedef struct FunctionValue {
    void (*call)(void* owner, int32_t value);
    void* owner;
} FunctionValue;

static void list_append(IntegerList* list, int32_t value) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(int32_t) * list->capacity);
    }
    list->items[list->count] = value;
    list->count = list->count + 1;
}

static void list_each(IntegerList* list, FunctionValue* function) {
    for (int32_t index = 0; index < list->count; index = index + 1) {
        function->call(function->owner, list->items[index]);
    }
}

static void tally_count_value(void* owner, int32_t value) {
    Tally* tally = owner;
    tally->total = tally->total + value;
    if (value > 900) tally->large = tally->large + 1;
}

int main(void) {
    int64_t start = now_nanoseconds();
    IntegerList* values = calloc(1, sizeof(IntegerList));
    for (int32_t index = 0; index < 1000000; index = index + 1) list_append(values, index % 1000);
    Tally* tally = calloc(1, sizeof(Tally));
    for (int32_t round = 0; round < 50; round = round + 1) {
        FunctionValue* count_value = malloc(sizeof(FunctionValue));
        count_value->call = tally_count_value;
        count_value->owner = tally;
        list_each(values, count_value);
        free(count_value);
    }
    int64_t microseconds = microseconds_since(start);
    printf("total %lld large %d\n", (long long)tally->total, tally->large);
    print_microseconds(microseconds);
    free(tally);
    free(values->items);
    free(values);
    return 0;
}
