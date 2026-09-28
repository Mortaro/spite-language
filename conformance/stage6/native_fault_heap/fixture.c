/* The foreign library conformance/stage6/native_fault_heap calls: check.sh builds it next to the program. */
#include <stdlib.h>
#ifdef _WIN32
#define EXPORTED __declspec(dllexport)
#else
#define EXPORTED
#endif

EXPORTED int free_inside(void) {
    char* block = (char*)malloc(64);
    free(block + 16);
    return 0;
}
