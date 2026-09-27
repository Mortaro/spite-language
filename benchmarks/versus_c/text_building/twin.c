/* The C twin of text_building.spite: a growable byte buffer, numbers written by hand, words joined by memcpy. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Buffer {
    char* bytes;
    size_t length;
    size_t capacity;
} Buffer;

static void reserve(Buffer* buffer, size_t more) {
    if (buffer->length + more <= buffer->capacity) {
        return;
    }
    size_t capacity = buffer->capacity == 0 ? 64 : buffer->capacity;
    while (capacity < buffer->length + more) {
        capacity = capacity * 2;
    }
    buffer->bytes = realloc(buffer->bytes, capacity);
    buffer->capacity = capacity;
}

static void append_bytes(Buffer* buffer, const char* bytes, size_t length) {
    reserve(buffer, length);
    memcpy(buffer->bytes + buffer->length, bytes, length);
    buffer->length = buffer->length + length;
}

static size_t digits_of(int32_t number, char* into) {
    char reversed[16];
    size_t count = 0;
    uint32_t left = number < 0 ? (uint32_t)(-(int64_t)number) : (uint32_t)number;
    do {
        reversed[count] = (char)('0' + left % 10);
        count = count + 1;
        left = left / 10;
    } while (left != 0);
    size_t written = 0;
    if (number < 0) {
        into[written] = '-';
        written = written + 1;
    }
    while (count > 0) {
        count = count - 1;
        into[written] = reversed[count];
        written = written + 1;
    }
    return written;
}

int main(void) {
    Buffer built = {0, 0, 0};
    char digits[16];
    for (int32_t index = 0; index < 3000000; index = index + 1) {
        append_bytes(&built, "item ", 5);
        append_bytes(&built, digits, digits_of(index, digits));
        append_bytes(&built, ",", 1);
    }
    int32_t word_count = 1000000;
    char** words = malloc(sizeof(char*) * word_count);
    size_t* lengths = malloc(sizeof(size_t) * word_count);
    for (int32_t index = 0; index < word_count; index = index + 1) {
        size_t length = digits_of(index, digits);
        words[index] = malloc(4 + length);
        memcpy(words[index], "word", 4);
        memcpy(words[index] + 4, digits, length);
        lengths[index] = 4 + length;
    }
    size_t joined_length = 0;
    for (int32_t index = 0; index < word_count; index = index + 1) {
        joined_length = joined_length + lengths[index] + (index > 0 ? 2 : 0);
    }
    char* joined = malloc(joined_length + 1);
    size_t position = 0;
    for (int32_t index = 0; index < word_count; index = index + 1) {
        if (index > 0) {
            memcpy(joined + position, ", ", 2);
            position = position + 2;
        }
        memcpy(joined + position, words[index], lengths[index]);
        position = position + lengths[index];
    }
    joined[position] = 0;
    printf("built %lld joined %lld\n", (long long)built.length, (long long)position);
    for (int32_t index = 0; index < word_count; index = index + 1) {
        free(words[index]);
    }
    free(words);
    free(lengths);
    free(joined);
    free(built.bytes);
    return 0;
}
