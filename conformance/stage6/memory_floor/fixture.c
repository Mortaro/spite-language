/* The foreign library conformance/stage6/memory_floor calls: check.sh builds it next to the program. */
#ifdef _WIN32
#define EXPORTED __declspec(dllexport)
#else
#define EXPORTED
#endif

EXPORTED long long measure(const char* text) { long long count = 0; while (text[count] != '\0') count = count + 1; return count; }
