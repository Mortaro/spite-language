/* naive/ written in C the way it reads: the counter and the settings that the four threads share each behind a
 * mutex taken by every call, as a C programmer guards an object threads share, and the journal, which only the
 * program's thread uses, a plain list. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

typedef struct HitCounter {
    Mutex mutex;
    int32_t hits;
} HitCounter;

typedef struct Settings {
    Mutex mutex;
    int32_t step;
    const char* label;
} Settings;

typedef struct Journal {
    char** lines;
    int32_t count;
    int32_t capacity;
} Journal;

typedef struct Worker {
    HitCounter* counter;
    Settings* settings;
    int32_t rounds;
    int32_t result;
} Worker;

static HitCounter counter;
static Settings settings;
static Journal journal;

static void hit_counter_record(HitCounter* self, int32_t amount) {
    mutex_lock(&self->mutex);
    self->hits = self->hits + amount;
    mutex_unlock(&self->mutex);
}

static int32_t hit_counter_total(HitCounter* self) {
    mutex_lock(&self->mutex);
    int32_t hits = self->hits;
    mutex_unlock(&self->mutex);
    return hits;
}

static int32_t settings_step_size(Settings* self) {
    mutex_lock(&self->mutex);
    int32_t step = self->step;
    mutex_unlock(&self->mutex);
    return step;
}

static const char* settings_name(Settings* self) {
    mutex_lock(&self->mutex);
    const char* label = self->label;
    mutex_unlock(&self->mutex);
    return label;
}

static void journal_note(Journal* self, const char* line) {
    if (self->count == self->capacity) {
        self->capacity = self->capacity == 0 ? 4 : self->capacity * 2;
        self->lines = realloc(self->lines, sizeof(char*) * self->capacity);
    }
    char* copy = malloc(strlen(line) + 1);
    strcpy(copy, line);
    self->lines[self->count] = copy;
    self->count = self->count + 1;
}

static int32_t journal_written(Journal* self) {
    return self->count;
}

static int32_t worker_run(Worker* self) {
    int32_t index = 0;
    while (index < self->rounds) {
        int32_t step = settings_step_size(self->settings);
        hit_counter_record(self->counter, step);
        index = index + 1;
    }
    return index;
}

#ifdef _WIN32
static DWORD WINAPI worker_thread(LPVOID argument) {
#else
static void* worker_thread(void* argument) {
#endif
    Worker* worker = argument;
    worker->result = worker_run(worker);
    return 0;
}

static int32_t count_on_four(int32_t rounds) {
    Worker* workers[4];
#ifdef _WIN32
    HANDLE threads[4];
#else
    pthread_t threads[4];
#endif
    for (int32_t index = 0; index < 4; index = index + 1) {
        workers[index] = malloc(sizeof(Worker));
        workers[index]->counter = &counter;
        workers[index]->settings = &settings;
        workers[index]->rounds = rounds;
        workers[index]->result = 0;
#ifdef _WIN32
        threads[index] = CreateThread(NULL, 0, worker_thread, workers[index], 0, NULL);
#else
        pthread_create(&threads[index], NULL, worker_thread, workers[index]);
#endif
    }
    int32_t total = 0;
    for (int32_t index = 0; index < 4; index = index + 1) {
#ifdef _WIN32
        WaitForSingleObject(threads[index], INFINITE);
        CloseHandle(threads[index]);
#else
        pthread_join(threads[index], NULL);
#endif
        total = total + workers[index]->result;
        free(workers[index]);
    }
    return total;
}

int main(void) {
    mutex_make(&counter.mutex);
    mutex_make(&settings.mutex);
    counter.hits = 0;
    settings.step = 2;
    settings.label = "hits";
    int64_t start = now_nanoseconds();
    journal_note(&journal, "starting");
    int32_t rounds = count_on_four(1000000);
    journal_note(&journal, "finished");
    const char* name = settings_name(&settings);
    int32_t total = hit_counter_total(&counter);
    int32_t written = journal_written(&journal);
    int64_t microseconds = microseconds_since(start);
    printf("%s %d after %d rounds, journal lines: %d\n", name, total, rounds, written);
    print_microseconds(microseconds);
    for (int32_t index = 0; index < journal.count; index = index + 1) free(journal.lines[index]);
    free(journal.lines);
    mutex_free(&counter.mutex);
    mutex_free(&settings.mutex);
    return 0;
}
