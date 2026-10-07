/* The same work tuned by hand: the vectors as six float locals the C compiler keeps in registers, and the cross
 * product with the constant axis (0, 1, 0) worked out by hand, (-z, 0, x), so a step is six multiplications fewer
 * and no call. The arithmetic left is the naive program's, in the same order, so the answer is the same. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "../clock.h"

int main(void) {
    int64_t start = now_nanoseconds();
    float position_x = 0.0f, position_y = 0.0f, position_z = 0.0f;
    float velocity_x = 1.0f, velocity_y = 0.5f, velocity_z = 0.25f;
    float total = 0.0f;
    for (int32_t step = 0; step < 5000000; step++) {
        position_x = position_x + (float)(velocity_x * 0.001f);
        position_y = position_y + (float)(velocity_y * 0.001f);
        position_z = position_z + (float)(velocity_z * 0.001f);
        /* velocity + cross(velocity, axis).scaled(0.0001), the cross being (-z, 0, x) */
        float nudged_x = velocity_x + (-velocity_z) * 0.0001f;
        float nudged_y = velocity_y;
        float nudged_z = velocity_z + velocity_x * 0.0001f;
        float squared = nudged_x * nudged_x + nudged_y * nudged_y + nudged_z * nudged_z;
        if (squared == 0.0f) {
            velocity_x = 0.0f;
            velocity_y = 0.0f;
            velocity_z = 0.0f;
        } else {
            float inverse_length = 1.0 / sqrtf(squared);
            velocity_x = nudged_x * inverse_length;
            velocity_y = nudged_y * inverse_length;
            velocity_z = nudged_z * inverse_length;
        }
        total = total + (position_x * velocity_x + position_y * velocity_y + position_z * velocity_z);
    }
    int64_t checksum = (int64_t)total;
    int64_t ended = (int64_t)(sqrtf(position_x * position_x + position_y * position_y + position_z * position_z) * 1000.0);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("checksum %lld ended at %lld\n", (long long)checksum, (long long)ended);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
