/* naive/ written in C the way it reads: eight nappers that each sleep, double their number and sleep again, run at
 * once the way C runs work at once, a thread each, whose stack holds the napper's locals while it sleeps, and the
 * program joins the threads in order and adds up their answers. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
static void sleep_milliseconds(int32_t milliseconds) { Sleep((DWORD)milliseconds); }
#else
#include <pthread.h>
#include <time.h>
static void sleep_milliseconds(int32_t milliseconds) {
    struct timespec wait = {milliseconds / 1000, (long)(milliseconds % 1000) * 1000000L};
    nanosleep(&wait, NULL);
}
#endif

typedef struct Napper {
    int32_t number;
    int32_t result;
} Napper;

static Napper* napper_make(int32_t number) {
    Napper* napper = malloc(sizeof(Napper));
    napper->number = number;
    napper->result = 0;
    return napper;
}

static int32_t napper_nap(Napper* self) {
    sleep_milliseconds(2);
    int32_t doubled = self->number * 2;
    sleep_milliseconds(2);
    return doubled + 1;
}

#ifdef _WIN32
static DWORD WINAPI napper_thread(LPVOID argument) {
#else
static void* napper_thread(void* argument) {
#endif
    Napper* napper = argument;
    napper->result = napper_nap(napper);
    return 0;
}

static int32_t nap_all(int32_t count) {
    Napper** nappers = malloc(sizeof(Napper*) * (size_t)count);
#ifdef _WIN32
    HANDLE* threads = malloc(sizeof(HANDLE) * (size_t)count);
#else
    pthread_t* threads = malloc(sizeof(pthread_t) * (size_t)count);
#endif
    for (int32_t index = 0; index < count; index = index + 1) {
        nappers[index] = napper_make(index);
#ifdef _WIN32
        threads[index] = CreateThread(NULL, 0, napper_thread, nappers[index], 0, NULL);
#else
        pthread_create(&threads[index], NULL, napper_thread, nappers[index]);
#endif
    }
    int32_t total = 0;
    for (int32_t read = 0; read < count; read = read + 1) {
#ifdef _WIN32
        WaitForSingleObject(threads[read], INFINITE);
        CloseHandle(threads[read]);
#else
        pthread_join(threads[read], NULL);
#endif
        total = total + nappers[read]->result;
        free(nappers[read]);
    }
    free(nappers);
    free(threads);
    return total;
}

int main(void) {
    int32_t total = nap_all(8);
    printf("total %d\n", total);
    return 0;
}
