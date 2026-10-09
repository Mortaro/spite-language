/* naive/ written in C the way it reads: each number in the text becomes a text of its own, then the pieces are
 * joined into the line, and the number's texts and the line are let go. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

static char* integer_to_text(int32_t value) {
    char* text = malloc(16);
    snprintf(text, 16, "%d", value);
    return text;
}

static char* join(const char** pieces, int32_t count) {
    size_t length = 0;
    for (int32_t index = 0; index < count; index = index + 1) length = length + strlen(pieces[index]);
    char* joined = malloc(length + 1);
    size_t at = 0;
    for (int32_t index = 0; index < count; index = index + 1) {
        size_t piece_length = strlen(pieces[index]);
        memcpy(joined + at, pieces[index], piece_length);
        at = at + piece_length;
    }
    joined[at] = 0;
    return joined;
}

static int32_t lines(int32_t count, int32_t round) {
    int32_t total = 0;
    for (int32_t index = 0; index < count; index = index + 1) {
        char* index_text = integer_to_text(index);
        char* round_text = integer_to_text(round);
        const char* pieces[] = {"line ", index_text, " of ", round_text, ";"};
        char* line = join(pieces, 5);
        free(index_text);
        free(round_text);
        total = total + (int32_t)strlen(line);
        free(line);
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int64_t total = 0;
    for (int32_t round = 0; round < 20; round = round + 1) {
        total = total + lines(100000, round);
    }
    int64_t microseconds = microseconds_since(start);
    printf("characters %lld\n", (long long)total);
    print_microseconds(microseconds);
    return 0;
}
