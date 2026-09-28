/* The foreign library conformance/stage6/native_fault_foreign calls: check.sh builds it next to the program. */
#include <stdint.h>
#ifdef _WIN32
#define EXPORTED __declspec(dllexport)
#else
#define EXPORTED
#endif

EXPORTED int add_numbers(int left, int right) { return left + right; }
EXPORTED int read_integer_at(int64_t address) { return *(volatile int*)(intptr_t)address; }
