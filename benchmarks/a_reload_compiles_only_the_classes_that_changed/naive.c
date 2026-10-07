/* naive/ written in C the way it reads: a struct per monster and per hero, a growable array of pointers for each
 * list, and the fight as rounds of every hero striking its monster until none is alive. A C program is rebuilt
 * whole after a change; a reload of the Spite program compiles only the class that changed. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Monster {
    int32_t health;
} Monster;

typedef struct Hero {
    int32_t strength;
} Hero;

typedef struct List {
    void** items;
    int32_t count;
    int32_t capacity;
} List;

static List* list_make(void) {
    List* list = malloc(sizeof(List));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void list_append(List* list, void* item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(void*) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static void list_free(List* list) {
    for (int32_t index = 0; index < list->count; index = index + 1) {
        free(list->items[index]);
    }
    free(list->items);
    free(list);
}

static Monster* monster_make(int32_t health) {
    Monster* monster = malloc(sizeof(Monster));
    monster->health = health;
    return monster;
}

static void monster_hurt(Monster* monster, int32_t amount) {
    monster->health = monster->health - amount;
}

static bool monster_is_alive(Monster* monster) {
    return monster->health > 0;
}

static Hero* hero_make(int32_t strength) {
    Hero* hero = malloc(sizeof(Hero));
    hero->strength = strength;
    return hero;
}

static int32_t hero_damage(Hero* hero) {
    return hero->strength + 1;
}

static bool any_is_alive(List* monsters) {
    for (int32_t index = 0; index < monsters->count; index = index + 1) {
        if (monster_is_alive(monsters->items[index])) return true;
    }
    return false;
}

static int32_t count_is_alive(List* monsters) {
    int32_t count = 0;
    for (int32_t index = 0; index < monsters->count; index = index + 1) {
        if (monster_is_alive(monsters->items[index])) count = count + 1;
    }
    return count;
}

static void strike_all(List* monsters, List* heroes) {
    for (int32_t index = 0; index < monsters->count; index = index + 1) {
        Monster* monster = monsters->items[index];
        Hero* hero = heroes->items[index];
        int32_t amount = hero_damage(hero);
        monster_hurt(monster, amount);
    }
}

static int32_t fight(List* monsters, List* heroes) {
    int32_t rounds = 0;
    while (any_is_alive(monsters)) {
        strike_all(monsters, heroes);
        rounds = rounds + 1;
    }
    return rounds;
}

int main(void) {
    List* monsters = list_make();
    List* heroes = list_make();
    for (int32_t index = 0; index < 100; index = index + 1) {
        list_append(monsters, monster_make(index % 40));
        list_append(heroes, hero_make(index % 7));
    }
    int32_t rounds = fight(monsters, heroes);
    int32_t alive = count_is_alive(monsters);
    printf("rounds %d alive %d\n", rounds, alive);
    list_free(monsters);
    list_free(heroes);
    return 0;
}
