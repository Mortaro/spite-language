/* naive/ written in C the way it reads: a reference-counted program with threads, where a function that is handed
 * a counted object counts it up with an atomic operation for its own parameter and down again when it returns, so
 * the list of rows, passed to match_into and on to find_row for each of three columns, is counted four times up
 * and four times down per entity. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct IntegerList {
    int32_t ref_count;
    int32_t* items;
    int32_t count;
    int32_t capacity;
} IntegerList;

typedef struct Matcher {
    int32_t ref_count;
    IntegerList* headers;
} Matcher;

typedef struct Stream {
    int32_t entity_count;
    int32_t result;
} Stream;

static IntegerList* integer_list_make(void) {
    IntegerList* list = malloc(sizeof(IntegerList));
    list->ref_count = 1;
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void integer_list_retain(IntegerList* list) {
    __atomic_add_fetch(&list->ref_count, 1, __ATOMIC_RELAXED);
}

static void integer_list_release(IntegerList* list) {
    if (__atomic_sub_fetch(&list->ref_count, 1, __ATOMIC_ACQ_REL) == 0) {
        free(list->items);
        free(list);
    }
}

static void integer_list_append(IntegerList* list, int32_t item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(int32_t) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static Matcher* matcher_make(int32_t column_count) {
    Matcher* matcher = malloc(sizeof(Matcher));
    matcher->ref_count = 1;
    matcher->headers = integer_list_make();
    for (int32_t index = 0; index < column_count; index = index + 1) integer_list_append(matcher->headers, index + 1);
    return matcher;
}

static void matcher_retain(Matcher* matcher) {
    __atomic_add_fetch(&matcher->ref_count, 1, __ATOMIC_RELAXED);
}

static void matcher_release(Matcher* matcher) {
    if (__atomic_sub_fetch(&matcher->ref_count, 1, __ATOMIC_ACQ_REL) == 0) {
        integer_list_release(matcher->headers);
        free(matcher);
    }
}

static int matcher_find_row(Matcher* self, int32_t index, int32_t entity, IntegerList* found) {
    integer_list_retain(found);
    if (index < 0 || index >= self->headers->count) {
        fprintf(stderr, "no header %d\n", index);
        exit(1);
    }
    int32_t place = entity % self->headers->items[index];
    found->items[index] = place;
    integer_list_release(found);
    return place >= 0;
}

static int matcher_match_into(Matcher* self, int32_t entity, IntegerList* found) {
    integer_list_retain(found);
    int32_t index = 0;
    int all = 1;
    while (all && index < self->headers->count) {
        all = matcher_find_row(self, index, entity, found);
        index = index + 1;
    }
    integer_list_release(found);
    return all;
}

static int32_t stream_run_positions(Stream* self, Matcher* matcher, IntegerList* rows) {
    matcher_retain(matcher);
    integer_list_retain(rows);
    int32_t total = 0;
    for (int32_t entity = 0; entity < self->entity_count; entity = entity + 1) {
        if (matcher_match_into(matcher, entity, rows)) total = total + rows->items[2];
    }
    integer_list_release(rows);
    matcher_release(matcher);
    return total;
}

static int32_t stream_run(Stream* self) {
    Matcher* matcher = matcher_make(3);
    IntegerList* found = integer_list_make();
    integer_list_append(found, 0);
    integer_list_append(found, 0);
    integer_list_append(found, 0);
    int32_t total = stream_run_positions(self, matcher, found);
    integer_list_release(found);
    matcher_release(matcher);
    return total;
}

#ifdef _WIN32
static DWORD WINAPI stream_thread(LPVOID argument) {
#else
static void* stream_thread(void* argument) {
#endif
    Stream* stream = argument;
    stream->result = stream_run(stream);
    return 0;
}

static int32_t stream_on_the_pool(int32_t entity_count) {
    Stream* stream = malloc(sizeof(Stream));
    stream->entity_count = entity_count;
    stream->result = 0;
#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, stream_thread, stream, 0, NULL);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    pthread_t thread;
    pthread_create(&thread, NULL, stream_thread, stream);
    pthread_join(thread, NULL);
#endif
    int32_t total = stream->result;
    free(stream);
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t total = stream_on_the_pool(3000000);
    int64_t microseconds = microseconds_since(start);
    printf("total %d\n", total);
    print_microseconds(microseconds);
    return 0;
}
