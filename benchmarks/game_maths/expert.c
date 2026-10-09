/* The same work tuned by hand: every number a float local the C compiler keeps in registers, and the structure of
 * the matrices used. A turn about the y axis changes only the four corners of rows and columns 0 and 2, so each of
 * the 200 000 products is eight multiplications instead of sixty-four (the other terms are exact zeros and ones), and
 * the transformed point never changes, so it is worked out once and added up a million times. The arithmetic left is
 * the naive program's, in the same order, so the answers are the same. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "../clock.h"

int main(void) {
    int64_t start = now_nanoseconds();
    /* a million position + velocity.scaled(0.001) */
    float position_x = 0.0f, position_y = 0.0f, position_z = 0.0f;
    const float velocity_x = 1.0f, velocity_y = 0.5f, velocity_z = 0.25f;
    for (int32_t step = 0; step < 1000000; step++) {
        position_x = position_x + velocity_x * 0.001f;
        position_y = position_y + velocity_y * 0.001f;
        position_z = position_z + velocity_z * 0.001f;
    }
    /* the turn by 0.001 about (0, 1, 0) as a quaternion, then its matrix's four changing corners */
    float half_angle = 0.001f * 0.5f;
    float half_sine = sinf(half_angle);
    float turn_y = 1.0f * half_sine;
    float turn_w = cosf(half_angle);
    float step_00 = 1.0f - 2.0f * (turn_y * turn_y + 0.0f);
    float step_02 = 2.0f * (0.0f - turn_w * turn_y);
    float step_20 = 2.0f * (0.0f + turn_w * turn_y);
    float step_22 = 1.0f - 2.0f * (0.0f + turn_y * turn_y);
    /* accumulated = accumulated * step, 200 000 times, from the identity: m[column][row] */
    float m00 = 1.0f, m02 = 0.0f, m20 = 0.0f, m22 = 1.0f;
    for (int32_t step = 0; step < 200000; step++) {
        float next_00 = m00 * step_00 + m20 * step_02;
        float next_02 = m02 * step_00 + m22 * step_02;
        float next_20 = m00 * step_20 + m20 * step_22;
        float next_22 = m02 * step_20 + m22 * step_22;
        m00 = next_00;
        m02 = next_02;
        m20 = next_20;
        m22 = next_22;
    }
    float combined = m00 + 1.0f + m22;
    /* a million transform_point of (0.5, 0.5, 0.5) by a translation of (1, 2, 3): the same x every time */
    float moved_x = 1.0f * 0.5f + 0.0f * 0.5f + 0.0f * 0.5f + 1.0f;
    float transformed = 0.0f;
    for (int32_t step = 0; step < 1000000; step++) {
        transformed = transformed + moved_x;
    }
    int64_t microseconds = microseconds_since(start);
    printf("ended at %lld %lld %lld diagonal %lld total %lld\n", (long long)(int64_t)(position_x * 1000.0f),
           (long long)(int64_t)(position_y * 1000.0f), (long long)(int64_t)(position_z * 1000.0f),
           (long long)(int64_t)(combined * 1000000.0f), (long long)(int64_t)transformed);
    print_microseconds(microseconds);
    return 0;
}
