/* naive/ written in C the way a C programmer writes it from the Spite: a struct of four ints per pixel, one malloc
 * each, a growable array of pointers, and the same passes: brighten every pixel, raise the contrast of every pixel,
 * a histogram of luminance, a count of the bright opaque pixels, the sum of each pixel's weight. --pixels=N sets how
 * many pixels. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

typedef struct Pixel {
    int32_t red;
    int32_t green;
    int32_t blue;
    int32_t alpha;
} Pixel;

typedef struct Pixels {
    Pixel** items;
    int32_t count;
    int32_t capacity;
} Pixels;

static void append(Pixels* pixels, Pixel* pixel) {
    if (pixels->count == pixels->capacity) {
        pixels->capacity = pixels->capacity ? pixels->capacity * 2 : 8;
        pixels->items = realloc(pixels->items, sizeof(Pixel*) * pixels->capacity);
    }
    pixels->items[pixels->count] = pixel;
    pixels->count = pixels->count + 1;
}

static Pixel* make_pixel(int64_t seed) {
    Pixel* pixel = malloc(sizeof(Pixel));
    pixel->red = (int32_t)(seed % 256);
    pixel->green = (int32_t)(seed / 256 % 256);
    pixel->blue = (int32_t)(seed / 65536 % 256);
    pixel->alpha = (int32_t)(seed / 16777216 * 2);
    return pixel;
}

static int32_t brightened(int32_t channel) {
    int32_t lifted = channel + 16;
    return lifted < 255 ? lifted : 255;
}

static int32_t contrasted(int32_t channel) {
    int32_t spread = (channel - 128) * 5 / 4 + 128;
    return spread < 0 ? 0 : spread > 255 ? 255 : spread;
}

static void brighten(Pixel* pixel) {
    pixel->red = brightened(pixel->red);
    pixel->green = brightened(pixel->green);
    pixel->blue = brightened(pixel->blue);
}

static void contrast(Pixel* pixel) {
    pixel->red = contrasted(pixel->red);
    pixel->green = contrasted(pixel->green);
    pixel->blue = contrasted(pixel->blue);
}

static int32_t luminance(Pixel* pixel) {
    return (pixel->red * 77 + pixel->green * 150 + pixel->blue * 29) / 256;
}

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
    Pixels pixels = {0};
    int64_t seed = 11;
    for (int32_t index = 0; index < count; index = index + 1) {
        seed = seed * 48271 % 2147483647;
        append(&pixels, make_pixel(seed));
    }
    int64_t made = now_nanoseconds();
    for (int32_t index = 0; index < pixels.count; index = index + 1) brighten(pixels.items[index]);
    for (int32_t index = 0; index < pixels.count; index = index + 1) contrast(pixels.items[index]);
    int64_t filtered = now_nanoseconds();
    int32_t histogram[256] = {0};
    for (int32_t index = 0; index < pixels.count; index = index + 1) {
        int32_t level = luminance(pixels.items[index]);
        histogram[level] = histogram[level] + 1;
    }
    int32_t bright = 0;
    for (int32_t index = 0; index < pixels.count; index = index + 1) {
        Pixel* pixel = pixels.items[index];
        if (pixel->alpha > 128 && luminance(pixel) > 100) bright = bright + 1;
    }
    int64_t weights = 0;
    for (int32_t index = 0; index < pixels.count; index = index + 1) {
        Pixel* pixel = pixels.items[index];
        weights = weights + pixel->red + pixel->green * 2 + pixel->blue * 3 + pixel->alpha * 5;
    }
    int64_t spread = 0;
    for (int32_t level = 0; level < 256; level = level + 1) spread = spread + histogram[level] * (level % 7 + 1);
    int64_t finished = now_nanoseconds();
    printf("spread %lld bright %d weights %lld\n", (long long)spread, bright, (long long)weights);
    print_microseconds((finished - start) / 1000);
    fprintf(stderr, "phases make %lld filter %lld measure %lld\n", (long long)((made - start) / 1000),
        (long long)((filtered - made) / 1000), (long long)((finished - filtered) / 1000));
    return 0;
}
