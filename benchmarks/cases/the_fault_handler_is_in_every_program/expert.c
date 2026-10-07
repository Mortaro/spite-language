/* The same work tuned by hand: the numbers in an array on the stack and one loop over them. An expert who wants a
 * fault reported writes a handler of their own; this one, like most C, writes none. */
#include <stdint.h>
#include <stdio.h>

#define NUMBER_COUNT 100

int main(void) {
    int32_t numbers[NUMBER_COUNT];
    for (int32_t index = 0; index < NUMBER_COUNT; index++) numbers[index] = index % 9;
    int32_t total = 0;
    for (int32_t index = 0; index < NUMBER_COUNT; index++) total += numbers[index] * index;
    printf("weighted %d\n", total);
    return 0;
}
