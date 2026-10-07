/* naive/ written in C the way it reads: the singleton is one global with a list of pointers to transforms and a
 * mutex, and each of its functions takes the mutex, as a C programmer makes a shared object safe. Each tick starts
 * eight threads that each read every row through at(row), so eight readers queue on one mutex for every read.
 * C keeps no counts, so a read is the pointer from its slot. */
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

#define ROW_TOTAL 15000
#define SYSTEM_COUNT 8

typedef struct Transform {
    int32_t across;
} Transform;

typedef struct Transforms {
    Transform** stored;
    int32_t count;
    int32_t capacity;
    Mutex mutex;
} Transforms;

typedef struct System {
    int32_t rows;
    int32_t result;
} System;

static Transforms transforms;

static Transform* transform_make(int32_t across) {
    Transform* transform = malloc(sizeof(Transform));
    transform->across = across;
    return transform;
}

static void transforms_insert(Transform* transform) {
    mutex_lock(&transforms.mutex);
    if (transforms.count == transforms.capacity) {
        transforms.capacity = transforms.capacity == 0 ? 4 : transforms.capacity * 2;
        transforms.stored = realloc(transforms.stored, sizeof(Transform*) * transforms.capacity);
    }
    transforms.stored[transforms.count] = transform;
    transforms.count = transforms.count + 1;
    mutex_unlock(&transforms.mutex);
}

static Transform* transforms_at(int32_t row) {
    mutex_lock(&transforms.mutex);
    if (row < 0 || row >= transforms.count) abort();
    Transform* transform = transforms.stored[row];
    mutex_unlock(&transforms.mutex);
    return transform;
}

static System* system_make(int32_t rows) {
    System* system = malloc(sizeof(System));
    system->rows = rows;
    system->result = 0;
    return system;
}

static int32_t system_run(System* system) {
    int32_t total = 0;
    for (int32_t row = 0; row < system->rows; row = row + 1) {
        Transform* transform = transforms_at(row);
        total = total + transform->across;
    }
    return total;
}

#ifdef _WIN32
static DWORD WINAPI system_thread(LPVOID argument) {
    System* system = argument;
    system->result = system_run(system);
    return 0;
}
#else
static void* system_thread(void* argument) {
    System* system = argument;
    system->result = system_run(system);
    return NULL;
}
#endif

static int64_t one_tick(void) {
    System* systems[SYSTEM_COUNT];
#ifdef _WIN32
    HANDLE threads[SYSTEM_COUNT];
#else
    pthread_t threads[SYSTEM_COUNT];
#endif
    for (int32_t index = 0; index < SYSTEM_COUNT; index = index + 1) {
        systems[index] = system_make(ROW_TOTAL);
#ifdef _WIN32
        threads[index] = CreateThread(NULL, 0, system_thread, systems[index], 0, NULL);
#else
        pthread_create(&threads[index], NULL, system_thread, systems[index]);
#endif
    }
    int64_t total = 0;
    for (int32_t index = 0; index < SYSTEM_COUNT; index = index + 1) {
#ifdef _WIN32
        WaitForSingleObject(threads[index], INFINITE);
        CloseHandle(threads[index]);
#else
        pthread_join(threads[index], NULL);
#endif
        total = total + systems[index]->result;
        free(systems[index]);
    }
    return total;
}

static int64_t ticks(int32_t count) {
    int64_t total = 0;
    for (int32_t tick = 0; tick < count; tick = tick + 1) {
        total = total + one_tick();
        Transform* written = transforms_at(tick);
        transforms_insert(written);
    }
    return total;
}

int main(void) {
    mutex_make(&transforms.mutex);
    int64_t start = now_nanoseconds();
    for (int32_t row = 0; row < ROW_TOTAL; row = row + 1) {
        Transform* transform = transform_make(row % 1000);
        transforms_insert(transform);
    }
    int64_t total = ticks(20);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    for (int32_t row = 0; row < ROW_TOTAL; row = row + 1) {
        free(transforms.stored[row]);
    }
    free(transforms.stored);
    mutex_free(&transforms.mutex);
    return 0;
}
