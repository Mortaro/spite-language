/* naive/ written in C the way it reads: sixty frames, each drawing and sleeping a millisecond, and on each of the
 * first eight a part saved by a call that sleeps twenty milliseconds and writes a file before it returns, so the
 * frame waits for it. */
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

static int32_t next_part = 0;
static int32_t saved = 0;
static int32_t frames = 0;
static int64_t checksum = 0;

static void save_part(void) {
    int32_t part = next_part;
    next_part = next_part + 1;
    sleep_milliseconds(20);
    char path[64];
    snprintf(path, sizeof(path), ".spite/frame_save_%d.txt", part);
    FILE* file = fopen(path, "wb");
    fprintf(file, "part %d", part);
    fclose(file);
    saved = saved + 1;
}

static void draw(void) {
    for (int32_t index = 0; index < 20000; index = index + 1) {
        checksum = checksum + (int64_t)index * frames % 1000;
    }
}

int main(void) {
    int64_t start = now_nanoseconds();
    while (frames < 60) {
        frames = frames + 1;
        draw();
        if (frames <= 8) save_part();
        sleep_milliseconds(1);
    }
    int64_t microseconds = microseconds_since(start);
    printf("frames %d parts saved %d checksum %lld\n", frames, saved, (long long)checksum);
    print_microseconds(microseconds);
    return 0;
}
