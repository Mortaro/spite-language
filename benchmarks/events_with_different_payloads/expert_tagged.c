/* The events inline in one array of tagged unions (each slot as large as the largest payload, with its tag), refilled
 * in place each round with nothing allocated, and handled by the same switch in the order they arrived: what an
 * expert writes first, and what the compiler could write for a list of a union whose items never leave it.
 * --events=N, --density=P and --rounds=N, as naive/ reads them. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

enum Kind { click_kind, key_kind, resize_kind };

typedef struct Event {
    uint8_t kind;
    int32_t first;    /* a click's x, a key's code, a resize's width */
    int32_t second;   /* a click's y, a resize's height */
    int32_t third;    /* a click's button */
} Event;

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
    Event* events = malloc(sizeof(Event) * (count ? count : 1));
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
            Event* event = &events[index];
            if (seed / 3 % 100 < density) {
                event->kind = resize_kind;
                event->first = (int32_t)(seed / 7 % 3840 + 1);
                event->second = (int32_t)(seed / 26880 % 2160 + 1);
            } else if (seed % 2 == 0) {
                event->kind = click_kind;
                event->first = (int32_t)(seed / 7 % 1920);
                event->second = (int32_t)(seed / 13440 % 1080);
                event->third = (int32_t)(seed / 5 % 3);
            } else {
                event->kind = key_kind;
                event->first = (int32_t)(seed / 7 % 128);
            }
        }
        int64_t made = now_nanoseconds();
        for (int32_t index = 0; index < count; index++) {
            const Event* event = &events[index];
            switch (event->kind) {
                case click_kind: clicks = clicks + event->first * event->third + event->second; break;
                case key_kind: typed = typed * 31 + (uint64_t)(int64_t)event->first; break;
                default:
                    width = event->first;
                    height = event->second;
                    resizes = resizes + 1;
                    break;
            }
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
