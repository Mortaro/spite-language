/* The same work tuned by hand: the points as two columns of plain values with no count at all, since only the
 * program's own thread ever sees them, each round's kept list a list of positions written branchlessly into one
 * buffer reused across rounds, and the side sum on its own thread as before. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#define POINT_COUNT 200000

/* each round's kept list is read once, so the C compiler cannot leave the list unwritten */
static volatile int32_t last_kept;

typedef struct Summer {
    int32_t limit;
    int64_t result;
} Summer;

#ifdef _WIN32
static DWORD WINAPI summer_thread(LPVOID argument) {
#else
static void* summer_thread(void* argument) {
#endif
    Summer* summer = argument;
    int64_t sum = 0;
    for (int32_t index = 0; index < summer->limit; index++) sum += index % 7;
    summer->result = sum;
    return 0;
}

int main(void) {
    int64_t start = now_nanoseconds();
    Summer summer = {20000000, 0};
#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, summer_thread, &summer, 0, NULL);
#else
    pthread_t thread;
    pthread_create(&thread, NULL, summer_thread, &summer);
#endif
    int32_t* across = malloc(sizeof(int32_t) * POINT_COUNT);
    int32_t* down = malloc(sizeof(int32_t) * POINT_COUNT);
    int32_t* kept = malloc(sizeof(int32_t) * POINT_COUNT);
    for (int32_t index = 0; index < POINT_COUNT; index++) {
        across[index] = index % 1000;
        down[index] = index % 700;
    }
    int64_t near_count = 0;
    for (int32_t round = 0; round < 20; round++) {
        int32_t bound = round * 80;
        int32_t kept_count = 0;
        for (int32_t index = 0; index < POINT_COUNT; index++) {
            kept[kept_count] = index;
            kept_count += across[index] + down[index] > bound;
        }
        near_count += kept_count;
        last_kept = kept[kept_count > 0 ? kept_count - 1 : 0];
    }
#ifdef _WIN32
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    pthread_join(thread, NULL);
#endif
    int64_t microseconds = microseconds_since(start);
    printf("sum %lld near %lld\n", (long long)summer.result, (long long)near_count);
    print_microseconds(microseconds);
    free(across);
    free(down);
    free(kept);
    return 0;
}
