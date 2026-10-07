/* naive/ written in C the way it reads: each round opens the file to write the round's text and closes it, then
 * opens it again, measures it, reads it into a new buffer and closes it, with the C library's blocking calls. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

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

static int32_t write_and_read(const char* path, int32_t rounds) {
    int32_t characters = 0;
    for (int32_t round = 0; round < rounds; round = round + 1) {
        char text[64];
        snprintf(text, sizeof(text), "round %d of the notes", round);
        file_write(path, text);
        char* read = file_read(path);
        if (read == NULL) {
            fprintf(stderr, "the notes could not be read\n");
            exit(1);
        }
        characters = characters + (int32_t)strlen(read);
        free(read);
    }
    return characters;
}

int main(void) {
    const char* path = ".spite/concurrency_machinery_notes.txt";
    int64_t start = now_nanoseconds();
    int32_t characters = write_and_read(path, 50);
    int removed = remove(path) == 0;
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("characters %d removed %s\n", characters, removed ? "true" : "false");
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
