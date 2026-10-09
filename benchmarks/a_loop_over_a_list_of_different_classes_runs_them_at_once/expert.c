/* The same work tuned by hand: each voice rendered on a thread of its own (two started, the third on the program's
 * own thread), each keeping its phase and level in locals and storing them once, on a cache line of its own, and the
 * print waiting for all three. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct Voice {
    _Alignas(64) int64_t level;
    int32_t phase;
} Voice;

static Voice voices[3];

static void sine_render(void) {
    int64_t level = 0;
    int32_t phase = 0;
    for (int32_t sample = 0; sample < 30000000; sample++) {
        phase = (phase + 7) % 1000;
        level += phase > 500 ? 1000 - phase : phase;
    }
    voices[0].level = level;
    voices[0].phase = phase;
}

static void square_render(void) {
    int64_t level = 0;
    int32_t phase = 0;
    for (int32_t sample = 0; sample < 30000000; sample++) {
        phase = (phase + 11) % 1000;
        level += phase < 500 ? 3 : 1;
    }
    voices[1].level = level;
    voices[1].phase = phase;
}

static void saw_render(void) {
    int64_t level = 0;
    int32_t phase = 0;
    for (int32_t sample = 0; sample < 30000000; sample++) {
        phase = (phase + 13) % 1000;
        level += phase % 97;
    }
    voices[2].level = level;
    voices[2].phase = phase;
}

#ifdef _WIN32
static DWORD WINAPI sine_thread(LPVOID argument) {
    (void)argument;
    sine_render();
    return 0;
}

static DWORD WINAPI square_thread(LPVOID argument) {
    (void)argument;
    square_render();
    return 0;
}
#else
static void* sine_thread(void* argument) {
    (void)argument;
    sine_render();
    return 0;
}

static void* square_thread(void* argument) {
    (void)argument;
    square_render();
    return 0;
}
#endif

int main(void) {
    int64_t start = now_nanoseconds();
#ifdef _WIN32
    HANDLE sine = CreateThread(NULL, 0, sine_thread, NULL, 0, NULL);
    HANDLE square = CreateThread(NULL, 0, square_thread, NULL, 0, NULL);
    saw_render();
    WaitForSingleObject(sine, INFINITE);
    WaitForSingleObject(square, INFINITE);
    CloseHandle(sine);
    CloseHandle(square);
#else
    pthread_t sine;
    pthread_t square;
    pthread_create(&sine, NULL, sine_thread, NULL);
    pthread_create(&square, NULL, square_thread, NULL);
    saw_render();
    pthread_join(sine, NULL);
    pthread_join(square, NULL);
#endif
    int64_t microseconds = microseconds_since(start);
    printf("%lld %lld %lld\n", (long long)voices[0].level, (long long)voices[1].level, (long long)voices[2].level);
    print_microseconds(microseconds);
    return 0;
}
