/* naive/ written in C the way it reads: stats.attributes is a list of attribute objects made from a table that
 * describes Stats (each one's name, class and where its value lies), and each calls add_attribute, which asks the
 * attribute's class and reads its value through it. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

enum { CLASS_INTEGER = 1 };

typedef struct Stats {
    int32_t strength;
    int32_t agility;
    int32_t wisdom;
    int32_t stamina;
} Stats;

typedef struct AttributeDescription {
    const char* name;
    int32_t class_id;
    size_t offset;
} AttributeDescription;

static const AttributeDescription stats_attributes[] = {
    {"strength", CLASS_INTEGER, offsetof(Stats, strength)},
    {"agility", CLASS_INTEGER, offsetof(Stats, agility)},
    {"wisdom", CLASS_INTEGER, offsetof(Stats, wisdom)},
    {"stamina", CLASS_INTEGER, offsetof(Stats, stamina)},
};

typedef struct Attribute {
    const char* name;
    int32_t class_id;
    void* owner;
    void* value;
} Attribute;

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
    free(list->items);
    free(list);
}

static Stats* stats_make(int32_t seed) {
    Stats* stats = malloc(sizeof(Stats));
    stats->strength = seed % 20;
    stats->agility = seed % 17;
    stats->wisdom = seed % 13;
    stats->stamina = seed % 11;
    return stats;
}

static List* attributes_of(Stats* stats) {
    List* attributes = list_make();
    for (size_t index = 0; index < sizeof(stats_attributes) / sizeof(stats_attributes[0]); index = index + 1) {
        Attribute* attribute = malloc(sizeof(Attribute));
        attribute->name = stats_attributes[index].name;
        attribute->class_id = stats_attributes[index].class_id;
        attribute->owner = stats;
        attribute->value = (char*)stats + stats_attributes[index].offset;
        list_append(attributes, attribute);
    }
    return attributes;
}

static int32_t running = 0;

static void add_attribute(Attribute* attribute) {
    if (attribute->class_id == CLASS_INTEGER) {
        running = running + *(int32_t*)attribute->value;
    }
}

static int32_t total_of(Stats* stats) {
    running = 0;
    List* attributes = attributes_of(stats);
    for (int32_t index = 0; index < attributes->count; index = index + 1) {
        add_attribute(attributes->items[index]);
    }
    for (int32_t index = 0; index < attributes->count; index = index + 1) {
        free(attributes->items[index]);
    }
    list_free(attributes);
    return running;
}

static List* make_party(int32_t count) {
    List* party = list_make();
    for (int32_t index = 0; index < count; index = index + 1) {
        Stats* stats = stats_make(index);
        list_append(party, stats);
    }
    return party;
}

static int64_t party_total(List* party) {
    int64_t total = 0;
    for (int32_t index = 0; index < party->count; index = index + 1) {
        Stats* stats = party->items[index];
        int32_t points = total_of(stats);
        total = total + points;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    List* party = make_party(200000);
    int64_t total = 0;
    for (int32_t round = 0; round < 40; round = round + 1) {
        total = total + party_total(party);
    }
    int64_t microseconds = microseconds_since(start);
    printf("total %lld\n", (long long)total);
    print_microseconds(microseconds);
    for (int32_t index = 0; index < party->count; index = index + 1) {
        free(party->items[index]);
    }
    list_free(party);
    return 0;
}
