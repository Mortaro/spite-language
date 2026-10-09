/* naive/ written in C the way it reads: the tally is shared with a thread, so it sits behind a mutex that every
 * call takes, also the ten million calls the program's thread makes after that thread has been joined. */
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
} Tally;

typedef struct Worker {
    Tally* tally;
    int32_t result;
} Worker;

static Tally tally;

static int32_t tally_add(Tally* self, int32_t amount) {
    mutex_lock(&self->mutex);
    self->total = self->total + amount;
    self->calls = self->calls + 1;
    mutex_unlock(&self->mutex);
    return amount + 1;
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

#ifdef _WIN32
static DWORD WINAPI worker_thread(LPVOID argument) {
#else
static void* worker_thread(void* argument) {
#endif
    Worker* worker = argument;
    worker->result = tally_add(worker->tally, 1);
    return 0;
}

static int32_t add_on_the_pool(void) {
    Worker* worker = malloc(sizeof(Worker));
    worker->tally = &tally;
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

static int32_t add_many(int32_t count) {
    int32_t index = 0;
    while (index < count) index = tally_add(&tally, index);
    return index;
}

int main(void) {
    mutex_make(&tally.mutex);
    tally.total = 0;
    tally.calls = 0;
    int64_t start = now_nanoseconds();
    int32_t first = add_on_the_pool();
    int32_t reached = add_many(10000000);
    int64_t total = tally_sum(&tally);
    int32_t calls = tally_call_count(&tally);
    int64_t microseconds = microseconds_since(start);
    printf("first %d reached %d calls %d total %lld\n", first, reached, calls, (long long)total);
    print_microseconds(microseconds);
    mutex_free(&tally.mutex);
    return 0;
}
