/* The foreign library conformance/stage6/foreign_callback_context_dropped calls back from: check.sh builds it next to the program. */
#ifdef _WIN32
#define EXPORTED __declspec(dllexport)
#else
#define EXPORTED
#endif

static void (*kept_tick)(int count, void* context) = 0;
static void* kept_context = 0;
EXPORTED void keep_tick(void (*tick)(int, void*), void* context) { kept_tick = tick; kept_context = context; }
EXPORTED void tick(int count) { kept_tick(count, kept_context); }
