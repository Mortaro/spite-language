/* The same work written by hand as the state machines the compiler writes: each napper a small frame holding where
 * it stopped, its number, its local and its deadline, on one thread with no stack of its own, and the loop sleeping
 * until the nearest deadline and stepping every napper whose time has come. */
#include <stdint.h>
#include <stdio.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
static void sleep_milliseconds(int32_t milliseconds) { Sleep((DWORD)milliseconds); }
#else
#include <time.h>
static void sleep_milliseconds(int32_t milliseconds) {
    struct timespec wait = {milliseconds / 1000, (long)(milliseconds % 1000) * 1000000L};
    nanosleep(&wait, NULL);
}
#endif

#define NAPPER_COUNT 8

typedef struct NapFrame {
    int32_t state;
    int32_t number;
    int32_t doubled;
    int32_t result;
    int64_t deadline;
} NapFrame;

/* runs a napper until it reaches a sleep that is not over; answers 1 once it has returned */
static int nap_step(NapFrame* frame, int64_t now) {
    switch (frame->state) {
        case 0:
            frame->deadline = now + 2000000;
            frame->state = 1;
            return 0;
        case 1:
            if (now < frame->deadline) return 0;
            frame->doubled = frame->number * 2;
            frame->deadline = now + 2000000;
            frame->state = 2;
            return 0;
        case 2:
            if (now < frame->deadline) return 0;
            frame->result = frame->doubled + 1;
            frame->state = 3;
            return 1;
        default:
            return 1;
    }
}

int main(void) {
    NapFrame frames[NAPPER_COUNT];
    int32_t running = NAPPER_COUNT;
    for (int32_t index = 0; index < NAPPER_COUNT; index++) {
        frames[index] = (NapFrame){0, index, 0, 0, 0};
        nap_step(&frames[index], now_nanoseconds());
    }
    while (running > 0) {
        int64_t now = now_nanoseconds();
        int64_t nearest = INT64_MAX;
        running = 0;
        for (int32_t index = 0; index < NAPPER_COUNT; index++) {
            if (frames[index].state == 3) continue;
            if (!nap_step(&frames[index], now)) {
                running++;
                if (frames[index].deadline < nearest) nearest = frames[index].deadline;
            }
        }
        if (running > 0 && nearest > now) sleep_milliseconds((int32_t)((nearest - now + 999999) / 1000000));
    }
    int32_t total = 0;
    for (int32_t index = 0; index < NAPPER_COUNT; index++) total += frames[index].result;
    printf("total %d\n", total);
    return 0;
}
