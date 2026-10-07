/* naive/ written in C the way it reads: a hunter that holds a pointer to its target, checks it once at the start
 * of a hunt, and then strikes it until it falls, counting each hit. A C programmer checks the pointer once because
 * they know record_hit cannot change it; the Spite compiler works that out for itself. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Monster {
    int32_t health;
} Monster;

typedef struct Hunter {
    Monster* target;
    int32_t hits;
} Hunter;

static Monster* monster_make(int32_t health) {
    Monster* monster = malloc(sizeof(Monster));
    monster->health = health;
    return monster;
}

static void monster_hurt(Monster* monster, int32_t amount) {
    monster->health = monster->health - amount;
}

static void record_hit(Hunter* hunter) {
    hunter->hits = hunter->hits + 1;
}

static int32_t hunt(Hunter* hunter) {
    if (hunter->target == NULL) {
        fprintf(stderr, "hunt: target is null\n");
        exit(1);
    }
    int32_t rounds = 0;
    while (hunter->target->health > 0) {
        record_hit(hunter);
        monster_hurt(hunter->target, 3);
        rounds = rounds + 1;
    }
    return rounds;
}

int main(void) {
    Hunter* hunter = malloc(sizeof(Hunter));
    hunter->target = NULL;
    hunter->hits = 0;
    int32_t rounds = 0;
    for (int32_t index = 0; index < 1000; index = index + 1) {
        Monster* monster = monster_make(index % 60);
        free(hunter->target);
        hunter->target = monster;
        rounds = rounds + hunt(hunter);
    }
    printf("rounds %d hits %d\n", rounds, hunter->hits);
    free(hunter->target);
    free(hunter);
    return 0;
}
