/* naive/ written in C the way it reads: the one piece of work that runs beside the program's thread gets a thread
 * of its own, made where the work starts and joined where its answer is read. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct Summer {
    int32_t limit;
    int32_t offset;
    int64_t result;
} Summer;

static Summer* summer_make(int32_t limit, int32_t offset) {
    Summer* summer = malloc(sizeof(Summer));
    summer->limit = limit;
    summer->offset = offset;
    summer->result = 0;
    return summer;
}

static int64_t summer_total(Summer* self) {
    int64_t sum = 0;
    for (int32_t index = self->offset; index < self->limit; index = index + 2) sum = sum + index % 7;
    return sum;
}

#ifdef _WIN32
static DWORD WINAPI summer_thread(LPVOID argument) {
#else
static void* summer_thread(void* argument) {
#endif
    Summer* summer = argument;
    summer->result = summer_total(summer);
    return 0;
}

static int64_t sum_on_two(int32_t limit) {
    Summer* evens = summer_make(limit, 0);
#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, summer_thread, evens, 0, NULL);
#else
    pthread_t thread;
    pthread_create(&thread, NULL, summer_thread, evens);
#endif
    Summer* odds = summer_make(limit, 1);
    int64_t odd_sum = summer_total(odds);
#ifdef _WIN32
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    pthread_join(thread, NULL);
#endif
    int64_t total = evens->result + odd_sum;
    free(evens);
    free(odds);
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int64_t total = sum_on_two(20000000);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
