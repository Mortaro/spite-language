/* The same work tuned by hand: the monsters' health as one array of numbers, and ten rounds of adding it up. The
 * round is added into the array's first value and taken back out, so the C compiler cannot add up one round and
 * multiply it by ten. */
#include <stdint.h>
#include <stdio.h>

#define MONSTER_COUNT 1000

int main(void) {
    int32_t health[MONSTER_COUNT];
    for (int32_t index = 0; index < MONSTER_COUNT; index++) {
        health[index] = index % 50;
    }
    int32_t total = 0;
    for (int32_t round = 0; round < 10; round++) {
        health[0] += round;
        int32_t sum = 0;
        for (int32_t index = 0; index < MONSTER_COUNT; index++) {
            sum += health[index];
        }
        total += sum - round;
        health[0] -= round;
    }
    printf("health %d\n", total);
    return 0;
}
