/* The same work tuned by hand: the points as two columns of plain values side by side, the notes as an array of
 * text pointers of their own, so the sum reads the points' memory in order, branchlessly, and never a note. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define COUNT 300000

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t* across = malloc(sizeof(int32_t) * COUNT);
    int32_t* down = malloc(sizeof(int32_t) * COUNT);
    const char** notes = malloc(sizeof(const char*) * COUNT);
    int32_t noted = 0;
    for (int32_t index = 0; index < COUNT; index++) {
        across[index] = index % 1000;
        down[index] = index % 30;
        notes[noted++] = "made";
    }
    int64_t total = 0;
    for (int32_t round = 0; round < 30; round++) {
        int64_t round_total = 0;
        for (int32_t index = 0; index < COUNT; index++) {
            int32_t kept = -(down[index] >= round);
            round_total += (across[index] + down[index]) & kept;
        }
        total += round_total;
    }
    int64_t microseconds = microseconds_since(start);
    printf("total %lld notes %d\n", (long long)total, noted);
    print_microseconds(microseconds);
    free(across);
    free(down);
    free(notes);
    return 0;
}
