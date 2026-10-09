/* The same work tuned by hand: every label fits 16 bytes, so the list is one array of 16-byte slots made once at
 * its size, each label written into its slot digit by digit with its length in the last byte, and nothing else
 * allocated. Each round still writes all 100 000 labels and reads every one back. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

#define LABEL_COUNT 100000

typedef struct Label {
    char bytes[15];
    uint8_t length;
} Label;

static char* write_digits(char* at, int32_t number) {
    char digits[12];
    int32_t count = 0;
    do {
        digits[count++] = (char)('0' + number % 10);
        number /= 10;
    } while (number > 0);
    while (count > 0) *at++ = digits[--count];
    return at;
}

static void make_labels(Label* labels, int32_t round) {
    for (int32_t index = 0; index < LABEL_COUNT; index++) {
        char* at = labels[index].bytes;
        *at++ = 'r';
        at = write_digits(at, round);
        *at++ = ' ';
        *at++ = 'n';
        at = write_digits(at, index);
        labels[index].length = (uint8_t)(at - labels[index].bytes);
    }
}

static int32_t checksum(const Label* labels) {
    int32_t total = 0;
    for (int32_t index = 0; index < LABEL_COUNT; index++) {
        int32_t length = labels[index].length;
        total += length + (uint8_t)labels[index].bytes[length - 1];
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    Label* labels = malloc(sizeof(Label) * LABEL_COUNT);
    int64_t total = 0;
    for (int32_t round = 0; round < 20; round++) {
        make_labels(labels, round);
        total += checksum(labels);
    }
    int64_t microseconds = microseconds_since(start);
    printf("total %lld\n", (long long)total);
    print_microseconds(microseconds);
    free(labels);
    return 0;
}
