/* The same work tuned by hand: only the program's thread touches the ledger, so its total, its count and its
 * milestones live in locals for the whole loop, a countdown replaces the remainder, and the struct is written once
 * at the end. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct Ledger {
    int64_t total;
    int32_t notes;
    int32_t milestones[16];
    int32_t milestone_count;
} Ledger;

static Ledger ledger;

#ifdef _WIN32
static DWORD WINAPI worker_thread(LPVOID argument) {
#else
static void* worker_thread(void* argument) {
#endif
    int32_t* result = argument;
    int32_t sum = 0;
    for (int32_t index = 0; index < 1000; index = index + 1) sum = sum + index % 7;
    *result = sum;
    return 0;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t counted = 0;
#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, worker_thread, &counted, 0, NULL);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    pthread_t thread;
    pthread_create(&thread, NULL, worker_thread, &counted);
    pthread_join(thread, NULL);
#endif
    int32_t amounts[4] = { 4, 12, 30, 7 };
    int32_t large = 0;
    for (int32_t index = 0; index < 4; index = index + 1) large = large + (amounts[index] > 10);
    int64_t total = 0;
    int32_t milestones = 0;
    int32_t seen = 0;
    int32_t until_milestone = 0;
    for (int32_t index = 0; index < 10000000; index = index + 1) {
        total = total + index;
        if (until_milestone == 0) {
            if (milestones < 16) ledger.milestones[milestones] = index;
            milestones = milestones + 1;
            until_milestone = 1000000;
        }
        until_milestone = until_milestone - 1;
        seen = seen + milestones;
        __asm__ volatile("" : "+r"(total));   /* each note's addition is made, as the program asks, not one formula */
    }
    ledger.total = total;
    ledger.notes = 10000000;
    ledger.milestone_count = milestones;
    int64_t microseconds = microseconds_since(start);
    printf("counted %d large %d seen %d notes %d milestones %d total %lld\n", counted, large, seen, ledger.notes,
           ledger.milestone_count, (long long)ledger.total);
    print_microseconds(microseconds);
    return 0;
}
