/* The smallest step from what Spite writes today: each component lives inside the entity that owns it, with a byte
 * saying which components are there, instead of being an object of its own behind a pointer. Catching fire writes
 * the fire's fields and sets a bit, going out clears it: nothing is allocated or freed. The entities still come from
 * a pool of blocks side by side and the list still holds pointers to them, and the systems are naive/'s, with the
 * same tests.
 * --items=N, --density=P, --passes=N and --churn=N, as naive/ reads them. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

enum { has_velocity = 1, has_health = 2, has_burning = 4, block = 4096 };

typedef struct Entity {
    int32_t x;
    int32_t y;
    int32_t across;
    int32_t down;
    int32_t points;
    int32_t ticks;
    int32_t damage;
    uint8_t parts;
} Entity;

static int32_t setting(int argument_count, char** arguments, const char* name, int32_t otherwise) {
    size_t length = strlen(name);
    for (int index = 1; index < argument_count; index++) {
        if (strncmp(arguments[index], name, length) == 0 && arguments[index][length] == '=') {
            return atoi(arguments[index] + length + 1);
        }
    }
    return otherwise;
}

int main(int argument_count, char** arguments) {
    int32_t items = setting(argument_count, arguments, "--items", 200000);
    int32_t density = setting(argument_count, arguments, "--density", 50);
    int32_t passes = setting(argument_count, arguments, "--passes", 20);
    int32_t churn = setting(argument_count, arguments, "--churn", 2000);
    int64_t start = now_nanoseconds();
    int32_t capacity = 64;
    Entity** entities = malloc(sizeof(Entity*) * capacity);
    Entity* pool = NULL;
    int32_t left_in_block = 0;
    int64_t seed = 42;
    for (int32_t index = 0; index < items; index++) {
        seed = seed * 48271 % 2147483647;
        if (left_in_block == 0) {
            pool = malloc(sizeof(Entity) * block);
            left_in_block = block;
        }
        Entity* entity = pool++;
        left_in_block--;
        entity->x = (int32_t)(seed % 4096);
        entity->y = (int32_t)(seed / 4096 % 4096);
        entity->parts = 0;
        if (seed / 1048576 % 100 < density) {
            entity->parts |= has_velocity;
            entity->across = (int32_t)(seed % 16);
            entity->down = (int32_t)(seed / 16 % 16);
        }
        if (seed / 3 % 100 < density) {
            entity->parts |= has_health;
            entity->points = (int32_t)(seed / 99 % 100 + 50);
        }
        if (index == capacity) {
            capacity = capacity * 2;
            entities = realloc(entities, sizeof(Entity*) * capacity);
        }
        entities[index] = entity;
    }
    int64_t made = now_nanoseconds();
    int64_t dealt = 0;
    for (int32_t pass = 0; pass < passes; pass++) {
        for (int32_t turn = 0; turn < churn; turn++) {
            seed = seed * 48271 % 2147483647;
            Entity* entity = entities[seed / 7 % items];
            if (!(entity->parts & has_burning)) {
                entity->parts |= has_burning;
                entity->ticks = (int32_t)(seed / 5 % 8 + 1);
                entity->damage = (int32_t)(seed / 40 % 5 + 1);
            }
        }
        for (int32_t index = 0; index < items; index++) {
            Entity* entity = entities[index];
            if (entity->parts & has_velocity) {
                entity->x = (entity->x + entity->across) % 4096;
                entity->y = (entity->y + entity->down) % 4096;
            }
        }
        for (int32_t index = 0; index < items; index++) {
            Entity* entity = entities[index];
            if ((entity->parts & (has_health | has_burning)) == (has_health | has_burning)) {
                entity->points = entity->points - entity->damage;
                dealt = dealt + entity->damage;
            }
        }
        for (int32_t index = 0; index < items; index++) {
            Entity* entity = entities[index];
            if (entity->parts & has_burning) {
                entity->ticks = entity->ticks - 1;
                if (entity->ticks == 0) entity->parts &= (uint8_t)~has_burning;
            }
        }
        for (int32_t index = 0; index < items; index++) {
            Entity* entity = entities[index];
            if (entity->parts & has_health) entity->points = entity->points + 1;
        }
    }
    int64_t positions = 0;
    int64_t points = 0;
    int32_t burning = 0;
    for (int32_t index = 0; index < items; index++) {
        Entity* entity = entities[index];
        positions = positions + entity->x + entity->y;
        if (entity->parts & has_health) points = points + entity->points;
        if (entity->parts & has_burning) burning = burning + 1;
    }
    int64_t finished = now_nanoseconds();
    printf("dealt %lld positions %lld points %lld burning %d\n", (long long)dealt, (long long)positions, (long long)points,
           burning);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld passes %lld\n", (long long)((made - start) / 1000), (long long)((finished - made) / 1000));
    return 0;
}
