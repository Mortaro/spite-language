/* The same work tuned by hand: no attribute walk, the four attributes added where they are read, and the party as
 * four columns of plain values that each round sums in one loop the C compiler vectorises. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define PARTY_COUNT 200000

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t* strength = malloc(sizeof(int32_t) * PARTY_COUNT);
    int32_t* agility = malloc(sizeof(int32_t) * PARTY_COUNT);
    int32_t* wisdom = malloc(sizeof(int32_t) * PARTY_COUNT);
    int32_t* stamina = malloc(sizeof(int32_t) * PARTY_COUNT);
    for (int32_t index = 0; index < PARTY_COUNT; index++) {
        strength[index] = index % 20;
        agility[index] = index % 17;
        wisdom[index] = index % 13;
        stamina[index] = index % 11;
    }
    int64_t total = 0;
    for (int32_t round = 0; round < 40; round++) {
        __asm__ volatile("" : : "r"(strength), "r"(agility), "r"(wisdom), "r"(stamina) : "memory");   /* every round reads the party again, as the program asks */
        int64_t round_total = 0;
        for (int32_t index = 0; index < PARTY_COUNT; index++) {
            round_total += strength[index] + agility[index] + wisdom[index] + stamina[index];
        }
        total += round_total;
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(strength);
    free(agility);
    free(wisdom);
    free(stamina);
    return 0;
}
