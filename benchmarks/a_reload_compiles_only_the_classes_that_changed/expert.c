/* The same fight tuned by hand: the monsters' health and the heroes' damage as two arrays of numbers on the stack,
 * each round one pass that strikes every monster and notes whether any is still alive. */
#include <stdint.h>
#include <stdio.h>

#define COUNT 100

int main(void) {
    int32_t health[COUNT];
    int32_t damage[COUNT];
    for (int32_t index = 0; index < COUNT; index++) {
        health[index] = index % 40;
        damage[index] = index % 7 + 1;
    }
    int32_t any_alive = 0;
    for (int32_t index = 0; index < COUNT; index++) any_alive |= health[index] > 0;
    int32_t rounds = 0;
    while (any_alive) {
        any_alive = 0;
        for (int32_t index = 0; index < COUNT; index++) {
            health[index] -= damage[index];
            any_alive |= health[index] > 0;
        }
        rounds++;
    }
    int32_t alive = 0;
    for (int32_t index = 0; index < COUNT; index++) alive += health[index] > 0;
    printf("rounds %d alive %d\n", rounds, alive);
    return 0;
}
