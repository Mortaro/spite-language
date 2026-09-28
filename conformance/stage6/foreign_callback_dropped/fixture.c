/* The foreign library conformance/stage6/foreign_callback_dropped calls back from: check.sh builds it next to the program. */
#ifdef _WIN32
#define EXPORTED __declspec(dllexport)
#else
#define EXPORTED
#endif

static void (*kept_click)(int count) = 0;
EXPORTED void keep_click(void (*click)(int)) { kept_click = click; }
EXPORTED void click(int count) { kept_click(count); }
