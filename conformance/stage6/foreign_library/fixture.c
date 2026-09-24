/* The foreign library conformance/stage6/foreign_library calls: check.sh builds it next to the program. */
#include <stdint.h>
#ifdef _WIN32
#define EXPORTED __declspec(dllexport)
#else
#define EXPORTED
#endif

EXPORTED int add_numbers(int left, int right) { return left + right; }
EXPORTED int multiplyNumbers(int left, int right) { return left * right; }
EXPORTED int GetAnswerPos(void) { return 42; }
EXPORTED double half_of(int value) { return value / 2.0; }
EXPORTED long long big_number(void) { return 5000000000LL; }
EXPORTED const char* greeting(void) { return "hello from C"; }
EXPORTED int text_length(const char* text) { int count = 0; while (text[count] != '\0') count = count + 1; return count; }
typedef struct { int x; int y; } Point;
EXPORTED int sum_point(Point* point) { return point->x + point->y; }
EXPORTED void MovePoint(Point* point) { point->x = point->x + 10; point->y = point->y + 20; }
EXPORTED int sum_values(int* values, int count) { int total = 0; for (int index = 0; index < count; index = index + 1) total = total + values[index]; return total; }
EXPORTED void double_values(int* values, int count) { for (int index = 0; index < count; index = index + 1) values[index] = values[index] * 2; }
EXPORTED long long wide_sum(long long left, long long right) { return left + right; }
EXPORTED long long WideSum(long long left, unsigned long long right) { return left + (long long)right; }
EXPORTED int Scaled(float factor, int value) { return (int)(factor * (float)value); }
EXPORTED long long HandleTwice(void* handle) { return 2 * (long long)(intptr_t)handle; }
EXPORTED void Forget(int value) { (void)value; }
