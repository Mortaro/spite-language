/* The same work tuned by hand: the orbits side by side in one array, cut into one band per processor, each band on
 * a thread of its own (the last on the program's own thread) running all ten ticks of its orbits back to back with
 * the angle and the count in registers, and the sum waiting for every band. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

typedef struct Orbit {
    float angle;
    float speed;
    int32_t turns;
} Orbit;

typedef struct Band {
    Orbit* orbits;
    int32_t first;
    int32_t end;
} Band;

static void run_band(Band* band) {
    for (int32_t index = band->first; index < band->end; index++) {
        Orbit* orbit = &band->orbits[index];
        float angle = orbit->angle;
        float speed = orbit->speed * 0.001f;
        int32_t turns = orbit->turns;
        for (int32_t tick = 0; tick < 10; tick++) {
            for (int32_t step = 0; step < 1000; step++) {
                angle = angle + speed;
                if (angle > 6.28318f) {
                    angle = angle - 6.28318f;
                    turns++;
                }
            }
        }
        orbit->angle = angle;
        orbit->turns = turns;
    }
}

#ifdef _WIN32
static DWORD WINAPI band_thread(LPVOID argument) {
    run_band((Band*)argument);
    return 0;
}
#else
static void* band_thread(void* argument) {
    run_band((Band*)argument);
    return 0;
}
#endif

int main(void) {
    int32_t count = 100000;
    Orbit* orbits = malloc(sizeof(Orbit) * count);
    for (int32_t index = 0; index < count; index++) {
        orbits[index].angle = 0.0f;
        orbits[index].speed = (float)(index % 17 + 1);
        orbits[index].turns = 0;
    }
#ifdef _WIN32
    SYSTEM_INFO system;
    GetSystemInfo(&system);
    int32_t bands = (int32_t)system.dwNumberOfProcessors;
#else
    int32_t bands = (int32_t)sysconf(_SC_NPROCESSORS_ONLN);
#endif
    if (bands < 1) bands = 1;
    if (bands > 64) bands = 64;
    Band band[64];
    int64_t start = now_nanoseconds();
    for (int32_t index = 0; index < bands; index++) {
        band[index].orbits = orbits;
        band[index].first = (int32_t)((int64_t)count * index / bands);
        band[index].end = (int32_t)((int64_t)count * (index + 1) / bands);
    }
#ifdef _WIN32
    HANDLE threads[64];
    for (int32_t index = 0; index < bands - 1; index++) threads[index] = CreateThread(NULL, 0, band_thread, &band[index], 0, NULL);
    run_band(&band[bands - 1]);
    for (int32_t index = 0; index < bands - 1; index++) {
        WaitForSingleObject(threads[index], INFINITE);
        CloseHandle(threads[index]);
    }
#else
    pthread_t threads[64];
    for (int32_t index = 0; index < bands - 1; index++) pthread_create(&threads[index], NULL, band_thread, &band[index]);
    run_band(&band[bands - 1]);
    for (int32_t index = 0; index < bands - 1; index++) pthread_join(threads[index], NULL);
#endif
    int64_t microseconds = microseconds_since(start);
    int32_t turns = 0;
    for (int32_t index = 0; index < count; index++) turns += orbits[index].turns;
    printf("turns %d\n", turns);
    print_microseconds(microseconds);
    free(orbits);
    return 0;
}
