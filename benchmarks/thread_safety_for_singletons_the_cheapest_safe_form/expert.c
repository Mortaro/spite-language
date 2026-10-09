/* The same work tuned by hand: the settings set before the threads start and read plainly, each of the four threads
 * adding its hits up in a register and storing them once on a cache line of its own, and the program's thread adding
 * the four counts into the counter once they are done; the journal a plain list, which only that thread uses. */
#include <stdint.h>
#include <stdio.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct Worker {
    _Alignas(64) int32_t rounds;
    int32_t hits;
} Worker;

static int32_t step = 2;
static const char* label = "hits";

#ifdef _WIN32
static DWORD WINAPI worker_thread(LPVOID argument) {
#else
static void* worker_thread(void* argument) {
#endif
    Worker* worker = argument;
    int32_t hits = 0;
    for (int32_t index = 0; index < worker->rounds; index++) {
        hits += step;
        __asm__ volatile("" : "+r"(hits));   /* each call's addition is made, as the program asks, not one product */
    }
    worker->hits = hits;
    return 0;
}

int main(void) {
    int64_t start = now_nanoseconds();
    const char* journal[2];
    int32_t written = 0;
    journal[written++] = "starting";
    Worker workers[4];
#ifdef _WIN32
    HANDLE threads[4];
#else
    pthread_t threads[4];
#endif
    for (int32_t index = 0; index < 4; index++) {
        workers[index].rounds = 1000000;
#ifdef _WIN32
        threads[index] = CreateThread(NULL, 0, worker_thread, &workers[index], 0, NULL);
#else
        pthread_create(&threads[index], NULL, worker_thread, &workers[index]);
#endif
    }
    int32_t rounds = 0;
    int32_t total = 0;
    for (int32_t index = 0; index < 4; index++) {
#ifdef _WIN32
        WaitForSingleObject(threads[index], INFINITE);
        CloseHandle(threads[index]);
#else
        pthread_join(threads[index], NULL);
#endif
        rounds += workers[index].rounds;
        total += workers[index].hits;
    }
    journal[written++] = "finished";
    __asm__ volatile("" : : "r"(journal) : "memory");   /* the journal is written, as the program asks, not only counted */
    int64_t microseconds = microseconds_since(start);
    printf("%s %d after %d rounds, journal lines: %d\n", label, total, rounds, written);
    print_microseconds(microseconds);
    return 0;
}
