/* The clock every case's C reads, the one a Spite program reads through clock.elapsed_nanoseconds(): the answer goes
 * to the standard output and "microseconds <n>" to the error output, so run.sh compares answers and times only the
 * work, not the start of a process. */
#ifndef CASES_CLOCK_H
#define CASES_CLOCK_H
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
static int64_t now_nanoseconds(void) {
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    return (int64_t)((double)counter.QuadPart * 1000000000.0 / (double)frequency.QuadPart);
}
#else
#include <time.h>
static int64_t now_nanoseconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)now.tv_sec * 1000000000 + now.tv_nsec;
}
#endif
#endif
