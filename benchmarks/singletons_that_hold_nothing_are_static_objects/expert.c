/* The same work written by hand: the squares and the joined names in static buffers of their final size, each name
 * written straight into the joined text, and nothing allocated at all. */
#include <stdint.h>
#include <stdio.h>

static int64_t squares[1000];
static char joined[1024];

int main(void) {
    for (int32_t index = 0; index < 1000; index++) {
        int64_t wide = index;
        squares[index] = wide * index;
    }
    int32_t characters = 0;
    for (int32_t index = 0; index < 100; index++) {
        if (index > 0) joined[characters++] = ',';
        characters += snprintf(joined + characters, sizeof(joined) - (size_t)characters, "name %d", index);
    }
    printf("squares %d largest %lld characters %d\n", 1000, (long long)squares[999], characters);
    return 0;
}
