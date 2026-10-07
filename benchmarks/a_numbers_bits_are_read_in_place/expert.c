/* The same work tuned by hand: the processor's own half-precision instructions (F16C: vcvtps2ph and vcvtph2ps),
 * which round to nearest even, turn too large a value into infinity and keep the sign, as the library's two
 * functions do, eight values at a time; a float's bits read through a union. Every value is still converted
 * both ways. */
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include "../clock.h"

typedef union {
    float value;
    uint32_t bits;
} FloatBits;

__attribute__((target("avx2,f16c"))) static int64_t round_trips(int32_t count) {
    int64_t total = 0;
    int32_t index = 0;
    for (; index + 8 <= count; index += 8) {
        float values[8];
        for (int32_t lane = 0; lane < 8; lane++) {
            values[lane] = (float)((index + lane) % 160000 - 20000) * 0.5f;
        }
        __m128i halves = _mm256_cvtps_ph(_mm256_loadu_ps(values), _MM_FROUND_TO_NEAREST_INT);
        __m256 backs = _mm256_cvtph_ps(halves);
        uint16_t half_words[8];
        uint32_t back_words[8];
        _mm_storeu_si128((__m128i*)half_words, halves);
        _mm256_storeu_si256((__m256i*)back_words, _mm256_castps_si256(backs));
        for (int32_t lane = 0; lane < 8; lane++) {
            total += (int64_t)half_words[lane] + back_words[lane];
        }
    }
    for (; index < count; index++) {
        float value = (float)(index % 160000 - 20000) * 0.5f;
        uint16_t half = _cvtss_sh(value, _MM_FROUND_TO_NEAREST_INT);
        FloatBits back = {.value = _cvtsh_ss(half)};
        total += (int64_t)half + back.bits;
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
