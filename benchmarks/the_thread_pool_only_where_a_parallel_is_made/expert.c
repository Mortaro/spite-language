/* The same work tuned by hand: one thread for the even half, started first, the odd half on the program's thread
 * meanwhile, each half summed into a register and stored once on a cache line of its own. */
#include <stdint.h>
#include <stdio.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct Half {
    _Alignas(64) int32_t limit;
    int32_t offset;
    int64_t result;
} Half;

static int64_t sum_half(const Half* half) {
    int64_t sum = 0;
    for (int32_t index = half->offset; index < half->limit; index += 2) sum += index % 7;
    return sum;
}

#ifdef _WIN32
static DWORD WINAPI half_thread(LPVOID argument) {
#else
static void* half_thread(void* argument) {
#endif
    Half* half = argument;
    half->result = sum_half(half);
    return 0;
}

int main(void) {
    int64_t start = now_nanoseconds();
    Half evens = {20000000, 0, 0};
    Half odds = {20000000, 1, 0};
#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, half_thread, &evens, 0, NULL);
#else
    pthread_t thread;
    pthread_create(&thread, NULL, half_thread, &evens);
#endif
    odds.result = sum_half(&odds);
#ifdef _WIN32
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    pthread_join(thread, NULL);
#endif
    int64_t total = evens.result + odds.result;
    int64_t microseconds = microseconds_since(start);
    printf("total %lld\n", (long long)total);
    print_microseconds(microseconds);
    return 0;
}
