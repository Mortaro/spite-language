/* The events in arrival order as columns (a column of tags and one column for each payload field, shared by the
 * kinds that have a field in that place: a column with holes), refilled in place each round, and handled by one loop
 * over the tags in arrival order with a select per kind instead of a switch.
 * --events=N, --density=P and --rounds=N, as naive/ reads them. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

enum Kind { click_kind, key_kind, resize_kind };

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
    uint8_t* restrict kind = malloc(count ? count : 1);
    int32_t* restrict first = malloc(room);
    int32_t* restrict second = malloc(room);
    int32_t* restrict third = malloc(room);
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
        for (int32_t index = 0; index < count; index++) {
            seed = seed * 48271 % 2147483647;
            if (seed / 3 % 100 < density) {
                kind[index] = resize_kind;
                first[index] = (int32_t)(seed / 7 % 3840 + 1);
                second[index] = (int32_t)(seed / 26880 % 2160 + 1);
                third[index] = 0;
            } else if (seed % 2 == 0) {
                kind[index] = click_kind;
                first[index] = (int32_t)(seed / 7 % 1920);
                second[index] = (int32_t)(seed / 13440 % 1080);
                third[index] = (int32_t)(seed / 5 % 3);
            } else {
                kind[index] = key_kind;
                first[index] = (int32_t)(seed / 7 % 128);
                second[index] = 0;
                third[index] = 0;
            }
        }
        int64_t made = now_nanoseconds();
        for (int32_t index = 0; index < count; index++) {
            int32_t is_click = kind[index] == click_kind;
            int32_t is_key = kind[index] == key_kind;
            int32_t is_resize = kind[index] == resize_kind;
            clicks = clicks + (is_click ? first[index] * third[index] + second[index] : 0);
            typed = is_key ? typed * 31 + (uint64_t)(int64_t)first[index] : typed;
            width = is_resize ? first[index] : width;
            height = is_resize ? second[index] : height;
            resizes = resizes + is_resize;
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
