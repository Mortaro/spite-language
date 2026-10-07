/* naive/ written in C the way it reads: the shelf is shared with the reader's thread, so it sits behind a mutex,
 * and every read of a box through it takes the mutex and counts the box it hands out with an atomic operation, as
 * a reference-counted program with threads is written in C; the box is let go once its weight is read. */
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

typedef struct Box {
    int32_t ref_count;
    int32_t weight;
} Box;

typedef struct BoxList {
    Box** items;
    int32_t count;
    int32_t capacity;
} BoxList;

typedef struct Shelf {
    Mutex mutex;
    BoxList* boxes;
} Shelf;

typedef struct Reader {
    Shelf* shelf;
    int32_t rounds;
    int64_t result;
} Reader;

static Shelf shelf;

static Box* box_make(int32_t weight) {
    Box* box = malloc(sizeof(Box));
    box->ref_count = 1;
    box->weight = weight;
    return box;
}

static void box_release(Box* box) {
    if (__atomic_sub_fetch(&box->ref_count, 1, __ATOMIC_ACQ_REL) == 0) free(box);
}

static void shelf_insert(Shelf* self, Box* box) {
    mutex_lock(&self->mutex);
    BoxList* list = self->boxes;
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(Box*) * list->capacity);
    }
    list->items[list->count] = box;
    list->count = list->count + 1;
    mutex_unlock(&self->mutex);
}

static int32_t shelf_count(Shelf* self) {
    mutex_lock(&self->mutex);
    int32_t count = self->boxes->count;
    mutex_unlock(&self->mutex);
    return count;
}

static Box* shelf_box_at(Shelf* self, int32_t place) {
    mutex_lock(&self->mutex);
    Box* box = NULL;
    if (place >= 0 && place < self->boxes->count) {
        box = self->boxes->items[place];
        __atomic_add_fetch(&box->ref_count, 1, __ATOMIC_RELAXED);
    }
    mutex_unlock(&self->mutex);
    return box;
}

static int64_t reader_read_round(Reader* self, int32_t rows, int32_t round) {
    int64_t total = 0;
    for (int32_t row = 0; row < rows; row = row + 1) {
        int32_t place = (row * 7 + round) % rows;
        Box* box = shelf_box_at(self->shelf, place);
        if (box == NULL) {
            fprintf(stderr, "no box at %d\n", place);
            exit(1);
        }
        total = total + box->weight;
        box_release(box);
    }
    return total;
}

static int64_t reader_run(Reader* self) {
    int32_t rows = shelf_count(self->shelf);
    int64_t total = 0;
    for (int32_t round = 0; round < self->rounds; round = round + 1) {
        int64_t read = reader_read_round(self, rows, round);
        total = total + read;
    }
    return total;
}

#ifdef _WIN32
static DWORD WINAPI reader_thread(LPVOID argument) {
#else
static void* reader_thread(void* argument) {
#endif
    Reader* reader = argument;
    reader->result = reader_run(reader);
    return 0;
}

static void fill_shelf(int32_t count) {
    for (int32_t index = 0; index < count; index = index + 1) {
        Box* box = box_make(index % 100);
        shelf_insert(&shelf, box);
    }
}

static int64_t read_on_the_pool(void) {
    Reader* reader = malloc(sizeof(Reader));
    reader->shelf = &shelf;
    reader->rounds = 4;
    reader->result = 0;
#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, reader_thread, reader, 0, NULL);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    pthread_t thread;
    pthread_create(&thread, NULL, reader_thread, reader);
    pthread_join(thread, NULL);
#endif
    int64_t total = reader->result;
    free(reader);
    return total;
}

int main(void) {
    mutex_make(&shelf.mutex);
    shelf.boxes = calloc(1, sizeof(BoxList));
    int64_t start = now_nanoseconds();
    fill_shelf(1000000);
    int64_t total = read_on_the_pool();
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    for (int32_t index = 0; index < shelf.boxes->count; index = index + 1) box_release(shelf.boxes->items[index]);
    free(shelf.boxes->items);
    free(shelf.boxes);
    mutex_free(&shelf.mutex);
    return 0;
}
