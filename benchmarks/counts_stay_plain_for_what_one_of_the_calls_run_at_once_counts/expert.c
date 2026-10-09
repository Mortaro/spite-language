/* The same work tuned by hand: the archive sorted on a thread of its own while the gallery sorts on the program's
 * own thread, the values in plain arrays, each round's kept objects counted without a list (the program only ever
 * reads how many there were), and the print waiting for both. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct Collection {
    _Alignas(64) int32_t values[2000];
    int32_t modulus;
    int32_t total;
} Collection;

static Collection collections[2];

static void sort(Collection* collection) {
    int32_t total = 0;
    int32_t modulus = collection->modulus;
    for (int32_t round = 0; round < 2000; round++) {
        int32_t wanted = round % modulus;
        int32_t count = 0;
        for (int32_t index = 0; index < 2000; index++) count += collection->values[index] % modulus == wanted;
        total += count;
    }
    collection->total = total;
}

#ifdef _WIN32
static DWORD WINAPI archive_thread(LPVOID argument) {
    (void)argument;
    sort(&collections[0]);
    return 0;
}
#else
static void* archive_thread(void* argument) {
    (void)argument;
    sort(&collections[0]);
    return 0;
}
#endif

int main(void) {
    for (int32_t index = 0; index < 2000; index++) {
        collections[0].values[index] = index * 7 % 1000;
        collections[1].values[index] = index * 11 % 1000;
    }
    collections[0].modulus = 3;
    collections[1].modulus = 5;
    int64_t start = now_nanoseconds();
#ifdef _WIN32
    HANDLE archive = CreateThread(NULL, 0, archive_thread, NULL, 0, NULL);
    sort(&collections[1]);
    WaitForSingleObject(archive, INFINITE);
    CloseHandle(archive);
#else
    pthread_t archive;
    pthread_create(&archive, NULL, archive_thread, NULL);
    sort(&collections[1]);
    pthread_join(archive, NULL);
#endif
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("%d %d\n", collections[0].total, collections[1].total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
