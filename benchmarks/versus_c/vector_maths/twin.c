/* The C twin of vector_maths.spite: the same steps on a Vector3 held by value, as a C programmer writes it. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>

typedef struct Vector3 {
    float x;
    float y;
    float z;
} Vector3;

static Vector3 vector3(float x, float y, float z) {
    Vector3 made = {x, y, z};
    return made;
}

static Vector3 sum(Vector3 left, Vector3 right) {
    return vector3(left.x + right.x, left.y + right.y, left.z + right.z);
}

static Vector3 scaled(Vector3 vector, float factor) {
    return vector3(vector.x * factor, vector.y * factor, vector.z * factor);
}

static float dot(Vector3 left, Vector3 right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

static Vector3 cross(Vector3 left, Vector3 right) {
    return vector3(left.y * right.z - left.z * right.y, left.z * right.x - left.x * right.z,
                   left.x * right.y - left.y * right.x);
}

static float length(Vector3 vector) {
    return sqrtf(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
}

static Vector3 normalized(Vector3 vector) {
    float squared = vector.x * vector.x + vector.y * vector.y + vector.z * vector.z;
    if (squared == 0.0) {
        return vector3(0.0, 0.0, 0.0);
    }
    float inverse_length = 1.0 / sqrtf(squared);
    return vector3(vector.x * inverse_length, vector.y * inverse_length, vector.z * inverse_length);
}

int main(void) {
    Vector3 position = vector3(0.0, 0.0, 0.0);
    Vector3 velocity = vector3(1.0, 0.5, 0.25);
    Vector3 axis = vector3(0.0, 1.0, 0.0);
    float total = 0.0;
    for (int32_t step = 0; step < 5000000; step = step + 1) {
        Vector3 moved = scaled(velocity, 0.001);
        position = sum(position, moved);
        Vector3 turned = cross(velocity, axis);
        Vector3 nudged = sum(velocity, scaled(turned, 0.0001));
        velocity = normalized(nudged);
        total = total + dot(position, velocity);
    }
    int64_t checksum = (int64_t)total;
    int64_t ended = (int64_t)(length(position) * 1000.0);
    printf("checksum %lld ended at %lld\n", (long long)checksum, (long long)ended);
    return 0;
}
