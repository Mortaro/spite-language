/* The same work tuned by hand: the one call on the thread is made while the program's thread waits for it, and
 * once it is joined only the program's thread touches the tally, so the ten million calls after it take no lock
 * and make no atomic operation: two additions in registers each, stored once. */
#include <stdint.h>
#include <stdio.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct Tally {
    int64_t total;
    int32_t calls;
} Tally;

static Tally tally;

#ifdef _WIN32
static DWORD WINAPI worker_thread(LPVOID argument) {
#else
static void* worker_thread(void* argument) {
#endif
    int32_t* result = argument;
    tally.total += 1;
    tally.calls += 1;
    *result = 2;
    return 0;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t first = 0;
#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, worker_thread, &first, 0, NULL);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    pthread_t thread;
    pthread_create(&thread, NULL, worker_thread, &first);
    pthread_join(thread, NULL);
#endif
    int64_t total = tally.total;
    int32_t calls = tally.calls;
    int32_t index = 0;
    while (index < 10000000) {
        total += index;
        calls += 1;
        __asm__ volatile("" : "+r"(total));   /* each call's addition is made, as the program asks, not one formula */
        index = index + 1;
    }
    tally.total = total;
    tally.calls = calls;
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("first %d reached %d calls %d total %lld\n", first, index, tally.calls, (long long)tally.total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
