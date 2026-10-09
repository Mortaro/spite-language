/* naive/ written in C the way it reads: a growable array of bytes for the List<Byte>, and base64.encode written as
 * the library writes it, each byte read and each digit written through a function of its own, the text a new
 * allocation per call. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Bytes {
    uint8_t* items;
    int32_t count;
    int32_t capacity;
} Bytes;

typedef struct Text {
    char* characters;
    int64_t length;
} Text;

static const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void bytes_append(Bytes* bytes, uint8_t value) {
    if (bytes->count == bytes->capacity) {
        bytes->capacity = bytes->capacity == 0 ? 4 : bytes->capacity * 2;
        bytes->items = realloc(bytes->items, bytes->capacity);
    }
    bytes->items[bytes->count] = value;
    bytes->count = bytes->count + 1;
}

static uint8_t read_byte(const uint8_t* address, int64_t offset) {
    return address[offset];
}

static void write_byte(char* address, int64_t offset, uint8_t value) {
    address[offset] = (char)value;
}

static int64_t write_digit(char* address, int64_t at, int32_t group, int32_t shift) {
    int32_t value = (group >> shift) & 63;
    write_byte(address, at, (uint8_t)alphabet[value]);
    return at + 1;
}

static Text encode(Bytes* bytes) {
    int32_t count = bytes->count;
    int64_t room = (count + 2) / 3 * 4;
    char* address = malloc(room + 1);
    int64_t written = 0;
    for (int32_t index = 0; index < count; index = index + 3) {
        int32_t group = read_byte(bytes->items, index) << 16;
        int32_t present = count - index;
        if (present > 1) group = group | (read_byte(bytes->items, index + 1) << 8);
        if (present > 2) group = group | read_byte(bytes->items, index + 2);
        written = write_digit(address, written, group, 18);
        written = write_digit(address, written, group, 12);
        if (present > 1) {
            written = write_digit(address, written, group, 6);
        } else {
            write_byte(address, written, '=');
            written = written + 1;
        }
        if (present > 2) {
            written = write_digit(address, written, group, 0);
        } else {
            write_byte(address, written, '=');
            written = written + 1;
        }
    }
    address[written] = 0;
    Text text = {address, written};
    return text;
}

static int64_t encode_rounds(Bytes* bytes) {
    int64_t total = 0;
    for (int32_t round = 0; round < 100; round = round + 1) {
        bytes->items[round] = (uint8_t)round;
        Text encoded = encode(bytes);
        total = total + encoded.length + encoded.characters[round];
        free(encoded.characters);
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    Bytes bytes = {NULL, 0, 0};
    for (int32_t index = 0; index < 100000; index = index + 1) bytes_append(&bytes, (uint8_t)(index * 7 % 256));
    int64_t total = encode_rounds(&bytes);
    int64_t microseconds = microseconds_since(start);
    printf("total %lld\n", (long long)total);
    print_microseconds(microseconds);
    free(bytes.items);
    return 0;
}
