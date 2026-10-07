/* The same work tuned by hand: the headers and the rows as arrays the thread keeps on its stack, the match written
 * inline in the entity loop, and nothing counted, since the thread holds both for the whole run. */
#include <stdint.h>
#include <stdio.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct Stream {
    int32_t entity_count;
    int32_t result;
} Stream;

#ifdef _WIN32
static DWORD WINAPI stream_thread(LPVOID argument) {
#else
static void* stream_thread(void* argument) {
#endif
    Stream* stream = argument;
    const int32_t headers[3] = {1, 2, 3};
    int32_t rows[3] = {0, 0, 0};
    int32_t total = 0;
    for (int32_t entity = 0; entity < stream->entity_count; entity++) {
        int all = 1;
        for (int32_t index = 0; all && index < 3; index++) {
            int32_t place = entity % headers[index];
            rows[index] = place;
            all = place >= 0;
        }
        if (all) total += rows[2];
    }
    stream->result = total;
    return 0;
}

int main(void) {
    int64_t start = now_nanoseconds();
    Stream stream = {3000000, 0};
#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, stream_thread, &stream, 0, NULL);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    pthread_t thread;
    pthread_create(&thread, NULL, stream_thread, &stream);
    pthread_join(thread, NULL);
#endif
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %d\n", stream.result);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
