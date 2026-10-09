/* naive/ written in C the way it reads: a growable array of bytes for the List<Byte>, the big-endian numbers put
 * together byte by byte as a C programmer writes them, and the decode loop reading through the list's pointer and
 * count, with no checks. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct ByteList {
    uint8_t* items;
    int32_t count;
    int32_t capacity;
} ByteList;

static ByteList* list_make(void) {
    ByteList* list = malloc(sizeof(ByteList));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void list_append(ByteList* list, uint8_t value) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, list->capacity);
    }
    list->items[list->count] = value;
    list->count = list->count + 1;
}

static void list_free(ByteList* list) {
    free(list->items);
    free(list);
}

static void append_integer_big_endian(ByteList* list, int32_t value) {
    uint32_t bits = (uint32_t)value;
    list_append(list, (uint8_t)(bits >> 24));
    list_append(list, (uint8_t)(bits >> 16));
    list_append(list, (uint8_t)(bits >> 8));
    list_append(list, (uint8_t)bits);
}

static void append_short_big_endian(ByteList* list, int16_t value) {
    uint16_t bits = (uint16_t)value;
    list_append(list, (uint8_t)(bits >> 8));
    list_append(list, (uint8_t)bits);
}

static int32_t read_integer_big_endian(ByteList* list, int32_t position) {
    uint8_t* at = list->items + position;
    return (int32_t)(((uint32_t)at[0] << 24) | ((uint32_t)at[1] << 16) | ((uint32_t)at[2] << 8) | (uint32_t)at[3]);
}

static int16_t read_short_big_endian(ByteList* list, int32_t position) {
    uint8_t* at = list->items + position;
    return (int16_t)(((uint16_t)at[0] << 8) | (uint16_t)at[1]);
}

static int64_t decode(ByteList* records) {
    int64_t total = 0;
    int32_t position = 0;
    while (position + 15 < records->count) {
        int32_t identity = read_integer_big_endian(records, position);
        int32_t across = read_integer_big_endian(records, position + 4);
        int32_t down = read_integer_big_endian(records, position + 8);
        int16_t kind = read_short_big_endian(records, position + 12);
        int16_t flags = read_short_big_endian(records, position + 14);
        total = total + identity + across + down * kind + flags;
        position = position + 16;
    }
    return total;
}

static int64_t rounds(ByteList* records) {
    int64_t total = 0;
    for (int32_t round = 0; round < 300; round = round + 1) {
        int64_t decoded = decode(records);
        total = total + decoded + round;
    }
    return total;
}

int main(void) {
    ByteList* records = list_make();
    for (int32_t index = 0; index < 100000; index = index + 1) {
        append_integer_big_endian(records, index);
        append_integer_big_endian(records, index * 7 - 50000);
        append_integer_big_endian(records, index % 1000);
        append_short_big_endian(records, (int16_t)(index % 5));
        append_short_big_endian(records, (int16_t)(index % 3));
    }
    int64_t start = now_nanoseconds();
    int64_t total = rounds(records);
    int64_t microseconds = microseconds_since(start);
    printf("total %lld\n", (long long)total);
    print_microseconds(microseconds);
    list_free(records);
    return 0;
}
