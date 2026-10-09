/* naive/ written in C the way a C programmer writes it: the text built in a growing buffer, then tokenised into a
 * struct per token, one malloc each, held by a growable array of pointers; then the same passes: a count of each
 * kind, the total of the numbers, the sum of each token's checksum. --pieces=N sets how many pieces the text has. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

typedef enum TokenKind { IDENTIFIER, NUMBER, SYMBOL } TokenKind;

typedef struct Token {
    TokenKind kind;
    int32_t start;
    int32_t length;
    int32_t line;
    int64_t value;
} Token;

typedef struct Tokens {
    Token** items;
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

static Token* make_token(TokenKind kind, int32_t start, int32_t length, int32_t line, int64_t value) {
    Token* token = malloc(sizeof(Token));
    token->kind = kind;
    token->start = start;
    token->length = length;
    token->line = line;
    token->value = value;
    return token;
}

static void append(Tokens* tokens, Token* token) {
    if (tokens->count == tokens->capacity) {
        tokens->capacity = tokens->capacity ? tokens->capacity * 2 : 8;
        tokens->items = realloc(tokens->items, sizeof(Token*) * tokens->capacity);
    }
    tokens->items[tokens->count] = token;
    tokens->count = tokens->count + 1;
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
            append(&tokens, make_token(IDENTIFIER, first, position - first, line, hash));
        } else if (code >= 48 && code <= 57) {
            int64_t number = 0;
            while (position < length && bytes[position] >= 48 && bytes[position] <= 57) {
                number = number * 10 + bytes[position] - 48;
                position = position + 1;
            }
            append(&tokens, make_token(NUMBER, first, position - first, line, number));
        } else {
            position = position + 1;
            append(&tokens, make_token(SYMBOL, first, 1, line, code));
        }
    }
    return tokens;
}

static int32_t count_kind(Tokens* tokens, TokenKind kind) {
    int32_t count = 0;
    for (int32_t index = 0; index < tokens->count; index = index + 1) {
        if (tokens->items[index]->kind == kind) count = count + 1;
    }
    return count;
}

static int64_t total_of_numbers(Tokens* tokens) {
    int64_t total = 0;
    for (int32_t index = 0; index < tokens->count; index = index + 1) {
        Token* token = tokens->items[index];
        if (token->kind == NUMBER) total = total + token->value;
    }
    return total;
}

static int64_t checksum(Token* token) {
    int64_t total = token->start;
    total = total + token->length * 7 + token->line * 13;
    total = total + token->value % 1000;
    return total;
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
    int32_t identifiers = count_kind(&tokens, IDENTIFIER);
    int32_t numbers = count_kind(&tokens, NUMBER);
    int32_t symbol_count = count_kind(&tokens, SYMBOL);
    int64_t number_total = total_of_numbers(&tokens);
    int64_t checksums = 0;
    for (int32_t index = 0; index < tokens.count; index = index + 1) {
        checksums = checksums + checksum(tokens.items[index]);
    }
    int64_t finished = now_nanoseconds();
    printf("identifiers %d numbers %d symbols %d number total %lld checksums %lld\n", identifiers, numbers, symbol_count,
        (long long)number_total, (long long)checksums);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases write %lld tokenise %lld passes %lld\n", (long long)((wrote - start) / 1000),
        (long long)((tokenised - wrote) / 1000), (long long)((finished - tokenised) / 1000));
    return 0;
}
