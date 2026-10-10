/* The same work tuned by hand: the drum rendered on a thread of its own while the bass renders on the program's own
 * thread, each meter on a cache line of its own with its total and peak kept in locals and stored once, and the
 * print waiting for both. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct Meter {
    _Alignas(64) int64_t total;
    int32_t peak;
    int32_t loud_count;
    int32_t* loud;
    int32_t loud_room;
} Meter;

static Meter meters[2];

static inline void keep_loud(Meter* meter, int32_t sample) {
    if (meter->loud_count == meter->loud_room) {
        meter->loud_room = meter->loud_room == 0 ? 16 : meter->loud_room * 2;
        meter->loud = realloc(meter->loud, sizeof(int32_t) * meter->loud_room);
    }
    meter->loud[meter->loud_count++] = sample;
}

static void drum_render(void) {
    Meter* meter = &meters[0];
    int64_t total = 0;
    int32_t peak = 0;
    int32_t phase = 0;
    for (int32_t step = 0; step < 20000000; step++) {
        phase = (phase + 7) % 1000;
        total += phase;
        if (phase > peak) peak = phase;
        if (phase > 995) keep_loud(meter, phase);
    }
    meter->total = total;
    meter->peak = peak;
}

static void bass_render(void) {
    Meter* meter = &meters[1];
    int64_t total = 0;
    int32_t peak = 0;
    int32_t phase = 0;
    for (int32_t step = 0; step < 20000000; step++) {
        phase = (phase + 13) % 1000;
        int32_t sample = (phase > 500 ? 1000 - phase : phase) * 2;
        total += sample;
        if (sample > peak) peak = sample;
        if (sample > 995) keep_loud(meter, sample);
    }
    meter->total = total;
    meter->peak = peak;
}

#ifdef _WIN32
static DWORD WINAPI drum_thread(LPVOID argument) {
    (void)argument;
    drum_render();
    return 0;
}
#else
static void* drum_thread(void* argument) {
    (void)argument;
    drum_render();
    return 0;
}
#endif

int main(void) {
    int64_t start = now_nanoseconds();
#ifdef _WIN32
    HANDLE drum = CreateThread(NULL, 0, drum_thread, NULL, 0, NULL);
    bass_render();
    WaitForSingleObject(drum, INFINITE);
    CloseHandle(drum);
#else
    pthread_t drum;
    pthread_create(&drum, NULL, drum_thread, NULL);
    bass_render();
    pthread_join(drum, NULL);
#endif
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("%lld %d %d %lld %d %d\n", (long long)meters[0].total, meters[0].peak, meters[0].loud_count,
        (long long)meters[1].total, meters[1].peak, meters[1].loud_count);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(meters[0].loud);
    free(meters[1].loud);
    return 0;
}
