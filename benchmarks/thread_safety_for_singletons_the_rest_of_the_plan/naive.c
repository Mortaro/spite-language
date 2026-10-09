/* naive/ written in C the way it reads: one event log behind a mutex, four threads that each record a quarter of a
 * million entries, every call taking and letting go of the mutex around its append. */
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

typedef struct EventLog {
    Mutex mutex;
    IntegerList* entries;
} EventLog;

typedef struct Logger {
    EventLog* log;
    int32_t rounds;
    int32_t source;
    int32_t result;
} Logger;

static EventLog event_log;

static void integer_list_append(IntegerList* list, int32_t item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(int32_t) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static void event_log_record(EventLog* self, int32_t entry) {
    mutex_lock(&self->mutex);
    integer_list_append(self->entries, entry);
    mutex_unlock(&self->mutex);
}

static int32_t event_log_count(EventLog* self) {
    mutex_lock(&self->mutex);
    int32_t count = self->entries->count;
    mutex_unlock(&self->mutex);
    return count;
}

static int32_t logger_entry_for(Logger* self, int32_t index) {
    return self->source * 1000000 + index;
}

static int32_t logger_log_all(Logger* self) {
    for (int32_t index = 0; index < self->rounds; index = index + 1) {
        int32_t entry = logger_entry_for(self, index);
        event_log_record(self->log, entry);
    }
    return self->rounds;
}

#ifdef _WIN32
static DWORD WINAPI logger_thread(LPVOID argument) {
#else
static void* logger_thread(void* argument) {
#endif
    Logger* logger = argument;
    logger->result = logger_log_all(logger);
    return 0;
}

static int32_t log_on_four(int32_t rounds) {
    Logger* loggers[4];
#ifdef _WIN32
    HANDLE threads[4];
#else
    pthread_t threads[4];
#endif
    for (int32_t index = 0; index < 4; index = index + 1) {
        loggers[index] = malloc(sizeof(Logger));
        loggers[index]->log = &event_log;
        loggers[index]->rounds = rounds;
        loggers[index]->source = index + 1;
        loggers[index]->result = 0;
#ifdef _WIN32
        threads[index] = CreateThread(NULL, 0, logger_thread, loggers[index], 0, NULL);
#else
        pthread_create(&threads[index], NULL, logger_thread, loggers[index]);
#endif
    }
    int32_t logged = 0;
    for (int32_t index = 0; index < 4; index = index + 1) {
#ifdef _WIN32
        WaitForSingleObject(threads[index], INFINITE);
        CloseHandle(threads[index]);
#else
        pthread_join(threads[index], NULL);
#endif
        logged = logged + loggers[index]->result;
        free(loggers[index]);
    }
    return logged;
}

int main(void) {
    mutex_make(&event_log.mutex);
    event_log.entries = calloc(1, sizeof(IntegerList));
    int64_t start = now_nanoseconds();
    int32_t logged = log_on_four(250000);
    int32_t count = event_log_count(&event_log);
    int64_t microseconds = microseconds_since(start);
    printf("logged %d entries %d\n", logged, count);
    print_microseconds(microseconds);
    free(event_log.entries->items);
    free(event_log.entries);
    mutex_free(&event_log.mutex);
    return 0;
}
