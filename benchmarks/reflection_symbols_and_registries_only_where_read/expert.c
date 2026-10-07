/* The same work tuned by hand: lamps and doors as arrays of plain flags, and the live lamps kept as a count, the
 * only thing the program asks of them, rather than a list of who they are. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define LAMP_COUNT 1000
#define DOOR_COUNT 1000

int main(void) {
    bool* lamps = malloc(sizeof(bool) * LAMP_COUNT);
    bool* doors = malloc(sizeof(bool) * DOOR_COUNT);
    int32_t live_lamps = 0;
    for (int32_t index = 0; index < LAMP_COUNT; index++) {
        lamps[index] = index % 3 == 0;
        live_lamps++;
    }
    for (int32_t index = 0; index < DOOR_COUNT; index++) {
        doors[index] = index % 4 == 0;
    }
    int32_t lit = 0;
    int32_t open = 0;
    for (int32_t index = 0; index < LAMP_COUNT; index++) lit += lamps[index];
    for (int32_t index = 0; index < DOOR_COUNT; index++) open += doors[index];
    printf("lamps lit %d doors open %d lamps alive %d\n", lit, open, live_lamps);
    free(lamps);
    free(doors);
    return 0;
}
