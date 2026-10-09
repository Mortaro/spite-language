/* The same work tuned by hand: the velocities as columns (entity, across, down) in arrays made once at their full
 * size, the marks as bytes, and both removals one branchless pass over the rows that writes each row at the kept
 * count and moves the count on only when the row stays. Every round still marks, fills and removes all 200 000. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define ITEM_TOTAL 200000

int main(void) {
    int64_t start = now_nanoseconds();
    int32_t* entity_column = malloc(sizeof(int32_t) * ITEM_TOTAL);
    float* across_column = malloc(sizeof(float) * ITEM_TOTAL);
    float* down_column = malloc(sizeof(float) * ITEM_TOTAL);
    uint8_t* despawned_column = malloc(ITEM_TOTAL);
    int32_t* entities = malloc(sizeof(int32_t) * ITEM_TOTAL);
    uint8_t* marks = malloc(ITEM_TOTAL);
    int64_t checksum = 0;
    for (int32_t round = 0; round < 40; round++) {
        int32_t removed_in_ten = round % 2 == 1 ? 9 : 5;
        for (int32_t entity = 0; entity < ITEM_TOTAL; entity++) {
            marks[entity] = (uint8_t)(entity * 7 % 10 < removed_in_ten);
        }
        for (int32_t entity = 0; entity < ITEM_TOTAL; entity++) {
            entity_column[entity] = entity;
            across_column[entity] = (float)entity;
            down_column[entity] = (float)entity * 0.5f;
            despawned_column[entity] = marks[entity];
            entities[entity] = entity;
        }
        int32_t kept = 0;
        for (int32_t row = 0; row < ITEM_TOTAL; row++) {
            entity_column[kept] = entity_column[row];
            across_column[kept] = across_column[row];
            down_column[kept] = down_column[row];
            despawned_column[kept] = despawned_column[row];
            kept += !despawned_column[row];
        }
        int32_t kept_entities = 0;
        for (int32_t row = 0; row < ITEM_TOTAL; row++) {
            int32_t entity = entities[row];
            entities[kept_entities] = entity;
            kept_entities += !marks[entity];
        }
        int64_t sum = 0;
        for (int32_t row = 0; row < kept; row++) {
            sum += (int64_t)entity_column[row] + entities[row];
        }
        checksum += sum;
    }
    int64_t microseconds = microseconds_since(start);
    printf("checksum %lld\n", (long long)checksum);
    print_microseconds(microseconds);
    free(entity_column);
    free(across_column);
    free(down_column);
    free(despawned_column);
    free(entities);
    free(marks);
    return 0;
}
