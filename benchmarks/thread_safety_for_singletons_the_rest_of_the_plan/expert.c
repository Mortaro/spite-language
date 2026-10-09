/* The same work tuned by hand, as the planned form for state that is only appended to would make it: each of the
 * four threads appends to a buffer of its own with no lock, and once they are done the program's thread merges the
 * four buffers into the log in the threads' order. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct Logger {
    _Alignas(64) int32_t* buffer;
    int32_t count;
    int32_t capacity;
    int32_t rounds;
    int32_t source;
} Logger;

#ifdef _WIN32
static DWORD WINAPI logger_thread(LPVOID argument) {
#else
static void* logger_thread(void* argument) {
#endif
    Logger* logger = argument;
    for (int32_t index = 0; index < logger->rounds; index++) {
        if (logger->count == logger->capacity) {
            logger->capacity = logger->capacity == 0 ? 1024 : logger->capacity * 2;
            logger->buffer = realloc(logger->buffer, sizeof(int32_t) * (size_t)logger->capacity);
        }
        logger->buffer[logger->count++] = logger->source * 1000000 + index;
    }
    return 0;
}

int main(void) {
    int64_t start = now_nanoseconds();
    Logger loggers[4];
#ifdef _WIN32
    HANDLE threads[4];
#else
    pthread_t threads[4];
#endif
    for (int32_t index = 0; index < 4; index++) {
        loggers[index] = (Logger){NULL, 0, 0, 250000, index + 1};
#ifdef _WIN32
        threads[index] = CreateThread(NULL, 0, logger_thread, &loggers[index], 0, NULL);
#else
        pthread_create(&threads[index], NULL, logger_thread, &loggers[index]);
#endif
    }
    int32_t logged = 0;
    for (int32_t index = 0; index < 4; index++) {
#ifdef _WIN32
        WaitForSingleObject(threads[index], INFINITE);
        CloseHandle(threads[index]);
#else
        pthread_join(threads[index], NULL);
#endif
        logged += loggers[index].rounds;
    }
    int32_t* entries = malloc(sizeof(int32_t) * (size_t)logged);
    int32_t count = 0;
    for (int32_t index = 0; index < 4; index++) {
        memcpy(entries + count, loggers[index].buffer, sizeof(int32_t) * (size_t)loggers[index].count);
        count += loggers[index].count;
        free(loggers[index].buffer);
    }
    __asm__ volatile("" : : "r"(entries) : "memory");   /* the log is merged, as the program asks, not only counted */
    int64_t microseconds = microseconds_since(start);
    printf("logged %d entries %d\n", logged, count);
    print_microseconds(microseconds);
    free(entries);
    return 0;
}
