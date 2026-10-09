/* The same work tuned by hand: each part is saved on a thread of its own, started on its frame, so the frames go on
 * drawing while the saves sleep and write; the threads are joined once the sixty frames are drawn. */
#include <stdint.h>
#include <stdio.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
static void sleep_milliseconds(int32_t milliseconds) { Sleep((DWORD)milliseconds); }
#else
#include <pthread.h>
#include <time.h>
static void sleep_milliseconds(int32_t milliseconds) {
    struct timespec wait = {milliseconds / 1000, (long)(milliseconds % 1000) * 1000000L};
    nanosleep(&wait, NULL);
}
#endif

static int32_t frames = 0;
static int64_t checksum = 0;

#ifdef _WIN32
static DWORD WINAPI save_part(LPVOID argument) {
#else
static void* save_part(void* argument) {
#endif
    int32_t part = (int32_t)(intptr_t)argument;
    sleep_milliseconds(20);
    char path[64];
    snprintf(path, sizeof(path), ".spite/frame_save_%d.txt", part);
    FILE* file = fopen(path, "wb");
    fprintf(file, "part %d", part);
    fclose(file);
    return 0;
}

static void draw(void) {
    int64_t sum = 0;
    for (int32_t index = 0; index < 20000; index = index + 1) {
        sum = sum + (int64_t)index * frames % 1000;
    }
    checksum = checksum + sum;
}

int main(void) {
    int64_t start = now_nanoseconds();
#ifdef _WIN32
    HANDLE savers[8];
#else
    pthread_t savers[8];
#endif
    while (frames < 60) {
        frames = frames + 1;
        draw();
        if (frames <= 8) {
#ifdef _WIN32
            savers[frames - 1] = CreateThread(NULL, 0, save_part, (LPVOID)(intptr_t)(frames - 1), 0, NULL);
#else
            pthread_create(&savers[frames - 1], NULL, save_part, (void*)(intptr_t)(frames - 1));
#endif
        }
        sleep_milliseconds(1);
    }
    for (int32_t index = 0; index < 8; index = index + 1) {
#ifdef _WIN32
        WaitForSingleObject(savers[index], INFINITE);
        CloseHandle(savers[index]);
#else
        pthread_join(savers[index], NULL);
#endif
    }
    int64_t microseconds = microseconds_since(start);
    printf("frames %d parts saved %d checksum %lld\n", frames, 8, (long long)checksum);
    print_microseconds(microseconds);
    return 0;
}
