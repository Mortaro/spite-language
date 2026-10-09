/* The same work tuned by hand: the three numbers held by value in registers, every step written out in the loop,
 * nothing allocated. Each step still adds in the order the program does, so the answer is the same to the bit. */
#include <stdint.h>
#include <stdio.h>
#include "../clock.h"

typedef struct Offset {
    float x;
    float y;
    float z;
} Offset;

static Offset simulate(int32_t steps) {
    Offset position = {0.0f, 0.0f, 0.0f};
    Offset velocity = {1.0f, 0.5f, 0.25f};
    const Offset gravity = {0.0f, -0.5f, 0.0f};
    const float pull = (float)0.000001;
    const float delta = (float)0.001;
    for (int32_t step = 0; step < steps; step++) {
        float pulled_x = gravity.x * pull;
        float pulled_y = gravity.y * pull;
        float pulled_z = gravity.z * pull;
        velocity.x = velocity.x + pulled_x;
        velocity.y = velocity.y + pulled_y;
        velocity.z = velocity.z + pulled_z;
        float moved_x = velocity.x * delta;
        float moved_y = velocity.y * delta;
        float moved_z = velocity.z * delta;
        position.x = position.x + moved_x;
        position.y = position.y + moved_y;
        position.z = position.z + moved_z;
    }
    return position;
}

int main(void) {
    int64_t start = now_nanoseconds();
    Offset ended = simulate(20000000);
    int64_t microseconds = microseconds_since(start);
    int64_t across = (int64_t)(ended.x * 1000.0f);
    int64_t up = (int64_t)(ended.y * 1000.0f);
    int64_t ahead = (int64_t)(ended.z * 1000.0f);
    printf("ended at %lld %lld %lld\n", (long long)across, (long long)up, (long long)ahead);
    print_microseconds(microseconds);
    return 0;
}
