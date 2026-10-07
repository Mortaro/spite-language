/* naive/ written in C the way it reads: a lock made on the heap from the system's thread library, opened by name
 * on first use with each function looked up by its name, and a thousand rounds of taking it, counting and letting
 * it go. The names are text written into the program, passed as they are, which is what the Spite program does
 * with them too. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>

typedef void (WINAPI *LockFunction)(PSRWLOCK);

typedef struct Lock {
    SRWLOCK* handle;
} Lock;

static HMODULE library = 0;
static LockFunction initialize_lock = 0;
static LockFunction acquire_lock = 0;
static LockFunction release_lock = 0;

static void open_library(void) {
    if (library != 0) return;
    library = LoadLibraryA("kernel32.dll");
    initialize_lock = (LockFunction)(void*)GetProcAddress(library, "InitializeSRWLock");
    acquire_lock = (LockFunction)(void*)GetProcAddress(library, "AcquireSRWLockExclusive");
    release_lock = (LockFunction)(void*)GetProcAddress(library, "ReleaseSRWLockExclusive");
}

static Lock* lock_make(void) {
    open_library();
    Lock* lock = malloc(sizeof(Lock));
    lock->handle = malloc(sizeof(SRWLOCK));
    initialize_lock(lock->handle);
    return lock;
}

static void lock_lock(Lock* lock) {
    acquire_lock(lock->handle);
}

static void lock_unlock(Lock* lock) {
    release_lock(lock->handle);
}

static void lock_free(Lock* lock) {
    free(lock->handle);
    free(lock);
    FreeLibrary(library);
}
#else
#include <pthread.h>

typedef struct Lock {
    pthread_mutex_t* handle;
} Lock;

static Lock* lock_make(void) {
    Lock* lock = malloc(sizeof(Lock));
    lock->handle = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(lock->handle, NULL);
    return lock;
}

static void lock_lock(Lock* lock) {
    pthread_mutex_lock(lock->handle);
}

static void lock_unlock(Lock* lock) {
    pthread_mutex_unlock(lock->handle);
}

static void lock_free(Lock* lock) {
    pthread_mutex_destroy(lock->handle);
    free(lock->handle);
    free(lock);
}
#endif

static Lock* guard = 0;
static int32_t count = 0;

static void count_once(void) {
    lock_lock(guard);
    count = count + 1;
    lock_unlock(guard);
}

int main(void) {
    guard = lock_make();
    for (int32_t round = 0; round < 1000; round = round + 1) {
        count_once();
    }
    printf("counted %d\n", count);
    lock_free(guard);
    return 0;
}
