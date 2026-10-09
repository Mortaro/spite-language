/* naive/ written in C the way it reads: the one Rules made by the first thread that asks for it, behind a lock, and
 * every Visit counting the Rules it holds up when it is made and down when it is freed, with atomic operations,
 * since two threads share it. */
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

typedef struct Rules {
    int32_t count;
    int32_t base;
} Rules;

typedef struct Visit {
    Rules* rules;
    int32_t number;
} Visit;

typedef struct Fetcher {
    int32_t rounds;
    int64_t result;
} Fetcher;

static Mutex rules_mutex;
static Rules* rules_made = NULL;

static Rules* rules_get(void) {
    mutex_lock(&rules_mutex);
    if (rules_made == NULL) {
        rules_made = malloc(sizeof(Rules));
        rules_made->count = 1;
        rules_made->base = 3;
    }
    Rules* rules = rules_made;
    __atomic_add_fetch(&rules->count, 1, __ATOMIC_SEQ_CST);
    mutex_unlock(&rules_mutex);
    return rules;
}

static void rules_release(Rules* rules) {
    if (__atomic_sub_fetch(&rules->count, 1, __ATOMIC_SEQ_CST) == 0) free(rules);
}

static Visit* visit_make(int32_t number) {
    Visit* visit = malloc(sizeof(Visit));
    visit->rules = rules_get();
    visit->number = number;
    return visit;
}

static void visit_free(Visit* visit) {
    rules_release(visit->rules);
    free(visit);
}

static int32_t visit_weight(Visit* visit) {
    return visit->number % 7 + visit->rules->base;
}

static int64_t fetcher_fetch_many(Fetcher* self) {
    int64_t found = 0;
    for (int32_t index = 0; index < self->rounds; index = index + 1) {
        Visit* visit = visit_make(index);
        found = found + visit_weight(visit);
        visit_free(visit);
    }
    return found;
}

#ifdef _WIN32
static DWORD WINAPI fetcher_thread(LPVOID argument) {
#else
static void* fetcher_thread(void* argument) {
#endif
    Fetcher* fetcher = argument;
    fetcher->result = fetcher_fetch_many(fetcher);
    return 0;
}

static int64_t fetch_on_two(int32_t rounds) {
    Fetcher* fetchers[2];
#ifdef _WIN32
    HANDLE threads[2];
#else
    pthread_t threads[2];
#endif
    for (int32_t index = 0; index < 2; index = index + 1) {
        fetchers[index] = malloc(sizeof(Fetcher));
        fetchers[index]->rounds = rounds;
        fetchers[index]->result = 0;
#ifdef _WIN32
        threads[index] = CreateThread(NULL, 0, fetcher_thread, fetchers[index], 0, NULL);
#else
        pthread_create(&threads[index], NULL, fetcher_thread, fetchers[index]);
#endif
    }
    int64_t fetched = 0;
    for (int32_t index = 0; index < 2; index = index + 1) {
#ifdef _WIN32
        WaitForSingleObject(threads[index], INFINITE);
        CloseHandle(threads[index]);
#else
        pthread_join(threads[index], NULL);
#endif
        fetched = fetched + fetchers[index]->result;
        free(fetchers[index]);
    }
    return fetched;
}

int main(void) {
    mutex_make(&rules_mutex);
    int64_t start = now_nanoseconds();
    int64_t fetched = fetch_on_two(10000000);
    int64_t microseconds = microseconds_since(start);
    printf("fetched %lld\n", (long long)fetched);
    print_microseconds(microseconds);
    if (rules_made != NULL) rules_release(rules_made);
    mutex_free(&rules_mutex);
    return 0;
}
