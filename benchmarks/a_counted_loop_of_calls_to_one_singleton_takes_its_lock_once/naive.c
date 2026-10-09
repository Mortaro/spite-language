/* naive/ written in C the way it reads: one shared tally behind a mutex, four threads that each call its add in a
 * loop, and every call taking and letting go of the mutex, as a C programmer guards an object threads share. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
typedef CRITICAL_SECTION Mutex;
static void mutex_make(Mutex* mutex) { InitializeCriticalSection(mutex); }
static void mutex_lock(Mutex* mutex) { EnterCriticalSection(mutex); }
static void mutex_unlock(Mutex* mutex) { LeaveCriticalSection(mutex); }
static void mutex_free(Mutex* mutex) { DeleteCriticalSection(mutex); }
#else
#include <pthread.h>
typedef pthread_mutex_t Mutex;
static void mutex_make(Mutex* mutex) { pthread_mutex_init(mutex, NULL); }
static void mutex_lock(Mutex* mutex) { pthread_mutex_lock(mutex); }
static void mutex_unlock(Mutex* mutex) { pthread_mutex_unlock(mutex); }
static void mutex_free(Mutex* mutex) { pthread_mutex_destroy(mutex); }
#endif

typedef struct Tally {
    Mutex mutex;
    int64_t total;
    int32_t calls;
    int32_t largest;
} Tally;

typedef struct Counter {
    Tally* tally;
    int32_t rounds;
    int32_t result;
} Counter;

static Tally tally;

static void tally_add(Tally* self, int32_t amount) {
    mutex_lock(&self->mutex);
    self->total = self->total + amount;
    self->calls = self->calls + 1;
    if (amount > self->largest) self->largest = amount;
    mutex_unlock(&self->mutex);
}

static int64_t tally_sum(Tally* self) {
    mutex_lock(&self->mutex);
    int64_t total = self->total;
    mutex_unlock(&self->mutex);
    return total;
}

static int32_t tally_call_count(Tally* self) {
    mutex_lock(&self->mutex);
    int32_t calls = self->calls;
    mutex_unlock(&self->mutex);
    return calls;
}

static int32_t tally_largest_amount(Tally* self) {
    mutex_lock(&self->mutex);
    int32_t largest = self->largest;
    mutex_unlock(&self->mutex);
    return largest;
}

static Counter* counter_make(int32_t rounds) {
    Counter* counter = malloc(sizeof(Counter));
    counter->tally = &tally;
    counter->rounds = rounds;
    counter->result = 0;
    return counter;
}

static int32_t counter_count_up(Counter* self) {
    for (int32_t index = 0; index < self->rounds; index = index + 1) tally_add(self->tally, index % 3);
    return self->rounds;
}

#ifdef _WIN32
static DWORD WINAPI counter_thread(LPVOID argument) {
#else
static void* counter_thread(void* argument) {
#endif
    Counter* counter = argument;
    counter->result = counter_count_up(counter);
    return 0;
}

static int32_t count_on_four(int32_t rounds) {
    Counter* counters[4];
#ifdef _WIN32
    HANDLE threads[4];
#else
    pthread_t threads[4];
#endif
    for (int32_t index = 0; index < 4; index = index + 1) {
        counters[index] = counter_make(rounds);
#ifdef _WIN32
        threads[index] = CreateThread(NULL, 0, counter_thread, counters[index], 0, NULL);
#else
        pthread_create(&threads[index], NULL, counter_thread, counters[index]);
#endif
    }
    int32_t counted = 0;
    for (int32_t index = 0; index < 4; index = index + 1) {
#ifdef _WIN32
        WaitForSingleObject(threads[index], INFINITE);
        CloseHandle(threads[index]);
#else
        pthread_join(threads[index], NULL);
#endif
        counted = counted + counters[index]->result;
        free(counters[index]);
    }
    return counted;
}

int main(void) {
    mutex_make(&tally.mutex);
    tally.total = 0;
    tally.calls = 0;
    tally.largest = 0;
    int64_t start = now_nanoseconds();
    int32_t counted = count_on_four(5000000);
    int64_t total = tally_sum(&tally);
    int32_t calls = tally_call_count(&tally);
    int32_t largest = tally_largest_amount(&tally);
    int64_t microseconds = microseconds_since(start);
    printf("counted %d calls %d total %lld largest %d\n", counted, calls, (long long)total, largest);
    print_microseconds(microseconds);
    mutex_free(&tally.mutex);
    return 0;
}
