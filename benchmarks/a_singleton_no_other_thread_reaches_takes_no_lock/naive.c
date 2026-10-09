/* naive/ written in C the way it reads: the worker runs on a thread of its own and touches nothing of the ledger,
 * so a C programmer gives the ledger no mutex; it is one global struct with a growable array, and every call and
 * every read of its milestones goes through a function. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct Milestones {
    int32_t* items;
    int32_t count;
    int32_t capacity;
} Milestones;

typedef struct Ledger {
    int64_t total;
    int32_t notes;
    Milestones* milestones;
} Ledger;

typedef struct Worker {
    int32_t result;
} Worker;

static Ledger ledger;

static void milestones_append(Milestones* self, int32_t value) {
    if (self->count == self->capacity) {
        self->capacity = self->capacity == 0 ? 4 : self->capacity * 2;
        self->items = realloc(self->items, (size_t)self->capacity * sizeof(int32_t));
    }
    self->items[self->count] = value;
    self->count = self->count + 1;
}

static int32_t ledger_note(Ledger* self, int32_t amount) {
    self->total = self->total + amount;
    self->notes = self->notes + 1;
    if (amount % 1000000 == 0) milestones_append(self->milestones, amount);
    return amount + 1;
}

static int ledger_is_large(int32_t amount) {
    return amount > 10;
}

#ifdef _WIN32
static DWORD WINAPI worker_thread(LPVOID argument) {
#else
static void* worker_thread(void* argument) {
#endif
    Worker* worker = argument;
    int32_t sum = 0;
    for (int32_t index = 0; index < 1000; index = index + 1) sum = sum + index % 7;
    worker->result = sum;
    return 0;
}

static int32_t count_on_the_pool(void) {
    Worker* worker = malloc(sizeof(Worker));
    worker->result = 0;
#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, worker_thread, worker, 0, NULL);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    pthread_t thread;
    pthread_create(&thread, NULL, worker_thread, worker);
    pthread_join(thread, NULL);
#endif
    int32_t result = worker->result;
    free(worker);
    return result;
}

static int32_t note_many(int32_t count) {
    int32_t seen = 0;
    int32_t index = 0;
    while (index < count) {
        index = ledger_note(&ledger, index);
        seen = seen + ledger.milestones->count;
    }
    return seen;
}

int main(void) {
    ledger.total = 0;
    ledger.notes = 0;
    ledger.milestones = calloc(1, sizeof(Milestones));
    int64_t start = now_nanoseconds();
    int32_t counted = count_on_the_pool();
    int32_t amounts[4] = { 4, 12, 30, 7 };
    int32_t large = 0;
    for (int32_t index = 0; index < 4; index = index + 1) large = large + ledger_is_large(amounts[index]);
    int32_t seen = note_many(10000000);
    int64_t microseconds = microseconds_since(start);
    printf("counted %d large %d seen %d notes %d milestones %d total %lld\n", counted, large, seen, ledger.notes,
           ledger.milestones->count, (long long)ledger.total);
    print_microseconds(microseconds);
    free(ledger.milestones->items);
    free(ledger.milestones);
    return 0;
}
