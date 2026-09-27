// game_maths.spite written by hand in C, with Vector3 and Matrix4 as plain structs passed and returned by value:
// the number "as fast as C" is measured against. Not Spite and not run by check.sh; build and run it by hand:
//   clang -O2 benchmarks/game_maths/game_maths.c -o game_maths_c.exe && ./game_maths_c.exe
// Each pass does what the Spite program does, with the same float arithmetic (a decimal literal is a double, as the
// Spite compiler writes it), so the answers printed are the same numbers.
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
static int64_t now_nanoseconds(void) {
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (int64_t)((double)counter.QuadPart * 1e9 / (double)frequency.QuadPart);
}
#else
static int64_t now_nanoseconds(void) {
    struct timespec spec;
    clock_gettime(CLOCK_MONOTONIC, &spec);
    return (int64_t)spec.tv_sec * 1000000000 + spec.tv_nsec;
}
#endif

typedef struct { float x, y, z; } Vector3;
typedef struct { float x, y, z, w; } Quaternion;
typedef struct { float m[4][4]; } Matrix4; // m[column][row]

static Vector3 vector3_sum(Vector3 left, Vector3 right) {
    Vector3 result = { left.x + right.x, left.y + right.y, left.z + right.z };
    return result;
}

static Vector3 vector3_scaled(Vector3 vector, float factor) {
    Vector3 result = { vector.x * factor, vector.y * factor, vector.z * factor };
    return result;
}

static Vector3 vector3_normalized(Vector3 vector) {
    float squared = vector.x * vector.x + vector.y * vector.y + vector.z * vector.z;
    if (squared == 0.0) {
        Vector3 zero = { 0.0, 0.0, 0.0 };
        return zero;
    }
    float inverse_length = 1.0 / sqrtf(squared);
    Vector3 result = { vector.x * inverse_length, vector.y * inverse_length, vector.z * inverse_length };
    return result;
}

static Matrix4 matrix4_identity(void) {
    Matrix4 result = { { { 1.0, 0.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0, 0.0 }, { 0.0, 0.0, 1.0, 0.0 }, { 0.0, 0.0, 0.0, 1.0 } } };
    return result;
}

static Matrix4 matrix4_multiply(Matrix4 self, Matrix4 other) {
    Matrix4 product;
    for (int column = 0; column < 4; column = column + 1) {
        for (int row = 0; row < 4; row = row + 1) {
            product.m[column][row] = self.m[0][row] * other.m[column][0] + self.m[1][row] * other.m[column][1] +
                                     self.m[2][row] * other.m[column][2] + self.m[3][row] * other.m[column][3];
        }
    }
    return product;
}

static Vector3 matrix4_transform_point(Matrix4 self, Vector3 point) {
    Vector3 result = {
        self.m[0][0] * point.x + self.m[1][0] * point.y + self.m[2][0] * point.z + self.m[3][0],
        self.m[0][1] * point.x + self.m[1][1] * point.y + self.m[2][1] * point.z + self.m[3][1],
        self.m[0][2] * point.x + self.m[1][2] * point.y + self.m[2][2] * point.z + self.m[3][2],
    };
    return result;
}

static Quaternion quaternion_axis_angle(Vector3 axis, float angle) {
    Vector3 unit = vector3_normalized(axis);
    float half_angle = angle * 0.5;
    float half_sine = sinf(half_angle);
    Quaternion result = { unit.x * half_sine, unit.y * half_sine, unit.z * half_sine, cosf(half_angle) };
    return result;
}

static Matrix4 quaternion_to_matrix(Quaternion rotation) {
    float x_x = rotation.x * rotation.x, y_y = rotation.y * rotation.y, z_z = rotation.z * rotation.z;
    float x_y = rotation.x * rotation.y, x_z = rotation.x * rotation.z, y_z = rotation.y * rotation.z;
    float w_x = rotation.w * rotation.x, w_y = rotation.w * rotation.y, w_z = rotation.w * rotation.z;
    Matrix4 result = matrix4_identity();
    result.m[0][0] = 1.0 - 2.0 * (y_y + z_z);
    result.m[0][1] = 2.0 * (x_y + w_z);
    result.m[0][2] = 2.0 * (x_z - w_y);
    result.m[1][0] = 2.0 * (x_y - w_z);
    result.m[1][1] = 1.0 - 2.0 * (x_x + z_z);
    result.m[1][2] = 2.0 * (y_z + w_x);
    result.m[2][0] = 2.0 * (x_z + w_y);
    result.m[2][1] = 2.0 * (y_z - w_x);
    result.m[2][2] = 1.0 - 2.0 * (x_x + y_y);
    return result;
}

static Vector3 vector_pass(void) {
    Vector3 position = { 0.0, 0.0, 0.0 };
    Vector3 velocity = { 1.0, 0.5, 0.25 };
    for (int step = 0; step < 1000000; step = step + 1) {
        Vector3 moved = vector3_scaled(velocity, 0.001);
        position = vector3_sum(position, moved);
    }
    return position;
}

static float matrix_pass(void) {
    Vector3 axis = { 0.0, 1.0, 0.0 };
    Matrix4 step_matrix = quaternion_to_matrix(quaternion_axis_angle(axis, 0.001));
    Matrix4 accumulated = matrix4_identity();
    for (int step = 0; step < 200000; step = step + 1) {
        accumulated = matrix4_multiply(accumulated, step_matrix);
    }
    return accumulated.m[0][0] + accumulated.m[1][1] + accumulated.m[2][2];
}

static float transform_pass(void) {
    Matrix4 model = matrix4_identity();
    model.m[3][0] = 1.0;
    model.m[3][1] = 2.0;
    model.m[3][2] = 3.0;
    Vector3 point = { 0.5, 0.5, 0.5 };
    float total = 0.0;
    for (int step = 0; step < 1000000; step = step + 1) {
        Vector3 moved = matrix4_transform_point(model, point);
        total = total + moved.x;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    Vector3 moved = vector_pass();
    int64_t vectors_end = now_nanoseconds();
    float combined = matrix_pass();
    int64_t matrices_end = now_nanoseconds();
    float transformed = transform_pass();
    int64_t transforms_end = now_nanoseconds();
    printf("1 000 000 Vector3 steps (a + b.scaled(t)) microseconds %lld ended at (%.8g, %.8g, %.8g)\n",
           (long long)((vectors_end - start) / 1000), moved.x, moved.y, moved.z);
    printf("200 000 Matrix4 products microseconds %lld diagonal %.8g\n", (long long)((matrices_end - vectors_end) / 1000),
           combined);
    printf("1 000 000 Matrix4 point transforms microseconds %lld total %.8g\n",
           (long long)((transforms_end - matrices_end) / 1000), transformed);
    return 0;
}
