/* The same work tuned by hand: the even count on a second thread while the odd count runs on the program's own, each
 * summing into a local of its own and storing it once, on its own cache line, and the print waiting for both. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct Total {
    _Alignas(64) int64_t value;
} Total;

static Total evens;
static Total odds;

#ifdef _WIN32
static DWORD WINAPI evens_thread(LPVOID argument) {
#else
static void* evens_thread(void* argument) {
#endif
    (void)argument;
    int64_t total = 0;
    for (int32_t index = 0; index < 40000000; index++) total += index % 7 * 2;
    evens.value = total;
    return 0;
}

static void odds_count(void) {
    int64_t total = 0;
    for (int32_t index = 0; index < 40000000; index++) total += index % 5 * 2 + 1;
    odds.value = total;
}

int main(void) {
    int64_t start = now_nanoseconds();
#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, evens_thread, NULL, 0, NULL);
    odds_count();
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    pthread_t thread;
    pthread_create(&thread, NULL, evens_thread, NULL);
    odds_count();
    pthread_join(thread, NULL);
#endif
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("%lld %lld\n", (long long)evens.value, (long long)odds.value);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
