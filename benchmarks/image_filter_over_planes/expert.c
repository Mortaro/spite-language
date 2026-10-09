/* The image tuned by hand: four planes of bytes, and the two filters fused into one function of a channel applied to
 * each colour plane in a pass of its own that the C compiler vectorises; then a vectorised pass that works out each
 * pixel's luminance into a plane of its own and counts the bright opaque pixels, and one that sums the weights.
 * (Fusing all of it into a single loop measured four times slower, and the luminance and the weights in one loop
 * slower than two: a loop with several stores and sums of different widths is no longer vectorised.) The histogram,
 * a scatter no vector instruction of the target does, is a last pass over the luminance plane into four histograms
 * (so consecutive equal levels do not wait on each other) added at the end. --pixels=N sets how many pixels. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

static int32_t setting_of(int argument_count, char** arguments, const char* flag, int32_t otherwise) {
    size_t flag_length = strlen(flag);
    for (int index = 1; index < argument_count; index++) {
        if (strncmp(arguments[index], flag, flag_length) == 0) return atoi(arguments[index] + flag_length);
    }
    return otherwise;
}

static inline int32_t filtered(int32_t channel) {
    int32_t lifted = channel + 16 < 255 ? channel + 16 : 255;
    int32_t spread = (lifted - 128) * 5 / 4 + 128;
    return spread < 0 ? 0 : spread > 255 ? 255 : spread;
}

static void filter(uint8_t* restrict plane, int32_t count) {
    for (int32_t index = 0; index < count; index++) plane[index] = (uint8_t)filtered(plane[index]);
}

int main(int argument_count, char** arguments) {
    int32_t count = setting_of(argument_count, arguments, "--pixels=", 4194304);
    int64_t start = now_nanoseconds();
    uint8_t* restrict red = malloc(count);
    uint8_t* restrict green = malloc(count);
    uint8_t* restrict blue = malloc(count);
    uint8_t* restrict alpha = malloc(count);
    uint8_t* restrict levels = malloc(count);
    int64_t seed = 11;
    for (int32_t index = 0; index < count; index++) {
        seed = seed * 48271 % 2147483647;
        red[index] = (uint8_t)(seed % 256);
        green[index] = (uint8_t)(seed / 256 % 256);
        blue[index] = (uint8_t)(seed / 65536 % 256);
        alpha[index] = (uint8_t)(seed / 16777216 * 2);
    }
    int64_t made = now_nanoseconds();
    filter(red, count);
    filter(green, count);
    filter(blue, count);
    int64_t filtered_at = now_nanoseconds();
    int32_t bright = 0;
    for (int32_t index = 0; index < count; index++) {
        int32_t level = (red[index] * 77 + green[index] * 150 + blue[index] * 29) / 256;
        levels[index] = (uint8_t)level;
        bright += (alpha[index] > 128) & (level > 100);
    }
    int64_t weights = 0;
    for (int32_t index = 0; index < count; index++) {
        weights += red[index] + green[index] * 2 + blue[index] * 3 + alpha[index] * 5;
    }
    int32_t histograms[4][256] = {{0}};
    int32_t index = 0;
    for (; index + 4 <= count; index += 4) {
        histograms[0][levels[index]]++;
        histograms[1][levels[index + 1]]++;
        histograms[2][levels[index + 2]]++;
        histograms[3][levels[index + 3]]++;
    }
    for (; index < count; index++) histograms[0][levels[index]]++;
    int64_t spread = 0;
    for (int32_t level = 0; level < 256; level++) {
        int32_t total = histograms[0][level] + histograms[1][level] + histograms[2][level] + histograms[3][level];
        spread += total * (level % 7 + 1);
    }
    int64_t finished = now_nanoseconds();
    printf("spread %lld bright %d weights %lld\n", (long long)spread, bright, (long long)weights);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld filter %lld measure %lld\n", (long long)((made - start) / 1000),
        (long long)((filtered_at - made) / 1000), (long long)((finished - filtered_at) / 1000));
    return 0;
}
