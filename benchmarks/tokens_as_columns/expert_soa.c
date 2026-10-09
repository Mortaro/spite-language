/* The second step: the tokens as five columns (kind, start, length, line, value), a structure of arrays, with the
 * same passes as naive.c, each a plain loop over the columns it reads, which the C compiler vectorises. The text is
 * written the same way as naive.c in every form. --pieces=N sets how many pieces the text has. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

enum { IDENTIFIER, NUMBER, SYMBOL };
typedef int32_t KIND;

typedef struct Tokens {
    KIND* kinds;
    int32_t* starts;
    int32_t* lengths;
    int32_t* lines;
    int64_t* values;
    int32_t count;
    int32_t capacity;
} Tokens;

typedef struct Text {
    char* bytes;
    int32_t length;
    int32_t capacity;
} Text;

static const char* words[8] = {"alpha", "beta", "gamma", "delta", "value", "count", "index", "total"};
static const char* symbols[6] = {"+", "=", "(", ")", ";", "*"};

static void append_text(Text* text, const char* piece, int32_t length) {
    if (text->length + length + 1 > text->capacity) {
        text->capacity = (text->length + length + 1) * 2;
        text->bytes = realloc(text->bytes, text->capacity);
    }
    memcpy(text->bytes + text->length, piece, length);
    text->length = text->length + length;
}

static Text written(int32_t pieces) {
    Text source = {0};
    int64_t seed = 7;
    char number[24];
    for (int32_t index = 0; index < pieces; index = index + 1) {
        seed = seed * 48271 % 2147483647;
        int64_t choice = seed % 3;
        int32_t picked = (int32_t)(seed / 3 % 8);
        const char* piece;
        if (choice == 0) {
            piece = words[picked];
        } else if (choice == 1) {
            snprintf(number, sizeof(number), "%lld", (long long)(seed / 24 % 100000));
            piece = number;
        } else {
            piece = symbols[picked % 6];
        }
        append_text(&source, piece, (int32_t)strlen(piece));
        append_text(&source, " ", 1);
        if (index % 12 == 11) {
            append_text(&source, "\n", 1);
        }
    }
    return source;
}

static void push(Tokens* tokens, KIND kind, int32_t start, int32_t length, int32_t line, int64_t value) {
    if (tokens->count == tokens->capacity) {
        tokens->capacity = tokens->capacity ? tokens->capacity * 2 : 1024;
        tokens->kinds = realloc(tokens->kinds, sizeof(KIND) * tokens->capacity);
        tokens->starts = realloc(tokens->starts, sizeof(int32_t) * tokens->capacity);
        tokens->lengths = realloc(tokens->lengths, sizeof(int32_t) * tokens->capacity);
        tokens->lines = realloc(tokens->lines, sizeof(int32_t) * tokens->capacity);
        tokens->values = realloc(tokens->values, sizeof(int64_t) * tokens->capacity);
    }
    int32_t at = tokens->count;
    tokens->kinds[at] = kind;
    tokens->starts[at] = start;
    tokens->lengths[at] = length;
    tokens->lines[at] = line;
    tokens->values[at] = value;
    tokens->count = at + 1;
}

static Tokens tokenized(Text* source) {
    Tokens tokens = {0};
    int32_t length = source->length;
    const char* bytes = source->bytes;
    int32_t line = 1;
    int32_t position = 0;
    while (position < length) {
        int32_t code = bytes[position];
        int32_t first = position;
        if (code == 10) {
            line = line + 1;
            position = position + 1;
        } else if (code == 32) {
            position = position + 1;
        } else if (code >= 97 && code <= 122) {
            int64_t hash = 0;
            while (position < length && bytes[position] >= 97 && bytes[position] <= 122) {
                hash = (hash * 31 + bytes[position]) % 1000000007;
                position = position + 1;
            }
            push(&tokens, IDENTIFIER, first, position - first, line, hash);
        } else if (code >= 48 && code <= 57) {
            int64_t number = 0;
            while (position < length && bytes[position] >= 48 && bytes[position] <= 57) {
                number = number * 10 + bytes[position] - 48;
                position = position + 1;
            }
            push(&tokens, NUMBER, first, position - first, line, number);
        } else {
            position = position + 1;
            push(&tokens, SYMBOL, first, 1, line, code);
        }
    }
    return tokens;
}

static int32_t count_kind(const KIND* restrict kinds, int32_t count, KIND kind) {
    int32_t found = 0;
    for (int32_t index = 0; index < count; index++) found += kinds[index] == kind;
    return found;
}

static int32_t setting_of(int argument_count, char** arguments, const char* flag, int32_t otherwise) {
    size_t flag_length = strlen(flag);
    for (int index = 1; index < argument_count; index++) {
        if (strncmp(arguments[index], flag, flag_length) == 0) return atoi(arguments[index] + flag_length);
    }
    return otherwise;
}

int main(int argument_count, char** arguments) {
    int32_t pieces = setting_of(argument_count, arguments, "--pieces=", 1500000);
    int64_t start = now_nanoseconds();
    Text source = written(pieces);
    int64_t wrote = now_nanoseconds();
    Tokens tokens = tokenized(&source);
    int64_t tokenised = now_nanoseconds();
    const KIND* restrict kinds = tokens.kinds;
    const int32_t* restrict starts = tokens.starts;
    const int32_t* restrict lengths = tokens.lengths;
    const int32_t* restrict lines = tokens.lines;
    const int64_t* restrict values = tokens.values;
    int32_t identifiers = count_kind(kinds, tokens.count, IDENTIFIER);
    int32_t numbers = count_kind(kinds, tokens.count, NUMBER);
    int32_t symbol_count = count_kind(kinds, tokens.count, SYMBOL);
    int64_t number_total = 0;
    for (int32_t index = 0; index < tokens.count; index++) number_total += kinds[index] == NUMBER ? values[index] : 0;
    int64_t checksums = 0;
    for (int32_t index = 0; index < tokens.count; index++) {
        checksums += (int64_t)starts[index] + lengths[index] * 7 + lines[index] * 13 + values[index] % 1000;
    }
    int64_t finished = now_nanoseconds();
    printf("identifiers %d numbers %d symbols %d number total %lld checksums %lld\n", identifiers, numbers, symbol_count,
        (long long)number_total, (long long)checksums);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases write %lld tokenise %lld passes %lld\n", (long long)((wrote - start) / 1000),
        (long long)((tokenised - wrote) / 1000), (long long)((finished - tokenised) / 1000));
    return 0;
}
