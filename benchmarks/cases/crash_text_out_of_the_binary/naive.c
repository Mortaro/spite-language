/* naive/ written in C the way it reads: a growable array of prices, a sum of the picked ones with a check that
 * each pick is inside the list, and a lookup that answers nothing past the end. A C programmer's checks print
 * their condition and place, so that text is in the executable; the Spite program's is in naive.crashes. */
#include <stdbool.h>
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

static void integer_list_free(IntegerList* list) {
    free(list->items);
    free(list);
}

static int32_t total_of(IntegerList* prices, IntegerList* picks) {
    int32_t total = 0;
    for (int32_t index = 0; index < picks->count; index = index + 1) {
        int32_t pick = picks->items[index];
        if (pick < 0 || pick >= prices->count) {
            fprintf(stderr, "naive.spite:22: crash prices[pick] failed in Naive.total_of: index %d, count %d\n", pick, prices->count);
            exit(1);
        }
        total = total + prices->items[pick];
    }
    return total;
}

static bool price_at(IntegerList* prices, int32_t place, int32_t* found) {
    if (!(place < prices->count)) return false;
    *found = prices->items[place];
    return true;
}

int main(void) {
    IntegerList* prices = integer_list_make();
    for (int32_t index = 0; index < 50; index = index + 1) {
        integer_list_append(prices, index * 3);
    }
    IntegerList* picks = integer_list_make();
    integer_list_append(picks, 4);
    integer_list_append(picks, 9);
    integer_list_append(picks, 16);
    integer_list_append(picks, 25);
    integer_list_append(picks, 36);
    integer_list_append(picks, 49);
    int32_t total = total_of(prices, picks);
    int32_t last = 0;
    if (!price_at(prices, 49, &last)) {
        fprintf(stderr, "naive.spite:13: crash last failed in Naive.Naive: last is null\n");
        exit(1);
    }
    printf("total %d last %d\n", total, last);
    integer_list_free(prices);
    integer_list_free(picks);
    return 0;
}
