/* The clock every case's C reads, the one a Spite program's Benchmark(work) reads through clock.elapsed_nanoseconds():
 * the answer goes to the standard output and "microseconds <n>" to the error output, so run.sh compares answers and
 * times only the work, not the start of a process. A program reads start = now_nanoseconds() before its work,
 * microseconds_since(start) after it, and prints the line with print_microseconds once it has printed its answer. */
#ifndef CASES_CLOCK_H
#define CASES_CLOCK_H
#include <stdint.h>
#include <stdio.h>
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
/* the whole microseconds since a reading, as Benchmark's duration.total('microseconds') is in Spite */
static inline int64_t microseconds_since(int64_t start) {
    return (now_nanoseconds() - start) / 1000;
}
/* the line run.sh and scripts/cases/check.sh read from the error output */
static inline void print_microseconds(int64_t microseconds) {
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
}
#endif
