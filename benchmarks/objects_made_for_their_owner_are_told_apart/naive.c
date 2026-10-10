/* naive/ written in C the way it reads: two tracks of two kinds behind one interface (a struct of a function
 * pointer), each with a meter of its own made for it and a list of loud samples in the meter, rendered in turn on
 * the one thread the program has. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Meter {
    int64_t total;
    int32_t peak;
    int32_t* loud;
    int32_t loud_count;
    int32_t loud_room;
} Meter;

typedef struct Track Track;
struct Track {
    void (*render)(Track* track);
    Meter* meter;
    int32_t phase;
};

static Meter* make_meter(void) {
    Meter* meter = malloc(sizeof(Meter));
    meter->total = 0;
    meter->peak = 0;
    meter->loud = 0;
    meter->loud_count = 0;
    meter->loud_room = 0;
    return meter;
}

static void record(Meter* meter, int32_t sample) {
    meter->total = meter->total + sample;
    if (sample > meter->peak) meter->peak = sample;
    if (sample > 995) {
        if (meter->loud_count == meter->loud_room) {
            meter->loud_room = meter->loud_room == 0 ? 16 : meter->loud_room * 2;
            meter->loud = realloc(meter->loud, sizeof(int32_t) * meter->loud_room);
        }
        meter->loud[meter->loud_count] = sample;
        meter->loud_count = meter->loud_count + 1;
    }
}

static void drum_render(Track* track) {
    for (int32_t step = 0; step < 20000000; step = step + 1) {
        track->phase = (track->phase + 7) % 1000;
        record(track->meter, track->phase);
    }
}

static void bass_render(Track* track) {
    for (int32_t step = 0; step < 20000000; step = step + 1) {
        track->phase = (track->phase + 13) % 1000;
        int32_t folded = track->phase;
        if (folded > 500) folded = 1000 - folded;
        record(track->meter, folded * 2);
    }
}

static Track* make_track(void (*render)(Track* track)) {
    Track* track = malloc(sizeof(Track));
    track->render = render;
    track->meter = make_meter();
    track->phase = 0;
    return track;
}

int main(void) {
    Track* tracks[2];
    tracks[0] = make_track(drum_render);
    tracks[1] = make_track(bass_render);
    int64_t start = now_nanoseconds();
    for (int index = 0; index < 2; index = index + 1) tracks[index]->render(tracks[index]);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    Meter* drum = tracks[0]->meter;
    Meter* bass = tracks[1]->meter;
    printf("%lld %d %d %lld %d %d\n", (long long)drum->total, drum->peak, drum->loud_count, (long long)bass->total,
        bass->peak, bass->loud_count);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    for (int index = 0; index < 2; index = index + 1) {
        free(tracks[index]->meter->loud);
        free(tracks[index]->meter);
        free(tracks[index]);
    }
    return 0;
}
