/* The same work tuned by hand: the prices and picks in arrays on the stack, each check a comparison that stops the
 * program with a short code of its own, so no condition text is in the executable. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define PRICE_COUNT 50

static void failed(int code) {
    fprintf(stderr, "failed %d\n", code);
    exit(1);
}

int main(void) {
    int32_t prices[PRICE_COUNT];
    for (int32_t index = 0; index < PRICE_COUNT; index++) prices[index] = index * 3;
    static const int32_t picks[6] = {4, 9, 16, 25, 36, 49};
    int32_t total = 0;
    for (int32_t index = 0; index < 6; index++) {
        int32_t pick = picks[index];
        if ((uint32_t)pick >= PRICE_COUNT) failed(1);
        total += prices[pick];
    }
    int32_t place = 49;
    if (place >= PRICE_COUNT) failed(2);
    int32_t last = prices[place];
    printf("total %d last %d\n", total, last);
    return 0;
}
