/* naive/ written in C the way it reads: one shared registry behind a mutex, four threads that each record a weight
 * a quarter of a million times, every call taking and letting go of the mutex around its append and its addition. */
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

typedef struct IntegerList {
    int32_t* items;
    int32_t count;
    int32_t capacity;
} IntegerList;

typedef struct Registry {
    Mutex mutex;
    IntegerList* weights;
    int64_t total;
} Registry;

typedef struct Recorder {
    Registry* registry;
    int32_t rounds;
    int32_t result;
} Recorder;

static Registry registry;

static IntegerList* integer_list_make(void) {
    IntegerList* list = malloc(sizeof(IntegerList));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void integer_list_append(IntegerList* list, int32_t item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(int32_t) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static void registry_record(Registry* self, int32_t weight) {
    mutex_lock(&self->mutex);
    integer_list_append(self->weights, weight);
    self->total = self->total + weight;
    mutex_unlock(&self->mutex);
}

static int32_t registry_recorded(Registry* self) {
    mutex_lock(&self->mutex);
    int32_t recorded = self->weights->count;
    mutex_unlock(&self->mutex);
    return recorded;
}

static int64_t registry_sum(Registry* self) {
    mutex_lock(&self->mutex);
    int64_t total = self->total;
    mutex_unlock(&self->mutex);
    return total;
}

static int32_t recorder_weight_of(Recorder* self, int32_t index) {
    (void)self;
    return index % 10 + 1;
}

static int32_t recorder_record_all(Recorder* self) {
    for (int32_t index = 0; index < self->rounds; index = index + 1) {
        int32_t weight = recorder_weight_of(self, index);
        registry_record(self->registry, weight);
    }
    return self->rounds;
}

#ifdef _WIN32
static DWORD WINAPI recorder_thread(LPVOID argument) {
#else
static void* recorder_thread(void* argument) {
#endif
    Recorder* recorder = argument;
    recorder->result = recorder_record_all(recorder);
    return 0;
}

static int32_t record_on_four(int32_t rounds) {
    Recorder* recorders[4];
#ifdef _WIN32
    HANDLE threads[4];
#else
    pthread_t threads[4];
#endif
    for (int32_t index = 0; index < 4; index = index + 1) {
        recorders[index] = malloc(sizeof(Recorder));
        recorders[index]->registry = &registry;
        recorders[index]->rounds = rounds;
        recorders[index]->result = 0;
#ifdef _WIN32
        threads[index] = CreateThread(NULL, 0, recorder_thread, recorders[index], 0, NULL);
#else
        pthread_create(&threads[index], NULL, recorder_thread, recorders[index]);
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
        counted = counted + recorders[index]->result;
        free(recorders[index]);
    }
    return counted;
}

int main(void) {
    mutex_make(&registry.mutex);
    registry.weights = integer_list_make();
    registry.total = 0;
    int64_t start = now_nanoseconds();
    int32_t counted = record_on_four(250000);
    int32_t recorded = registry_recorded(&registry);
    int64_t total = registry_sum(&registry);
    int64_t microseconds = microseconds_since(start);
    printf("counted %d recorded %d total %lld\n", counted, recorded, (long long)total);
    print_microseconds(microseconds);
    free(registry.weights->items);
    free(registry.weights);
    mutex_free(&registry.mutex);
    return 0;
}
