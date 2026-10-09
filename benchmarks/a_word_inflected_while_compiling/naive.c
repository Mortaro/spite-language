/* naive/ written in C the way it reads: label calls pluralize on "cactus" every time, which lower-cases the word,
 * looks it up among the uncountable words and the irregular ones, and answers a new text, and then the label is
 * made as a new text of its own. */
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

static const char* uncountables[] = {
    "data", "equipment", "health", "information", "rice", "money", "species", "series", "fish", "sheep", "jeans",
    "police", "deer", "moose", "aircraft", "news", "furniture", "luggage", "advice", "software", "hardware",
    "knowledge",
};

static const char* irregulars[][2] = {
    {"person", "people"}, {"man", "men"}, {"woman", "women"}, {"child", "children"}, {"sex", "sexes"},
    {"move", "moves"}, {"zombie", "zombies"}, {"foot", "feet"}, {"tooth", "teeth"}, {"goose", "geese"},
    {"cactus", "cacti"}, {"fungus", "fungi"}, {"nucleus", "nuclei"}, {"radius", "radii"},
    {"stimulus", "stimuli"}, {"syllabus", "syllabi"}, {"alumnus", "alumni"}, {"criterion", "criteria"},
    {"phenomenon", "phenomena"}, {"appendix", "appendices"}, {"leaf", "leaves"}, {"loaf", "loaves"},
    {"thief", "thieves"}, {"hoof", "hooves"}, {"hero", "heroes"}, {"potato", "potatoes"}, {"echo", "echoes"},
    {"veto", "vetoes"}, {"torpedo", "torpedoes"},
};

static bool ends_with(const char* word, const char* suffix) {
    size_t word_length = strlen(word);
    size_t suffix_length = strlen(suffix);
    return word_length >= suffix_length && strcmp(word + word_length - suffix_length, suffix) == 0;
}

static char* joined(const char* first, const char* second) {
    size_t first_length = strlen(first);
    size_t second_length = strlen(second);
    char* text = malloc(first_length + second_length + 1);
    memcpy(text, first, first_length);
    memcpy(text + first_length, second, second_length + 1);
    return text;
}

static char* pluralize(const char* word) {
    char* lower = joined(word, "");
    for (char* at = lower; *at != 0; at = at + 1) {
        *at = (char)tolower((unsigned char)*at);
    }
    for (size_t index = 0; index < sizeof(uncountables) / sizeof(uncountables[0]); index = index + 1) {
        if (strcmp(lower, uncountables[index]) == 0) return lower;
    }
    for (size_t index = 0; index < sizeof(irregulars) / sizeof(irregulars[0]); index = index + 1) {
        if (strcmp(lower, irregulars[index][0]) == 0) {
            free(lower);
            return joined(irregulars[index][1], "");
        }
    }
    char* plural;
    if (ends_with(lower, "x") || ends_with(lower, "ch") || ends_with(lower, "ss") || ends_with(lower, "sh")) {
        plural = joined(lower, "es");
    } else if (ends_with(lower, "s")) {
        plural = joined(lower, "");
    } else {
        plural = joined(lower, "s");
    }
    free(lower);
    return plural;
}

static char* label(int32_t count) {
    if (count == 1) {
        return joined("1 cactus", "");
    }
    char* plural = pluralize("cactus");
    size_t length = (size_t)snprintf(NULL, 0, "%d %s", count, plural);
    char* text = malloc(length + 1);
    snprintf(text, length + 1, "%d %s", count, plural);
    free(plural);
    return text;
}

static int64_t characters(int32_t count) {
    int64_t total = 0;
    for (int32_t index = 0; index < count; index = index + 1) {
        char* shown = label(index % 50);
        total = total + (int64_t)strlen(shown);
        free(shown);
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int64_t total = characters(1000000);
    int64_t microseconds = microseconds_since(start);
    printf("characters %lld\n", (long long)total);
    print_microseconds(microseconds);
    return 0;
}
