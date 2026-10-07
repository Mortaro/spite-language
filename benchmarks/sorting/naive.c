/* naive/ written in C the way a C programmer writes it:
 * the same quicksort (middle pivot, recursing into the smaller side) on an int32 array. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* The same clock the Spite program reads: the time of the work goes to the error output and the answer to the
 * standard output, so run.sh compares the answers and times the work without the process's start. */
#ifdef _WIN32
#include <windows.h>
static int64_t now_nanoseconds(void) {
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    return (int64_t)((double)counter.QuadPart * 1000000000.0 / (double)frequency.QuadPart);
}
#else
#include <time.h>
static int64_t now_nanoseconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)now.tv_sec * 1000000000 + now.tv_nsec;
}
#endif

static void sort(int32_t* numbers, int32_t low, int32_t high) {
    int32_t left = low;
    int32_t right = high;
    while (left < right) {
        int32_t middle = left + (right - left) / 2;
        int32_t pivot = numbers[middle];
        int32_t lower = left;
        int32_t upper = right;
        while (lower <= upper) {
            while (numbers[lower] < pivot) {
                lower = lower + 1;
            }
            while (numbers[upper] > pivot) {
                upper = upper - 1;
            }
            if (lower <= upper) {
                int32_t swapped = numbers[lower];
                numbers[lower] = numbers[upper];
                numbers[upper] = swapped;
                lower = lower + 1;
                upper = upper - 1;
            }
        }
        if (upper - left < right - lower) {
            sort(numbers, left, upper);
            left = lower;
        } else {
            sort(numbers, lower, right);
            right = upper;
        }
    }
}

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t count = 2000000;
    int32_t* numbers = malloc(sizeof(int32_t) * count);
    int64_t seed = 42;
    for (int32_t index = 0; index < count; index = index + 1) {
        seed = seed * 48271 % 2147483647;
        numbers[index] = (int32_t)(seed % 1000000);
    }
    sort(numbers, 0, count - 1);
    int64_t checksum = 0;
    int ordered = 1;
    for (int32_t index = 0; index < count; index = index + 1) {
        checksum = checksum + numbers[index] * (index % 7);
        if (index > 0 && numbers[index - 1] > numbers[index]) {
            ordered = 0;
        }
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("ordered %s checksum %lld\n", ordered ? "true" : "false", (long long)checksum);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(numbers);
    return 0;
}
