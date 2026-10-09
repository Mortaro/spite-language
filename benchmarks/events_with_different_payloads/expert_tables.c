/* The events grouped by the payload they carry (one table per member of the union): each event is appended to the
 * columns of its own kind as it is made, and each table is handled by a loop of its own with no switch. The order of
 * the events of one kind is kept, which the keys need (each key's code is folded into the text typed so far, so their
 * order is part of the answer) and the resizes need (the last one wins); the order between kinds is lost, which
 * nothing can see, since the three handlers write nothing in common.
 * --events=N, --density=P and --rounds=N, as naive/ reads them. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

static int32_t setting(int argument_count, char** arguments, const char* name, int32_t otherwise) {
    size_t length = strlen(name);
    for (int index = 1; index < argument_count; index++) {
        if (strncmp(arguments[index], name, length) == 0 && arguments[index][length] == '=') {
            return atoi(arguments[index] + length + 1);
        }
    }
    return otherwise;
}

int main(int argument_count, char** arguments) {
    int32_t count = setting(argument_count, arguments, "--events", 100000);
    int32_t density = setting(argument_count, arguments, "--density", 10);
    int32_t rounds = setting(argument_count, arguments, "--rounds", 40);
    int64_t start = now_nanoseconds();
    size_t room = sizeof(int32_t) * (count ? count : 1);
    int32_t* restrict click_x = malloc(room);
    int32_t* restrict click_y = malloc(room);
    int32_t* restrict click_button = malloc(room);
    int32_t* restrict key_code = malloc(room);
    int32_t* restrict resize_width = malloc(room);
    int32_t* restrict resize_height = malloc(room);
    int64_t clicks = 0;
    uint64_t typed = 0;
    int32_t width = 0;
    int32_t height = 0;
    int32_t resizes = 0;
    int64_t seed = 42;
    int64_t making = 0;
    int64_t handling = 0;
    for (int32_t round = 0; round < rounds; round++) {
        int64_t round_start = now_nanoseconds();
        int32_t click_count = 0;
        int32_t key_count = 0;
        int32_t resize_count = 0;
        for (int32_t index = 0; index < count; index++) {
            seed = seed * 48271 % 2147483647;
            if (seed / 3 % 100 < density) {
                resize_width[resize_count] = (int32_t)(seed / 7 % 3840 + 1);
                resize_height[resize_count] = (int32_t)(seed / 26880 % 2160 + 1);
                resize_count++;
            } else if (seed % 2 == 0) {
                click_x[click_count] = (int32_t)(seed / 7 % 1920);
                click_y[click_count] = (int32_t)(seed / 13440 % 1080);
                click_button[click_count] = (int32_t)(seed / 5 % 3);
                click_count++;
            } else {
                key_code[key_count] = (int32_t)(seed / 7 % 128);
                key_count++;
            }
        }
        int64_t made = now_nanoseconds();
        for (int32_t row = 0; row < click_count; row++) clicks = clicks + click_x[row] * click_button[row] + click_y[row];
        for (int32_t row = 0; row < key_count; row++) typed = typed * 31 + (uint64_t)(int64_t)key_code[row];
        for (int32_t row = 0; row < resize_count; row++) {
            width = resize_width[row];
            height = resize_height[row];
            resizes = resizes + 1;
        }
        making += made - round_start;
        handling += now_nanoseconds() - made;
    }
    int64_t finished = now_nanoseconds();
    printf("clicks %lld typed %lld width %d height %d resizes %d\n", (long long)clicks, (long long)(int64_t)typed, width,
           height, resizes);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld handle %lld\n", (long long)(making / 1000), (long long)(handling / 1000));
    return 0;
}
