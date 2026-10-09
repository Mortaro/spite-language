/* naive/ written in C the way a C programmer writes it: each event a malloc'd struct with a tag saying which payload
 * it carries, a growable array of pointers cleared (and every event freed) each round, and the same loop that
 * switches on each event's tag in the order the events arrived.
 * --events=N (per round), --density=P (the percent of events that are a resize) and --rounds=N. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

enum Kind { click_kind, key_kind, resize_kind };

typedef struct Event {
    enum Kind kind;
} Event;

typedef struct Click {
    Event event;
    int32_t x;
    int32_t y;
    int32_t button;
} Click;

typedef struct Key {
    Event event;
    int32_t code;
} Key;

typedef struct Resize {
    Event event;
    int32_t width;
    int32_t height;
} Resize;

typedef struct Events {
    Event** items;
    int32_t count;
    int32_t capacity;
} Events;

static int64_t clicks = 0;
static int64_t typed = 0;
static int32_t width = 0;
static int32_t height = 0;
static int32_t resizes = 0;

static void append(Events* events, Event* event) {
    if (events->count == events->capacity) {
        events->capacity = events->capacity ? events->capacity * 2 : 8;
        events->items = realloc(events->items, sizeof(Event*) * events->capacity);
    }
    events->items[events->count] = event;
    events->count = events->count + 1;
}

static Event* made_event(int64_t seed, int32_t density) {
    if (seed / 3 % 100 < density) {
        Resize* resize = malloc(sizeof(Resize));
        resize->event.kind = resize_kind;
        resize->width = (int32_t)(seed / 7 % 3840 + 1);
        resize->height = (int32_t)(seed / 26880 % 2160 + 1);
        return &resize->event;
    }
    if (seed % 2 == 0) {
        Click* click = malloc(sizeof(Click));
        click->event.kind = click_kind;
        click->x = (int32_t)(seed / 7 % 1920);
        click->y = (int32_t)(seed / 13440 % 1080);
        click->button = (int32_t)(seed / 5 % 3);
        return &click->event;
    }
    Key* key = malloc(sizeof(Key));
    key->event.kind = key_kind;
    key->code = (int32_t)(seed / 7 % 128);
    return &key->event;
}

static void handle(Event* event) {
    switch (event->kind) {
        case click_kind: {
            Click* click = (Click*)event;
            clicks = clicks + click->x * click->button + click->y;
            break;
        }
        case key_kind: {
            Key* key = (Key*)event;
            typed = (int64_t)((uint64_t)typed * 31 + (uint64_t)(int64_t)key->code);
            break;
        }
        case resize_kind: {
            Resize* resize = (Resize*)event;
            width = resize->width;
            height = resize->height;
            resizes = resizes + 1;
            break;
        }
    }
}

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
    Events events = {0};
    int64_t seed = 42;
    int64_t making = 0;
    int64_t handling = 0;
    for (int32_t round = 0; round < rounds; round++) {
        int64_t round_start = now_nanoseconds();
        for (int32_t index = 0; index < events.count; index++) free(events.items[index]);
        events.count = 0;
        for (int32_t index = 0; index < count; index++) {
            seed = seed * 48271 % 2147483647;
            append(&events, made_event(seed, density));
        }
        int64_t made = now_nanoseconds();
        for (int32_t index = 0; index < events.count; index++) handle(events.items[index]);
        making += made - round_start;
        handling += now_nanoseconds() - made;
    }
    int64_t finished = now_nanoseconds();
    printf("clicks %lld typed %lld width %d height %d resizes %d\n", (long long)clicks, (long long)typed, width, height,
           resizes);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld handle %lld\n", (long long)(making / 1000), (long long)(handling / 1000));
    return 0;
}
