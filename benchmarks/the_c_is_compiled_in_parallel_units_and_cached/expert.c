/* The same work tuned by hand: the prices and counts as two arrays of numbers on the stack, and one loop that adds
 * up their products. */
#include <stdint.h>
#include <stdio.h>

#define ITEM_COUNT 500

int main(void) {
    int32_t price[ITEM_COUNT];
    int32_t count[ITEM_COUNT];
    for (int32_t index = 0; index < ITEM_COUNT; index++) {
        price[index] = index % 13;
        count[index] = index % 4;
    }
    int32_t worth = 0;
    for (int32_t index = 0; index < ITEM_COUNT; index++) worth += price[index] * count[index];
    printf("worth %d\n", worth);
    return 0;
}
