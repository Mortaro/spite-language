/* The same work tuned by hand: the file opened once with the system's own calls, and each round written from the
 * start, cut to the round's length, and read back from the start into one buffer on the stack, so a round is four
 * system calls and no open or close. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../clock.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

int main(void) {
    const char* path = ".spite/concurrency_machinery_notes.txt";
    int64_t start = now_nanoseconds();
    int32_t characters = 0;
#ifdef _WIN32
    HANDLE file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
#else
    int file = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
#endif
    for (int32_t round = 0; round < 50; round++) {
        char text[64];
        char read[64];
        int length = snprintf(text, sizeof(text), "round %d of the notes", round);
#ifdef _WIN32
        DWORD done = 0;
        SetFilePointer(file, 0, NULL, FILE_BEGIN);
        WriteFile(file, text, (DWORD)length, &done, NULL);
        SetEndOfFile(file);
        SetFilePointer(file, 0, NULL, FILE_BEGIN);
        ReadFile(file, read, sizeof(read), &done, NULL);
        characters += (int32_t)done;
#else
        pwrite(file, text, (size_t)length, 0);
        ftruncate(file, length);
        characters += (int32_t)pread(file, read, sizeof(read), 0);
#endif
    }
#ifdef _WIN32
    CloseHandle(file);
#else
    close(file);
#endif
    int removed = remove(path) == 0;
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("characters %d removed %s\n", characters, removed ? "true" : "false");
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
