/* The same work tuned by hand: each round reads the first file on a thread of its own while the program's thread
 * reads the second, both with the system's own calls into a buffer each that is made once and reused, and the
 * round joins the thread before it adds up the lengths. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define BUFFER_BYTES (1 << 20)

typedef struct Reading {
    const char* path;
    char* buffer;
    int64_t length;
} Reading;

static void read_whole(Reading* reading) {
#ifdef _WIN32
    HANDLE file = CreateFileA(reading->path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    DWORD got = 0;
    ReadFile(file, reading->buffer, BUFFER_BYTES, &got, NULL);
    CloseHandle(file);
    reading->length = got;
#else
    int file = open(reading->path, O_RDONLY);
    reading->length = read(file, reading->buffer, BUFFER_BYTES);
    close(file);
#endif
    __asm__ volatile("" : : "r"(reading->buffer) : "memory");   /* the text is read, as the program asks, not only measured */
}

#ifdef _WIN32
static DWORD WINAPI reading_thread(LPVOID argument) {
#else
static void* reading_thread(void* argument) {
#endif
    read_whole(argument);
    return 0;
}

static void write_text(const char* path, const char* word, int32_t count) {
    FILE* file = fopen(path, "wb");
    for (int32_t index = 0; index < count; index++) {
        fprintf(file, index == 0 ? "%s line number %d" : "\n%s line number %d", word, index);
    }
    fclose(file);
}

int main(void) {
    Reading first = {".spite/reads_in_a_row_first.txt", malloc(BUFFER_BYTES), 0};
    Reading second = {".spite/reads_in_a_row_second.txt", malloc(BUFFER_BYTES), 0};
    write_text(first.path, "first", 4000);
    write_text(second.path, "second", 4000);
    int64_t start = now_nanoseconds();
    int64_t characters = 0;
    for (int32_t round = 0; round < 40; round++) {
#ifdef _WIN32
        HANDLE thread = CreateThread(NULL, 0, reading_thread, &first, 0, NULL);
        read_whole(&second);
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
#else
        pthread_t thread;
        pthread_create(&thread, NULL, reading_thread, &first);
        read_whole(&second);
        pthread_join(thread, NULL);
#endif
        characters += first.length + second.length;
    }
    int64_t microseconds = microseconds_since(start);
    int first_removed = remove(first.path) == 0;
    int second_removed = remove(second.path) == 0;
    printf("characters %lld removed %s %s\n", (long long)characters, first_removed ? "true" : "false",
        second_removed ? "true" : "false");
    print_microseconds(microseconds);
    free(first.buffer);
    free(second.buffer);
    return 0;
}
