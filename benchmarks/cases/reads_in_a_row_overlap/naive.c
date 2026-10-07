/* naive/ written in C the way it reads: the two files written once, then each round reads the first file and then
 * the second, each read opening the file, measuring it, reading it into a new buffer and closing it. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

static char* make_text(const char* word, int32_t count) {
    size_t capacity = 64;
    size_t length = 0;
    char* text = malloc(capacity);
    text[0] = 0;
    for (int32_t index = 0; index < count; index = index + 1) {
        char line[64];
        int written = snprintf(line, sizeof(line), index == 0 ? "%s line number %d" : "\n%s line number %d", word, index);
        while (length + (size_t)written + 1 > capacity) {
            capacity = capacity * 2;
            text = realloc(text, capacity);
        }
        memcpy(text + length, line, (size_t)written + 1);
        length = length + (size_t)written;
    }
    return text;
}

static int file_write(const char* path, const char* text) {
    FILE* file = fopen(path, "wb");
    if (file == NULL) return 0;
    size_t length = strlen(text);
    size_t written = fwrite(text, 1, length, file);
    fclose(file);
    return written == length;
}

static char* file_read(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) return NULL;
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* content = malloc((size_t)length + 1);
    size_t got = fread(content, 1, (size_t)length, file);
    content[got] = 0;
    fclose(file);
    return content;
}

static int32_t read_both(const char* first_path, const char* second_path) {
    char* first = file_read(first_path);
    char* second = file_read(second_path);
    if (first == NULL || second == NULL) {
        fprintf(stderr, "a file could not be read\n");
        exit(1);
    }
    int32_t characters = (int32_t)strlen(first) + (int32_t)strlen(second);
    free(first);
    free(second);
    return characters;
}

static int64_t read_rounds(const char* first_path, const char* second_path, int32_t rounds) {
    int64_t characters = 0;
    for (int32_t round = 0; round < rounds; round = round + 1) {
        int32_t read = read_both(first_path, second_path);
        characters = characters + read;
    }
    return characters;
}

int main(void) {
    const char* first_path = ".spite/reads_in_a_row_first.txt";
    const char* second_path = ".spite/reads_in_a_row_second.txt";
    char* first_text = make_text("first", 4000);
    char* second_text = make_text("second", 4000);
    file_write(first_path, first_text);
    file_write(second_path, second_text);
    int64_t start = now_nanoseconds();
    int64_t characters = read_rounds(first_path, second_path, 40);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    int first_removed = remove(first_path) == 0;
    int second_removed = remove(second_path) == 0;
    printf("characters %lld removed %s %s\n", (long long)characters, first_removed ? "true" : "false",
        second_removed ? "true" : "false");
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(first_text);
    free(second_text);
    return 0;
}
