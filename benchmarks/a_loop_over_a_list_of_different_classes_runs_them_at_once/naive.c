/* naive/ written in C the way it reads: three voices of three kinds behind one interface (a struct of a function
 * pointer), kept in an array, each rendered in turn on the one thread the program has. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Voice Voice;
struct Voice {
    void (*render)(Voice* voice);
    int64_t level;
    int32_t phase;
};

static void sine_render(Voice* voice) {
    for (int32_t sample = 0; sample < 30000000; sample = sample + 1) {
        voice->phase = (voice->phase + 7) % 1000;
        int32_t folded = voice->phase;
        if (folded > 500) folded = 1000 - folded;
        voice->level = voice->level + folded;
    }
}

static void square_render(Voice* voice) {
    for (int32_t sample = 0; sample < 30000000; sample = sample + 1) {
        voice->phase = (voice->phase + 11) % 1000;
        if (voice->phase < 500) {
            voice->level = voice->level + 3;
        } else {
            voice->level = voice->level + 1;
        }
    }
}

static void saw_render(Voice* voice) {
    for (int32_t sample = 0; sample < 30000000; sample = sample + 1) {
        voice->phase = (voice->phase + 13) % 1000;
        voice->level = voice->level + voice->phase % 97;
    }
}

static Voice* make_voice(void (*render)(Voice* voice)) {
    Voice* voice = malloc(sizeof(Voice));
    voice->render = render;
    voice->level = 0;
    voice->phase = 0;
    return voice;
}

int main(void) {
    Voice* voices[3];
    voices[0] = make_voice(sine_render);
    voices[1] = make_voice(square_render);
    voices[2] = make_voice(saw_render);
    int64_t start = now_nanoseconds();
    for (int index = 0; index < 3; index = index + 1) voices[index]->render(voices[index]);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("%lld %lld %lld\n", (long long)voices[0]->level, (long long)voices[1]->level, (long long)voices[2]->level);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    for (int index = 0; index < 3; index = index + 1) free(voices[index]);
    return 0;
}
