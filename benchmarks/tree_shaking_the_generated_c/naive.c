/* naive/ written in C the way it reads: a growable array of texts, the names joined into a new text, and the
 * longest length found by walking them. A C programmer writes only what the program uses, which is what the
 * compiler's tree shaking leaves of the Spite program. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct TextList {
    const char** items;
    int32_t count;
    int32_t capacity;
} TextList;

static TextList* text_list_make(void) {
    TextList* list = malloc(sizeof(TextList));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void text_list_append(TextList* list, const char* item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(const char*) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static void text_list_free(TextList* list) {
    free(list->items);
    free(list);
}

static char* join(TextList* list, const char* separator) {
    size_t length = 0;
    for (int32_t index = 0; index < list->count; index = index + 1) {
        if (index > 0) length = length + strlen(separator);
        length = length + strlen(list->items[index]);
    }
    char* text = malloc(length + 1);
    text[0] = 0;
    for (int32_t index = 0; index < list->count; index = index + 1) {
        if (index > 0) strcat(text, separator);
        strcat(text, list->items[index]);
    }
    return text;
}

static int32_t longest_length(TextList* names) {
    int32_t longest = 0;
    for (int32_t index = 0; index < names->count; index = index + 1) {
        int32_t length = (int32_t)strlen(names->items[index]);
        longest = longest > length ? longest : length;
    }
    return longest;
}

int main(void) {
    TextList* names = text_list_make();
    text_list_append(names, "ann");
    text_list_append(names, "bob");
    text_list_append(names, "cy");
    char* joined = join(names, ", ");
    int32_t longest = longest_length(names);
    printf("%s %d\n", joined, longest);
    free(joined);
    text_list_free(names);
    return 0;
}
