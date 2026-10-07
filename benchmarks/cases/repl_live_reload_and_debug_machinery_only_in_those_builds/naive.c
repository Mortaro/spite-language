/* naive/ written in C the way it reads: a struct per monster, a growable array of pointers to them, and ten rounds
 * of adding up their health. A C program has no REPL, no live reload and no allocation table unless its author
 * writes one, which is what a production Spite build carries of them: nothing. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Monster {
    int32_t health;
} Monster;

typedef struct MonsterList {
    Monster** items;
    int32_t count;
    int32_t capacity;
} MonsterList;

static MonsterList* monster_list_make(void) {
    MonsterList* list = malloc(sizeof(MonsterList));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void monster_list_append(MonsterList* list, Monster* monster) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(Monster*) * list->capacity);
    }
    list->items[list->count] = monster;
    list->count = list->count + 1;
}

static Monster* monster_make(int32_t health) {
    Monster* monster = malloc(sizeof(Monster));
    monster->health = health;
    return monster;
}

static int32_t sum_health(MonsterList* monsters) {
    int32_t total = 0;
    for (int32_t index = 0; index < monsters->count; index = index + 1) {
        total = total + monsters->items[index]->health;
    }
    return total;
}

static int32_t total_health(MonsterList* monsters) {
    int32_t total = 0;
    for (int32_t round = 0; round < 10; round = round + 1) {
        total = total + sum_health(monsters);
    }
    return total;
}

int main(void) {
    MonsterList* monsters = monster_list_make();
    for (int32_t index = 0; index < 1000; index = index + 1) {
        Monster* monster = monster_make(index % 50);
        monster_list_append(monsters, monster);
    }
    int32_t total = total_health(monsters);
    printf("health %d\n", total);
    for (int32_t index = 0; index < monsters->count; index = index + 1) {
        free(monsters->items[index]);
    }
    free(monsters->items);
    free(monsters);
    return 0;
}
