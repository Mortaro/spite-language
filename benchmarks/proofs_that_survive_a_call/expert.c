/* The same work tuned by hand: each monster's health a local number, struck until it falls, and the hits counted
 * in a local; no pointer is held, so there is nothing to check. */
#include <stdint.h>
#include <stdio.h>

int main(void) {
    int32_t rounds = 0;
    int32_t hits = 0;
    for (int32_t index = 0; index < 1000; index++) {
        int32_t health = index % 60;
        while (health > 0) {
            hits++;
            health -= 3;
            rounds++;
        }
    }
    printf("rounds %d hits %d\n", rounds, hits);
    return 0;
}
