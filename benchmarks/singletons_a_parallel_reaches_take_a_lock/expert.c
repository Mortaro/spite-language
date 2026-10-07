/* The same work tuned by hand: the registry's list made once at its final size, each of the four threads writing
 * its weights into a quarter of it of its own and adding them up in a register, with no lock and no atomic
 * operation, and the program's thread adding the four totals once they are done. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct Recorder {
    _Alignas(64) int32_t* weights;
    int32_t rounds;
    int64_t total;
} Recorder;

#ifdef _WIN32
static DWORD WINAPI recorder_thread(LPVOID argument) {
#else
static void* recorder_thread(void* argument) {
#endif
    Recorder* recorder = argument;
    int64_t total = 0;
    for (int32_t index = 0; index < recorder->rounds; index++) {
        int32_t weight = index % 10 + 1;
        recorder->weights[index] = weight;
        total += weight;
    }
    recorder->total = total;
    return 0;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t rounds = 250000;
    int32_t* weights = malloc(sizeof(int32_t) * (size_t)rounds * 4);
    Recorder recorders[4];
#ifdef _WIN32
    HANDLE threads[4];
#else
    pthread_t threads[4];
#endif
    for (int32_t index = 0; index < 4; index++) {
        recorders[index].weights = weights + (size_t)index * (size_t)rounds;
        recorders[index].rounds = rounds;
#ifdef _WIN32
        threads[index] = CreateThread(NULL, 0, recorder_thread, &recorders[index], 0, NULL);
#else
        pthread_create(&threads[index], NULL, recorder_thread, &recorders[index]);
#endif
    }
    int32_t counted = 0;
    int64_t total = 0;
    for (int32_t index = 0; index < 4; index++) {
#ifdef _WIN32
        WaitForSingleObject(threads[index], INFINITE);
        CloseHandle(threads[index]);
#else
        pthread_join(threads[index], NULL);
#endif
        counted += recorders[index].rounds;
        total += recorders[index].total;
    }
    int32_t recorded = rounds * 4;
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("counted %d recorded %d total %lld\n", counted, recorded, (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(weights);
    return 0;
}
