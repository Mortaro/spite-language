/* naive/ written in C the way it reads: a struct for the mixer, made with malloc, and a function call per value. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Mixer {
    int32_t factor;
} Mixer;

static Mixer* mixer_make(int32_t factor) {
    Mixer* mixer = malloc(sizeof(Mixer));
    mixer->factor = factor;
    return mixer;
}

static int32_t mixer_mixed(Mixer* mixer, int32_t value) {
    return (value % 65536 * mixer->factor + 7) % 1000;
}

static int64_t checksum(Mixer* mixer, int32_t count) {
    int64_t total = 0;
    for (int32_t index = 0; index < count; index = index + 1) {
        total = total + mixer_mixed(mixer, index);
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    Mixer* mixer = mixer_make(31);
    int64_t total = checksum(mixer, 50000000);
    int64_t microseconds = microseconds_since(start);
    printf("checksum %lld\n", (long long)total);
    print_microseconds(microseconds);
    free(mixer);
    return 0;
}
