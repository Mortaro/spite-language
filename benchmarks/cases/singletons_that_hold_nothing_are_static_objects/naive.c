/* naive/ written in C the way it reads: a growable array of numbers and one of texts, each made with malloc, the
 * texts made one by one and joined into a new one. A C programmer has no object for the heap or for how a list
 * stores its items: malloc and the array's element type are that already. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct LongList {
    int64_t* items;
    int32_t count;
    int32_t capacity;
} LongList;

typedef struct TextList {
    char** items;
    int32_t count;
    int32_t capacity;
} TextList;

static LongList* long_list_make(void) {
    LongList* list = malloc(sizeof(LongList));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void long_list_append(LongList* list, int64_t item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(int64_t) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static TextList* text_list_make(void) {
    TextList* list = malloc(sizeof(TextList));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void text_list_append(TextList* list, char* item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(char*) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static char* text_list_join(TextList* list, const char* separator) {
    size_t length = 0;
    for (int32_t index = 0; index < list->count; index = index + 1) {
        if (index > 0) length = length + strlen(separator);
        length = length + strlen(list->items[index]);
    }
    char* joined = malloc(length + 1);
    joined[0] = 0;
    for (int32_t index = 0; index < list->count; index = index + 1) {
        if (index > 0) strcat(joined, separator);
        strcat(joined, list->items[index]);
    }
    return joined;
}

static LongList* make_squares(int32_t count) {
    LongList* squares = long_list_make();
    for (int32_t index = 0; index < count; index = index + 1) {
        int64_t wide = index;
        long_list_append(squares, wide * index);
    }
    return squares;
}

static TextList* make_names(int32_t count) {
    TextList* names = text_list_make();
    for (int32_t index = 0; index < count; index = index + 1) {
        char* name = malloc(24);
        snprintf(name, 24, "name %d", index);
        text_list_append(names, name);
    }
    return names;
}

int main(void) {
    LongList* squares = make_squares(1000);
    TextList* names = make_names(100);
    int64_t largest = squares->items[squares->count - 1];
    char* joined = text_list_join(names, ",");
    int32_t characters = (int32_t)strlen(joined);
    printf("squares %d largest %lld characters %d\n", squares->count, (long long)largest, characters);
    free(joined);
    for (int32_t index = 0; index < names->count; index = index + 1) free(names->items[index]);
    free(names->items);
    free(names);
    free(squares->items);
    free(squares);
    return 0;
}
