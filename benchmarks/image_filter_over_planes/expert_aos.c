/* The image as a C programmer who knows images stores it: one array of interleaved bytes, red, green, blue and
 * alpha side by side (four bytes a pixel, the array of structures packed to the channel's size), with the same
 * passes as naive.c. Each filter walks every byte and keeps the alpha byte as it was with a select, so the C compiler
 * vectorises it sixteen bytes at a time. --pixels=N sets how many pixels. */
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

int main(int argument_count, char** arguments) {
    int32_t count = setting_of(argument_count, arguments, "--pixels=", 4194304);
    int64_t start = now_nanoseconds();
    uint8_t* restrict image = malloc((size_t)count * 4);
    int64_t seed = 11;
    for (int32_t index = 0; index < count; index++) {
        seed = seed * 48271 % 2147483647;
        image[index * 4] = (uint8_t)(seed % 256);
        image[index * 4 + 1] = (uint8_t)(seed / 256 % 256);
        image[index * 4 + 2] = (uint8_t)(seed / 65536 % 256);
        image[index * 4 + 3] = (uint8_t)(seed / 16777216 * 2);
    }
    int64_t made = now_nanoseconds();
    int32_t bytes = count * 4;
    for (int32_t index = 0; index < bytes; index++) {
        int32_t channel = image[index];
        int32_t lifted = channel + 16 < 255 ? channel + 16 : 255;
        image[index] = (uint8_t)((index & 3) == 3 ? channel : lifted);
    }
    for (int32_t index = 0; index < bytes; index++) {
        int32_t channel = image[index];
        int32_t spread = (channel - 128) * 5 / 4 + 128;
        spread = spread < 0 ? 0 : spread > 255 ? 255 : spread;
        image[index] = (uint8_t)((index & 3) == 3 ? channel : spread);
    }
    int64_t filtered = now_nanoseconds();
    int32_t histogram[256] = {0};
    for (int32_t index = 0; index < count; index++) {
        const uint8_t* pixel = &image[index * 4];
        histogram[(pixel[0] * 77 + pixel[1] * 150 + pixel[2] * 29) / 256]++;
    }
    int32_t bright = 0;
    for (int32_t index = 0; index < count; index++) {
        const uint8_t* pixel = &image[index * 4];
        bright += (pixel[3] > 128) & ((pixel[0] * 77 + pixel[1] * 150 + pixel[2] * 29) / 256 > 100);
    }
    int64_t weights = 0;
    for (int32_t index = 0; index < count; index++) {
        const uint8_t* pixel = &image[index * 4];
        weights += pixel[0] + pixel[1] * 2 + pixel[2] * 3 + pixel[3] * 5;
    }
    int64_t spread = 0;
    for (int32_t level = 0; level < 256; level++) spread += histogram[level] * (level % 7 + 1);
    int64_t finished = now_nanoseconds();
    printf("spread %lld bright %d weights %lld\n", (long long)spread, bright, (long long)weights);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld filter %lld measure %lld\n", (long long)((made - start) / 1000),
        (long long)((filtered - made) / 1000), (long long)((finished - filtered) / 1000));
    return 0;
}
