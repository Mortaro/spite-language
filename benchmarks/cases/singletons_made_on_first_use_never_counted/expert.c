/* The same work tuned by hand: the one Rules is a static object set before the threads start, so fetching it is
 * reading it, with no lock and no count, and each visit is two numbers in registers. */
#include <stdint.h>
#include <stdio.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct Rules {
    int32_t base;
} Rules;

typedef struct Fetcher {
    _Alignas(64) int32_t rounds;
    int64_t result;
} Fetcher;

static Rules rules;

#ifdef _WIN32
static DWORD WINAPI fetcher_thread(LPVOID argument) {
#else
static void* fetcher_thread(void* argument) {
#endif
    Fetcher* fetcher = argument;
    int64_t found = 0;
    for (int32_t index = 0; index < fetcher->rounds; index++) {
        found += index % 7 + rules.base;
    }
    fetcher->result = found;
    return 0;
}

int main(void) {
    int64_t start = now_nanoseconds();
    rules.base = 3;
    Fetcher fetchers[2];
#ifdef _WIN32
    HANDLE threads[2];
#else
    pthread_t threads[2];
#endif
    for (int32_t index = 0; index < 2; index++) {
        fetchers[index].rounds = 10000000;
#ifdef _WIN32
        threads[index] = CreateThread(NULL, 0, fetcher_thread, &fetchers[index], 0, NULL);
#else
        pthread_create(&threads[index], NULL, fetcher_thread, &fetchers[index]);
#endif
    }
    int64_t fetched = 0;
    for (int32_t index = 0; index < 2; index++) {
#ifdef _WIN32
        WaitForSingleObject(threads[index], INFINITE);
        CloseHandle(threads[index]);
#else
        pthread_join(threads[index], NULL);
#endif
        fetched += fetchers[index].result;
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("fetched %lld\n", (long long)fetched);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
