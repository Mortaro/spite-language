/* naive/ written in C the way it reads, a join at a time: the first name and " meets " make a text, that text and
 * the second name make another, and so on, each one let go once the next is made. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

typedef struct List {
    char** items;
    int32_t count;
    int32_t capacity;
} List;

static void list_append(List* list, char* item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(char*) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static char* join_two(const char* left, const char* right) {
    size_t left_length = strlen(left);
    size_t right_length = strlen(right);
    char* joined = malloc(left_length + right_length + 1);
    memcpy(joined, left, left_length);
    memcpy(joined + left_length, right, right_length + 1);
    return joined;
}

static int32_t meetings(List* names) {
    int32_t total = 0;
    for (int32_t index = 0; index + 1 < names->count; index = index + 1) {
        char* first = join_two(names->items[index], " meets ");
        char* second = join_two(first, names->items[index + 1]);
        char* line = join_two(second, " at the gate.");
        free(first);
        free(second);
        total = total + (int32_t)strlen(line);
        free(line);
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    List* names = calloc(1, sizeof(List));
    for (int32_t index = 0; index < 1000; index = index + 1) {
        char* name = malloc(32);
        snprintf(name, 32, "traveller number %d", index);
        list_append(names, name);
    }
    int64_t total = 0;
    for (int32_t round = 0; round < 1000; round = round + 1) {
        total = total + meetings(names);
    }
    int64_t microseconds = microseconds_since(start);
    printf("characters %lld\n", (long long)total);
    print_microseconds(microseconds);
    for (int32_t index = 0; index < names->count; index = index + 1) free(names->items[index]);
    free(names->items);
    free(names);
    return 0;
}
