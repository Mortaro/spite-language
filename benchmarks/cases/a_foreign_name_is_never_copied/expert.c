/* The same work tuned by hand: the lock is a static variable and the system's lock functions are called directly,
 * linked when the program is, so nothing is opened or looked up by name while it runs. */
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
static SRWLOCK guard = SRWLOCK_INIT;
#define LOCK() AcquireSRWLockExclusive(&guard)
#define UNLOCK() ReleaseSRWLockExclusive(&guard)
#else
#include <pthread.h>
static pthread_mutex_t guard = PTHREAD_MUTEX_INITIALIZER;
#define LOCK() pthread_mutex_lock(&guard)
#define UNLOCK() pthread_mutex_unlock(&guard)
#endif

int main(void) {
    int32_t count = 0;
    for (int32_t round = 0; round < 1000; round++) {
        LOCK();
        count++;
        UNLOCK();
    }
    printf("counted %d\n", count);
    return 0;
}
