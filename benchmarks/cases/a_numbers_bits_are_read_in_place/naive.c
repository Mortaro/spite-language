/* naive/ written in C the way it reads: the library's two conversions as two functions, the same steps in the same
 * order, and a float's bits read the way C programmers are taught to read them without breaking aliasing rules:
 * memcpy into a 4-byte variable and back. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../clock.h"

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float bits_as_float(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint16_t to_half_precision(float value) {
    uint32_t bit_pattern = float_bits(value);
    uint32_t sign_part = (bit_pattern >> 16) & 32768u;
    uint32_t magnitude = bit_pattern & 2147483647u;
    uint32_t half = 0;
    if (magnitude >= 1199570944u) {
        half = 31744;
        if (magnitude > 2139095040u) {
            half = 32256;
        }
    } else if (magnitude < 947912704u) {
        uint32_t magic = 1056964608u;
        float shifted = bits_as_float(magnitude) + bits_as_float(magic);
        half = float_bits(shifted) - magic;
    } else {
        uint32_t odd = (magnitude >> 13) & 1u;
        uint32_t rounded = magnitude + 3355447295u + odd;
        half = rounded >> 13;
    }
    return (uint16_t)(half | sign_part);
}

static float half_precision_to_float(uint16_t half) {
    uint32_t whole = half;
    uint32_t shifted_exponent = 260046848u;
    uint32_t bit_pattern = (whole & 32767u) << 13;
    uint32_t exponent = shifted_exponent & bit_pattern;
    bit_pattern = bit_pattern + 939524096u;
    if (exponent == shifted_exponent) {
        bit_pattern = bit_pattern + 939524096u;
    } else if (exponent == 0) {
        bit_pattern = bit_pattern + 8388608u;
        uint32_t magic = 947912704u;
        float value = bits_as_float(bit_pattern) - bits_as_float(magic);
        bit_pattern = float_bits(value);
    }
    uint32_t sign_part = (whole & 32768u) << 16;
    bit_pattern = bit_pattern | sign_part;
    return bits_as_float(bit_pattern);
}

static int64_t round_trips(int32_t count) {
    int64_t total = 0;
    for (int32_t index = 0; index < count; index = index + 1) {
        float value = index % 160000 - 20000;
        value = value * 0.5f;
        uint16_t half = to_half_precision(value);
        float back = half_precision_to_float(half);
        uint32_t back_bits = float_bits(back);
        total = total + half + back_bits;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int64_t total = round_trips(30000000);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
