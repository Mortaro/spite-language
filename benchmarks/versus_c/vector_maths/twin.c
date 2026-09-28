/* The C twin of vector_maths.spite: the same steps on a Vector3 held by value, as a C programmer writes it. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>

/* The same clock the Spite program reads: the time of the work goes to the error output and the answer to the
 * standard output, so run.sh compares the answers and times the work without the process's start. */
#ifdef _WIN32
#include <windows.h>
static int64_t now_nanoseconds(void) {
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    return (int64_t)((double)counter.QuadPart * 1000000000.0 / (double)frequency.QuadPart);
}
#else
#include <time.h>
static int64_t now_nanoseconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)now.tv_sec * 1000000000 + now.tv_nsec;
}
#endif

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
    int64_t start = now_nanoseconds();
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
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("checksum %lld ended at %lld\n", (long long)checksum, (long long)ended);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
