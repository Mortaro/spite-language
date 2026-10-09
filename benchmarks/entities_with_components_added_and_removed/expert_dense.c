/* The entities kept in the order they were made, as columns over all entities, every component included, with a byte
 * per entity saying which components it has (a column with holes). Catching fire or going out writes one entity's
 * columns and flips a bit; a system walks every entity and tests its byte, which the C compiler turns into a masked
 * select.
 * --items=N, --density=P, --passes=N and --churn=N, as naive/ reads them. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

enum { has_velocity = 1, has_health = 2, has_burning = 4 };

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
    int32_t* restrict x = malloc(sizeof(int32_t) * capacity);
    int32_t* restrict y = malloc(sizeof(int32_t) * capacity);
    int32_t* restrict across = malloc(sizeof(int32_t) * capacity);
    int32_t* restrict down = malloc(sizeof(int32_t) * capacity);
    int32_t* restrict points = malloc(sizeof(int32_t) * capacity);
    int32_t* restrict ticks = malloc(sizeof(int32_t) * capacity);
    int32_t* restrict damage = malloc(sizeof(int32_t) * capacity);
    int32_t* restrict parts = malloc(sizeof(int32_t) * capacity);
    int64_t seed = 42;
    for (int32_t index = 0; index < items; index++) {
        seed = seed * 48271 % 2147483647;
        if (index == capacity) {
            capacity = capacity * 2;
            x = realloc(x, sizeof(int32_t) * capacity);
            y = realloc(y, sizeof(int32_t) * capacity);
            across = realloc(across, sizeof(int32_t) * capacity);
            down = realloc(down, sizeof(int32_t) * capacity);
            points = realloc(points, sizeof(int32_t) * capacity);
            ticks = realloc(ticks, sizeof(int32_t) * capacity);
            damage = realloc(damage, sizeof(int32_t) * capacity);
            parts = realloc(parts, sizeof(int32_t) * capacity);
        }
        x[index] = (int32_t)(seed % 4096);
        y[index] = (int32_t)(seed / 4096 % 4096);
        int32_t kind = 0;
        across[index] = 0;
        down[index] = 0;
        points[index] = 0;
        ticks[index] = 0;
        damage[index] = 0;
        if (seed / 1048576 % 100 < density) {
            kind |= has_velocity;
            across[index] = (int32_t)(seed % 16);
            down[index] = (int32_t)(seed / 16 % 16);
        }
        if (seed / 3 % 100 < density) {
            kind |= has_health;
            points[index] = (int32_t)(seed / 99 % 100 + 50);
        }
        parts[index] = kind;
    }
    int64_t made = now_nanoseconds();
    int64_t dealt = 0;
    for (int32_t pass = 0; pass < passes; pass++) {
        for (int32_t turn = 0; turn < churn; turn++) {
            seed = seed * 48271 % 2147483647;
            int32_t entity = (int32_t)(seed / 7 % items);
            if (!(parts[entity] & has_burning)) {
                parts[entity] |= has_burning;
                ticks[entity] = (int32_t)(seed / 5 % 8 + 1);
                damage[entity] = (int32_t)(seed / 40 % 5 + 1);
            }
        }
        /* an absent velocity is stored as zero, so moving by it changes nothing: no test */
        for (int32_t index = 0; index < items; index++) {
            x[index] = (x[index] + across[index]) & 4095;
            y[index] = (y[index] + down[index]) & 4095;
        }
        /* an absent fire is stored as zero damage, so only the health needs testing */
        int32_t dealt_now = 0;   /* at most five a fire, so a pass's total fits 32 bits and the sum stays in vector lanes */
        for (int32_t index = 0; index < items; index++) {
            int32_t hurt = damage[index] & -((parts[index] >> 1) & 1);   /* a mask, not a branch: health is there at random */
            points[index] = points[index] - hurt;
            dealt_now = dealt_now + hurt;
        }
        dealt = dealt + dealt_now;
        for (int32_t index = 0; index < items; index++) {
            int32_t burning = (parts[index] & has_burning) != 0;
            int32_t left = ticks[index] - burning;
            ticks[index] = left;
            int32_t out = burning & (left == 0);
            damage[index] = damage[index] & (out - 1);
            parts[index] = parts[index] & ~(out << 2);
        }
        for (int32_t index = 0; index < items; index++) points[index] = points[index] + ((parts[index] & has_health) ? 1 : 0);
    }
    int64_t positions = 0;
    int64_t total_points = 0;
    int32_t burning = 0;
    for (int32_t index = 0; index < items; index++) {
        positions = positions + x[index] + y[index];
        total_points = total_points + points[index];
        burning = burning + ((parts[index] & has_burning) != 0);
    }
    int64_t finished = now_nanoseconds();
    printf("dealt %lld positions %lld points %lld burning %d\n", (long long)dealt, (long long)positions,
           (long long)total_points, burning);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld passes %lld\n", (long long)((made - start) / 1000), (long long)((finished - made) / 1000));
    return 0;
}
