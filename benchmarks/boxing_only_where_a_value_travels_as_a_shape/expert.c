/* The same work tuned by hand: every value is held in place in one array of tagged values (the class, and the
 * number, the Boolean, or the text's 15 bytes and length inline), nothing boxed and nothing allocated per value; each
 * round writes each value's text into a buffer on the stack, digit by digit for a number, and adds up the lengths. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

#define VALUE_COUNT 90000

typedef struct Value {
    uint8_t class;
    uint8_t length;
    union {
        int32_t integer;
        int32_t boolean;
        char text[15];
    } value;
} Value;

static int32_t write_digits(char* at, int32_t number) {
    char digits[12];
    int32_t count = 0;
    do {
        digits[count++] = (char)('0' + number % 10);
        number /= 10;
    } while (number > 0);
    for (int32_t index = 0; index < count; index++) at[index] = digits[count - 1 - index];
    return count;
}

static int32_t measure(const Value* values) {
    int32_t total = 0;
    char text[16];
    for (int32_t index = 0; index < VALUE_COUNT; index++) {
        int32_t length;
        const Value* value = &values[index];
        if (value->class == 0) {
            length = write_digits(text, value->value.integer);
        } else if (value->class == 1) {
            length = value->value.boolean ? 4 : 5;
            memcpy(text, value->value.boolean ? "true" : "false", (size_t)length);
        } else {
            length = value->length;
            memcpy(text, value->value.text, (size_t)length);
        }
        __asm__ volatile("" : : "r"(text) : "memory");   /* the text is made, as the program asks, not only measured */
        total += length;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    Value* values = malloc(sizeof(Value) * VALUE_COUNT);
    for (int32_t index = 0; index < VALUE_COUNT; index++) {
        int32_t kind = index % 3;
        values[index].class = (uint8_t)kind;
        if (kind == 0) {
            values[index].value.integer = index;
        } else if (kind == 1) {
            values[index].value.boolean = index % 2 == 0;
        } else {
            memcpy(values[index].value.text, "item ", 5);
            values[index].length = (uint8_t)(5 + write_digits(values[index].value.text + 5, index));
        }
    }
    int64_t total = 0;
    for (int32_t round = 0; round < 20; round++) total += measure(values);
    int64_t microseconds = microseconds_since(start);
    printf("total %lld\n", (long long)total);
    print_microseconds(microseconds);
    free(values);
    return 0;
}
