/* The entities kept in the order they were made, with each component in a sparse set of its own: the position is a
 * column over all entities, and the velocity, the health and the burning are each a dense array of values with the
 * entity that owns each row, plus a map from entity to row (-1 when it has none). Catching fire appends a row and
 * going out moves the set's last row into the gap; nothing else moves. A system over one component walks its set; a
 * system over two walks the smaller set and probes the other's map.
 * --items=N, --density=P, --passes=N and --churn=N, as naive/ reads them. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

typedef struct Set {
    int32_t count;
    int32_t capacity;
    int32_t* owner;
    int32_t* first;
    int32_t* second;
    int32_t* row_of;
} Set;

static int32_t setting(int argument_count, char** arguments, const char* name, int32_t otherwise) {
    size_t length = strlen(name);
    for (int index = 1; index < argument_count; index++) {
        if (strncmp(arguments[index], name, length) == 0 && arguments[index][length] == '=') {
            return atoi(arguments[index] + length + 1);
        }
    }
    return otherwise;
}

static void add(Set* set, int32_t entity, int32_t first, int32_t second) {
    if (set->count == set->capacity) {
        set->capacity = set->capacity ? set->capacity * 2 : 64;
        set->owner = realloc(set->owner, sizeof(int32_t) * set->capacity);
        set->first = realloc(set->first, sizeof(int32_t) * set->capacity);
        set->second = realloc(set->second, sizeof(int32_t) * set->capacity);
    }
    set->owner[set->count] = entity;
    set->first[set->count] = first;
    set->second[set->count] = second;
    set->row_of[entity] = set->count;
    set->count++;
}

static void take_row(Set* set, int32_t row) {
    int32_t entity = set->owner[row];
    int32_t last = --set->count;
    int32_t moved = set->owner[last];
    set->owner[row] = moved;
    set->first[row] = set->first[last];
    set->second[row] = set->second[last];
    set->row_of[moved] = row;
    set->row_of[entity] = -1;
}

int main(int argument_count, char** arguments) {
    int32_t items = setting(argument_count, arguments, "--items", 200000);
    int32_t density = setting(argument_count, arguments, "--density", 50);
    int32_t passes = setting(argument_count, arguments, "--passes", 20);
    int32_t churn = setting(argument_count, arguments, "--churn", 2000);
    int64_t start = now_nanoseconds();
    int32_t capacity = 64;
    int32_t* x = malloc(sizeof(int32_t) * capacity);
    int32_t* y = malloc(sizeof(int32_t) * capacity);
    Set velocities = {0};
    Set healths = {0};
    Set fires = {0};
    velocities.row_of = malloc(sizeof(int32_t) * capacity);
    healths.row_of = malloc(sizeof(int32_t) * capacity);
    fires.row_of = malloc(sizeof(int32_t) * capacity);
    int64_t seed = 42;
    for (int32_t index = 0; index < items; index++) {
        seed = seed * 48271 % 2147483647;
        if (index == capacity) {
            capacity = capacity * 2;
            x = realloc(x, sizeof(int32_t) * capacity);
            y = realloc(y, sizeof(int32_t) * capacity);
            velocities.row_of = realloc(velocities.row_of, sizeof(int32_t) * capacity);
            healths.row_of = realloc(healths.row_of, sizeof(int32_t) * capacity);
            fires.row_of = realloc(fires.row_of, sizeof(int32_t) * capacity);
        }
        x[index] = (int32_t)(seed % 4096);
        y[index] = (int32_t)(seed / 4096 % 4096);
        velocities.row_of[index] = -1;
        healths.row_of[index] = -1;
        fires.row_of[index] = -1;
        if (seed / 1048576 % 100 < density) add(&velocities, index, (int32_t)(seed % 16), (int32_t)(seed / 16 % 16));
        if (seed / 3 % 100 < density) add(&healths, index, (int32_t)(seed / 99 % 100 + 50), 0);
    }
    int64_t made = now_nanoseconds();
    int64_t dealt = 0;
    for (int32_t pass = 0; pass < passes; pass++) {
        for (int32_t turn = 0; turn < churn; turn++) {
            seed = seed * 48271 % 2147483647;
            int32_t entity = (int32_t)(seed / 7 % items);
            if (fires.row_of[entity] < 0) add(&fires, entity, (int32_t)(seed / 5 % 8 + 1), (int32_t)(seed / 40 % 5 + 1));
        }
        for (int32_t row = 0; row < velocities.count; row++) {
            int32_t entity = velocities.owner[row];
            x[entity] = (x[entity] + velocities.first[row]) & 4095;
            y[entity] = (y[entity] + velocities.second[row]) & 4095;
        }
        if (fires.count <= healths.count) {
            for (int32_t row = 0; row < fires.count; row++) {
                int32_t health = healths.row_of[fires.owner[row]];
                if (health >= 0) {
                    healths.first[health] = healths.first[health] - fires.second[row];
                    dealt = dealt + fires.second[row];
                }
            }
        } else {
            for (int32_t row = 0; row < healths.count; row++) {
                int32_t fire = fires.row_of[healths.owner[row]];
                if (fire >= 0) {
                    healths.first[row] = healths.first[row] - fires.second[fire];
                    dealt = dealt + fires.second[fire];
                }
            }
        }
        for (int32_t row = fires.count - 1; row >= 0; row--) {
            fires.first[row] = fires.first[row] - 1;
            if (fires.first[row] == 0) take_row(&fires, row);
        }
        int32_t* restrict health_points = healths.first;
        for (int32_t row = 0; row < healths.count; row++) health_points[row] = health_points[row] + 1;
    }
    int64_t positions = 0;
    int64_t points = 0;
    for (int32_t index = 0; index < items; index++) positions = positions + x[index] + y[index];
    for (int32_t row = 0; row < healths.count; row++) points = points + healths.first[row];
    int64_t finished = now_nanoseconds();
    printf("dealt %lld positions %lld points %lld burning %d\n", (long long)dealt, (long long)positions, (long long)points,
           fires.count);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld passes %lld\n", (long long)((made - start) / 1000), (long long)((finished - made) / 1000));
    return 0;
}
