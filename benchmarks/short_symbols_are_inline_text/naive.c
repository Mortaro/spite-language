/* naive/ written in C the way it reads: a struct per monster, a growable array of pointers, each attribute's name
 * a pointer to constant text whose length is measured with strlen for every monster, and the sum of the
 * monsters' power. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Monster {
    int32_t health;
    int32_t experience_points_gained;
} Monster;

typedef struct MonsterList {
    Monster** items;
    int32_t count;
    int32_t capacity;
} MonsterList;

static const char* attribute_names[2] = {"health", "experience_points_gained"};

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

static Monster* monster_make(int32_t starting_experience) {
    Monster* monster = malloc(sizeof(Monster));
    monster->health = 10;
    monster->experience_points_gained = starting_experience;
    return monster;
}

static int32_t monster_power(Monster* monster) {
    return monster->health + monster->experience_points_gained;
}

static int32_t letters = 0;

static void add_name_length(const char* name) {
    letters = letters + (int32_t)strlen(name);
}

static void count_letters(Monster* monster) {
    (void)monster;
    for (int32_t index = 0; index < 2; index = index + 1) {
        add_name_length(attribute_names[index]);
    }
}

static int32_t sum_power(MonsterList* monsters) {
    int32_t total = 0;
    for (int32_t index = 0; index < monsters->count; index = index + 1) {
        total = total + monster_power(monsters->items[index]);
    }
    return total;
}

int main(void) {
    MonsterList* monsters = monster_list_make();
    for (int32_t index = 0; index < 300; index = index + 1) {
        Monster* monster = monster_make(index);
        monster_list_append(monsters, monster);
    }
    for (int32_t index = 0; index < monsters->count; index = index + 1) {
        count_letters(monsters->items[index]);
    }
    int32_t power = sum_power(monsters);
    printf("letters %d power %d\n", letters, power);
    for (int32_t index = 0; index < monsters->count; index = index + 1) {
        free(monsters->items[index]);
    }
    free(monsters->items);
    free(monsters);
    return 0;
}
