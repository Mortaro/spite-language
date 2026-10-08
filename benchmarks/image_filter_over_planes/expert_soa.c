/* The image as four planes of bytes, one per channel (a structure of arrays narrowed to the channel's size), with the
 * same passes as naive.c. Each filter is a loop over one plane at a time that the C compiler vectorises sixteen bytes
 * at a time, and the alpha plane is never read by them. --pixels=N sets how many pixels. */
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

static void brighten(uint8_t* restrict plane, int32_t count) {
    for (int32_t index = 0; index < count; index++) {
        int32_t lifted = plane[index] + 16;
        plane[index] = (uint8_t)(lifted < 255 ? lifted : 255);
    }
}

static void contrast(uint8_t* restrict plane, int32_t count) {
    for (int32_t index = 0; index < count; index++) {
        int32_t spread = (plane[index] - 128) * 5 / 4 + 128;
        plane[index] = (uint8_t)(spread < 0 ? 0 : spread > 255 ? 255 : spread);
    }
}

int main(int argument_count, char** arguments) {
    int32_t count = setting_of(argument_count, arguments, "--pixels=", 4194304);
    int64_t start = now_nanoseconds();
    uint8_t* restrict red = malloc(count);
    uint8_t* restrict green = malloc(count);
    uint8_t* restrict blue = malloc(count);
    uint8_t* restrict alpha = malloc(count);
    int64_t seed = 11;
    for (int32_t index = 0; index < count; index++) {
        seed = seed * 48271 % 2147483647;
        red[index] = (uint8_t)(seed % 256);
        green[index] = (uint8_t)(seed / 256 % 256);
        blue[index] = (uint8_t)(seed / 65536 % 256);
        alpha[index] = (uint8_t)(seed / 16777216 * 2);
    }
    int64_t made = now_nanoseconds();
    brighten(red, count);
    brighten(green, count);
    brighten(blue, count);
    contrast(red, count);
    contrast(green, count);
    contrast(blue, count);
    int64_t filtered = now_nanoseconds();
    int32_t histogram[256] = {0};
    for (int32_t index = 0; index < count; index++) {
        histogram[(red[index] * 77 + green[index] * 150 + blue[index] * 29) / 256]++;
    }
    int32_t bright = 0;
    for (int32_t index = 0; index < count; index++) {
        bright += (alpha[index] > 128) & ((red[index] * 77 + green[index] * 150 + blue[index] * 29) / 256 > 100);
    }
    int64_t weights = 0;
    for (int32_t index = 0; index < count; index++) {
        weights += red[index] + green[index] * 2 + blue[index] * 3 + alpha[index] * 5;
    }
    int64_t spread = 0;
    for (int32_t level = 0; level < 256; level++) spread += histogram[level] * (level % 7 + 1);
    int64_t finished = now_nanoseconds();
    printf("spread %lld bright %d weights %lld\n", (long long)spread, bright, (long long)weights);
    fprintf(stderr, "microseconds %lld\n", (long long)((finished - start) / 1000));
    fprintf(stderr, "phases make %lld filter %lld measure %lld\n", (long long)((made - start) / 1000),
        (long long)((filtered - made) / 1000), (long long)((finished - filtered) / 1000));
    return 0;
}
