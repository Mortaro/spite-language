/* The foreign library conformance/stage6/foreign_library calls: check.sh builds it next to the program. */
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
