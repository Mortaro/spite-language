/* The same work tuned by hand: the 32 squares in an array on the stack, written and summed in one pass. */
#include <stdint.h>
#include <stdio.h>
#include "../clock.h"

int main(void) {
    int64_t start = now_nanoseconds();
    int64_t total = 0;
    for (int32_t round = 0; round < 1000000; round++) {
        int64_t squares[32];
        int64_t offset = round % 1000;
        int64_t sum = 0;
        for (int32_t index = 0; index < 32; index++) {
            int64_t value = index + offset;
            squares[index] = value * value;
            sum += squares[index];
        }
        /* keeps the squares written, as the program writes them before it reads them */
        __asm__ volatile("" : : "r"(squares) : "memory");
        total += sum;
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
