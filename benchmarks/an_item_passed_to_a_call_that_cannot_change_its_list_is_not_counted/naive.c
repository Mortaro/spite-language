/* naive/ written in C the way it reads: a playlist holding a growable array of pointers to tracks allocated one by
 * one; each pass hands one track to a small function and reads its seconds again. Nothing is counted. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Track {
    int32_t seconds;
} Track;

typedef struct Tracks {
    Track** items;
    int32_t count;
    int32_t capacity;
} Tracks;

typedef struct Playlist {
    Tracks* tracks;
} Playlist;

static void tracks_append(Tracks* self, Track* track) {
    if (self->count == self->capacity) {
        self->capacity = self->capacity == 0 ? 4 : self->capacity * 2;
        self->items = realloc(self->items, (size_t)self->capacity * sizeof(Track*));
    }
    self->items[self->count] = track;
    self->count = self->count + 1;
}

static Track* track_at(Tracks* self, int32_t at) {
    if (at < 0 || at >= self->count) abort();
    return self->items[at];
}

static int32_t length_of(Playlist* self, Track* track) {
    (void)self;
    return track->seconds;
}

static int32_t playlist_listen(Playlist* self, int32_t count) {
    int32_t total = 0;
    for (int32_t index = 0; index < count; index = index + 1) {
        int32_t at = index % 16;
        total = total + length_of(self, track_at(self->tracks, at)) % 7 + track_at(self->tracks, at)->seconds % 5;
    }
    return total;
}

int main(void) {
    Playlist* playlist = malloc(sizeof(Playlist));
    playlist->tracks = calloc(1, sizeof(Tracks));
    for (int32_t index = 0; index < 16; index = index + 1) {
        Track* track = malloc(sizeof(Track));
        track->seconds = 120 + index;
        tracks_append(playlist->tracks, track);
    }
    int64_t start = now_nanoseconds();
    int32_t total = playlist_listen(playlist, 10000000);
    int64_t microseconds = microseconds_since(start);
    printf("total %d\n", total);
    print_microseconds(microseconds);
    for (int32_t index = 0; index < playlist->tracks->count; index = index + 1) free(playlist->tracks->items[index]);
    free(playlist->tracks->items);
    free(playlist->tracks);
    free(playlist);
    return 0;
}
