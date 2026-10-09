/* naive/ written in C the way a C programmer writes it: a struct per entity with a pointer to each component it may
 * have (NULL when it has none), one malloc per object, a burning freed when it goes out, a growable array of
 * pointers, and the same systems: every pass sets --churn entities chosen at random on fire, moves the entities with
 * a velocity, burns the ones with health that are on fire, cools every fire (putting it out when its ticks run out)
 * and heals every entity with health.
 * --items=N, --density=P (the percent of entities with a velocity, and with health), --passes=N and --churn=N. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

typedef struct Velocity {
    int32_t across;
    int32_t down;
} Velocity;

typedef struct Health {
    int32_t points;
} Health;

typedef struct Burning {
    int32_t ticks;
    int32_t damage;
} Burning;

typedef struct Entity {
    int32_t x;
    int32_t y;
    Velocity* velocity;
    Health* health;
    Burning* burning;
} Entity;

typedef struct Entities {
    Entity** items;
    int32_t count;
    int32_t capacity;
} Entities;

static void append(Entities* entities, Entity* entity) {
    if (entities->count == entities->capacity) {
        entities->capacity = entities->capacity ? entities->capacity * 2 : 8;
        entities->items = realloc(entities->items, sizeof(Entity*) * entities->capacity);
    }
    entities->items[entities->count] = entity;
    entities->count = entities->count + 1;
}

static Entity* make_entity(int64_t seed, int32_t density) {
    Entity* entity = malloc(sizeof(Entity));
    entity->x = (int32_t)(seed % 4096);
    entity->y = (int32_t)(seed / 4096 % 4096);
    entity->velocity = NULL;
    entity->health = NULL;
    entity->burning = NULL;
    if (seed / 1048576 % 100 < density) {
        entity->velocity = malloc(sizeof(Velocity));
        entity->velocity->across = (int32_t)(seed % 16);
        entity->velocity->down = (int32_t)(seed / 16 % 16);
    }
    if (seed / 3 % 100 < density) {
        entity->health = malloc(sizeof(Health));
        entity->health->points = (int32_t)(seed / 99 % 100 + 50);
    }
    return entity;
}

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
    Entities entities = {0};
    int64_t seed = 42;
    for (int32_t index = 0; index < items; index++) {
        seed = seed * 48271 % 2147483647;
        append(&entities, make_entity(seed, density));
    }
    int64_t made = now_nanoseconds();
    int64_t dealt = 0;
    for (int32_t pass = 0; pass < passes; pass++) {
        for (int32_t turn = 0; turn < churn; turn++) {
            seed = seed * 48271 % 2147483647;
            Entity* entity = entities.items[seed / 7 % entities.count];
            if (!entity->burning) {
                entity->burning = malloc(sizeof(Burning));
                entity->burning->ticks = (int32_t)(seed / 5 % 8 + 1);
                entity->burning->damage = (int32_t)(seed / 40 % 5 + 1);
            }
        }
        for (int32_t index = 0; index < entities.count; index++) {
            Entity* entity = entities.items[index];
            if (entity->velocity) {
                entity->x = (entity->x + entity->velocity->across) % 4096;
                entity->y = (entity->y + entity->velocity->down) % 4096;
            }
        }
        for (int32_t index = 0; index < entities.count; index++) {
            Entity* entity = entities.items[index];
            if (entity->health && entity->burning) {
                entity->health->points = entity->health->points - entity->burning->damage;
                dealt = dealt + entity->burning->damage;
            }
        }
        for (int32_t index = 0; index < entities.count; index++) {
            Entity* entity = entities.items[index];
            if (entity->burning) {
                entity->burning->ticks = entity->burning->ticks - 1;
                if (entity->burning->ticks == 0) {
                    free(entity->burning);
                    entity->burning = NULL;
                }
            }
        }
        for (int32_t index = 0; index < entities.count; index++) {
            Entity* entity = entities.items[index];
            if (entity->health) entity->health->points = entity->health->points + 1;
        }
    }
    int64_t positions = 0;
    int64_t points = 0;
    int32_t burning = 0;
    for (int32_t index = 0; index < entities.count; index++) {
        Entity* entity = entities.items[index];
        positions = positions + entity->x + entity->y;
        if (entity->health) points = points + entity->health->points;
        if (entity->burning) burning = burning + 1;
    }
    int64_t finished = now_nanoseconds();
    printf("dealt %lld positions %lld points %lld burning %d\n", (long long)dealt, (long long)positions, (long long)points,
           burning);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld passes %lld\n", (long long)((made - start) / 1000), (long long)((finished - made) / 1000));
    return 0;
}
