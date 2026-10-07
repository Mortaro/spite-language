/* naive/ written in C the way it reads: text never changes, so "text + piece" makes a new text of both and the old
 * one is let go, on every append. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

typedef struct Text {
    char* bytes;
    int64_t length;
} Text;

static Text* text_make(const char* bytes, int64_t length) {
    Text* text = malloc(sizeof(Text));
    text->bytes = malloc((size_t)length + 1);
    memcpy(text->bytes, bytes, (size_t)length);
    text->bytes[length] = 0;
    text->length = length;
    return text;
}

static void text_free(Text* text) {
    free(text->bytes);
    free(text);
}

static Text* text_join(Text* left, Text* right) {
    Text* joined = malloc(sizeof(Text));
    joined->length = left->length + right->length;
    joined->bytes = malloc((size_t)joined->length + 1);
    memcpy(joined->bytes, left->bytes, (size_t)left->length);
    memcpy(joined->bytes + left->length, right->bytes, (size_t)right->length);
    joined->bytes[joined->length] = 0;
    return joined;
}

static Text* words(int32_t count) {
    Text* text = text_make("", 0);
    Text* piece = text_make("word ", 5);
    for (int32_t index = 0; index < count; index = index + 1) {
        Text* longer = text_join(text, piece);
        text_free(text);
        text = longer;
    }
    text_free(piece);
    return text;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t total = 0;
    for (int32_t round = 0; round < 8; round = round + 1) {
        Text* text = words(10000);
        total = total + (int32_t)text->length;
        text_free(text);
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("characters %d\n", total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
