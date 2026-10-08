/* naive/ written in C the way it reads: a rack holding an array of pointers to crates, and place() storing a pointer
 * into a slot after reading the weight there. Nothing is counted, since C has no counts. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Crate {
    int32_t weight;
} Crate;

typedef struct Rack {
    Crate** crates;
    int32_t count;
    Crate* light;
    Crate* heavy;
} Rack;

static int32_t place(Rack* self, int32_t at, Crate* crate) {
    if (at < 0 || at >= self->count) abort();
    int32_t before = self->crates[at]->weight;
    self->crates[at] = crate;
    return before == crate->weight ? 0 : 1;
}

static int32_t restock(Rack* self, int32_t count) {
    int32_t changed = 0;
    for (int32_t index = 0; index < count; index = index + 1) {
        int32_t at = index % 16;
        if (index % 64 == 0) changed = changed + place(self, at, self->heavy);
        else changed = changed + place(self, at, self->light);
    }
    return changed;
}

int main(void) {
    Rack* rack = malloc(sizeof(Rack));
    rack->light = malloc(sizeof(Crate));
    rack->heavy = malloc(sizeof(Crate));
    rack->light->weight = 2;
    rack->heavy->weight = 9;
    rack->count = 16;
    rack->crates = malloc(16 * sizeof(Crate*));
    for (int32_t index = 0; index < 16; index = index + 1) rack->crates[index] = rack->light;
    int64_t start = now_nanoseconds();
    int32_t changed = restock(rack, 10000000);
    int32_t total = 0;
    for (int32_t index = 0; index < rack->count; index = index + 1) total = total + rack->crates[index]->weight;
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("changed %d total %d\n", changed, total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(rack->crates);
    free(rack->light);
    free(rack->heavy);
    free(rack);
    return 0;
}
