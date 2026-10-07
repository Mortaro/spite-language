/* The same work tuned by hand: each attribute's name kept with its length beside it, the short one's bytes inside
 * the 16-byte value as the Spite symbol table keeps them, so adding up the names' lengths for each monster reads
 * no text; the monsters' power as one array of numbers. */
#include <stdint.h>
#include <stdio.h>

typedef struct Name {
    char bytes[15];
    uint8_t length;
} Name;

typedef struct LongName {
    const char* bytes;
    int64_t length;
} LongName;

#define MONSTER_COUNT 300

int main(void) {
    static const Name health = {"health", 6};
    static const LongName experience = {"experience_points_gained", 24};
    int32_t experience_points[MONSTER_COUNT];
    for (int32_t index = 0; index < MONSTER_COUNT; index++) experience_points[index] = index;
    int32_t letters = 0;
    for (int32_t index = 0; index < MONSTER_COUNT; index++) {
        letters += health.length;
        letters += (int32_t)experience.length;
    }
    int32_t power = 0;
    for (int32_t index = 0; index < MONSTER_COUNT; index++) power += 10 + experience_points[index];
    printf("letters %d power %d\n", letters, power);
    return 0;
}
