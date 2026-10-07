/* The same work tuned by hand: the transforms as one column of plain numbers, and no lock at all, since the
 * program's own shape makes it safe: the column is written only between ticks, after every thread of the last tick
 * has been joined and before the next one starts, so the threads only ever read it. Each thread sums the rows in
 * a loop the C compiler vectorises. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#define ROW_TOTAL 15000
#define SYSTEM_COUNT 8
#define TICK_COUNT 20

static int32_t* across;
static int32_t across_count;

typedef struct Work {
    int32_t rows;
    int32_t result;
} Work;

static int32_t sum_rows(int32_t rows) {
    int32_t total = 0;
    for (int32_t row = 0; row < rows; row++) {
        total += across[row];
    }
    return total;
}

#ifdef _WIN32
static DWORD WINAPI work_thread(LPVOID argument) {
    Work* work = argument;
    work->result = sum_rows(work->rows);
    return 0;
}
#else
static void* work_thread(void* argument) {
    Work* work = argument;
    work->result = sum_rows(work->rows);
    return NULL;
}
#endif

int main(void) {
    int64_t start = now_nanoseconds();
    across = malloc(sizeof(int32_t) * (ROW_TOTAL + TICK_COUNT));
    for (int32_t row = 0; row < ROW_TOTAL; row++) {
        across[row] = row % 1000;
    }
    across_count = ROW_TOTAL;
    int64_t total = 0;
    for (int32_t tick = 0; tick < TICK_COUNT; tick++) {
        Work works[SYSTEM_COUNT];
#ifdef _WIN32
        HANDLE threads[SYSTEM_COUNT];
#else
        pthread_t threads[SYSTEM_COUNT];
#endif
        for (int32_t index = 0; index < SYSTEM_COUNT; index++) {
            works[index].rows = ROW_TOTAL;
            works[index].result = 0;
#ifdef _WIN32
            threads[index] = CreateThread(NULL, 0, work_thread, &works[index], 0, NULL);
#else
            pthread_create(&threads[index], NULL, work_thread, &works[index]);
#endif
        }
        for (int32_t index = 0; index < SYSTEM_COUNT; index++) {
#ifdef _WIN32
            WaitForSingleObject(threads[index], INFINITE);
            CloseHandle(threads[index]);
#else
            pthread_join(threads[index], NULL);
#endif
            total += works[index].result;
        }
        across[across_count] = across[tick];
        across_count++;
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(across);
    return 0;
}
