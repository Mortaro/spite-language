/* The same work tuned by hand: each of the four threads makes every call's addition, count and comparison into its
 * own tally on its own cache line, with no lock and no atomic operation, and the program's thread adds the four
 * tallies into the shared one once they are done. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct Counter {
    _Alignas(64) int32_t rounds;
    int32_t result;
    int64_t total;
    int32_t calls;
    int32_t largest;
} Counter;

#ifdef _WIN32
static DWORD WINAPI counter_thread(LPVOID argument) {
#else
static void* counter_thread(void* argument) {
#endif
    Counter* counter = argument;
    int64_t total = 0;
    int32_t calls = 0;
    int32_t largest = 0;
    for (int32_t index = 0; index < counter->rounds; index++) {
        int32_t amount = index % 3;
        total += amount;
        calls++;
        largest = amount > largest ? amount : largest;
    }
    counter->total = total;
    counter->calls = calls;
    counter->largest = largest;
    counter->result = counter->rounds;
    return 0;
}

int main(void) {
    int64_t start = now_nanoseconds();
    Counter counters[4];
#ifdef _WIN32
    HANDLE threads[4];
#else
    pthread_t threads[4];
#endif
    for (int32_t index = 0; index < 4; index++) {
        counters[index].rounds = 5000000;
#ifdef _WIN32
        threads[index] = CreateThread(NULL, 0, counter_thread, &counters[index], 0, NULL);
#else
        pthread_create(&threads[index], NULL, counter_thread, &counters[index]);
#endif
    }
    int32_t counted = 0;
    int64_t total = 0;
    int32_t calls = 0;
    int32_t largest = 0;
    for (int32_t index = 0; index < 4; index++) {
#ifdef _WIN32
        WaitForSingleObject(threads[index], INFINITE);
        CloseHandle(threads[index]);
#else
        pthread_join(threads[index], NULL);
#endif
        counted += counters[index].result;
        total += counters[index].total;
        calls += counters[index].calls;
        largest = counters[index].largest > largest ? counters[index].largest : largest;
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("counted %d calls %d total %lld largest %d\n", counted, calls, (long long)total, largest);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
