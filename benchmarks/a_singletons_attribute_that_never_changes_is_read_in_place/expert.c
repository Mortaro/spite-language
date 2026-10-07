/* The same work tuned by hand: the boxes' weights as one column of numbers filled before the reader starts, which
 * the reader's thread then reads with no lock and no count, since nothing writes the column while it reads. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#define BOX_COUNT 1000000

typedef struct Reader {
    const int32_t* weights;
    int32_t rows;
    int32_t rounds;
    int64_t result;
} Reader;

#ifdef _WIN32
static DWORD WINAPI reader_thread(LPVOID argument) {
#else
static void* reader_thread(void* argument) {
#endif
    Reader* reader = argument;
    int64_t total = 0;
    for (int32_t round = 0; round < reader->rounds; round++) {
        int32_t place = round % reader->rows;
        for (int32_t row = 0; row < reader->rows; row++) {
            total += reader->weights[place];
            place += 7;
            if (place >= reader->rows) place -= reader->rows;
        }
    }
    reader->result = total;
    return 0;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t* weights = malloc(sizeof(int32_t) * BOX_COUNT);
    for (int32_t index = 0; index < BOX_COUNT; index++) weights[index] = index % 100;
    Reader reader = {weights, BOX_COUNT, 4, 0};
#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, reader_thread, &reader, 0, NULL);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    pthread_t thread;
    pthread_create(&thread, NULL, reader_thread, &reader);
    pthread_join(thread, NULL);
#endif
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)reader.result);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(weights);
    return 0;
}
