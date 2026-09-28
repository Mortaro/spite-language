/* The foreign library conformance/stage6/native_fault_illegal calls: check.sh builds it next to the program. */
#ifdef _WIN32
#define EXPORTED __declspec(dllexport)
#else
#define EXPORTED
#endif

EXPORTED int stop_here(void) { __builtin_trap(); return 0; }
