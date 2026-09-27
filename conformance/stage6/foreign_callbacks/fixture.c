/* The foreign library conformance/stage6/foreign_callbacks calls back from: check.sh builds it next to the program. */
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
#define EXPORTED __declspec(dllexport)
#else
#include <pthread.h>
#define EXPORTED
#endif

/* Synchronous and with no user data, as qsort calls its comparison. */
EXPORTED int sum_mapped(int count, int (*each)(int value)) {
    int total = 0;
    for (int value = 1; value <= count; value = value + 1) total = total + each(value);
    return total;
}

/* Kept with no user data and called later, as Windows keeps a window procedure. */
static long long (*kept_procedure)(long long window, unsigned int message, long long parameter) = 0;
EXPORTED void keep_procedure(long long (*procedure)(long long, unsigned int, long long)) { kept_procedure = procedure; }
EXPORTED long long send_message(long long window, unsigned int message, long long parameter) {
    return kept_procedure(window, message, parameter);
}

/* User data last, as EnumWindows and Vulkan's debug messenger pass it. */
EXPORTED int visit_each(int count, int (*visit)(int index, double weight, void* context), void* context) {
    int visited = 0;
    for (int index = 0; index < count; index = index + 1) {
        if (visit(index, index * 0.5, context)) visited = visited + 1;
    }
    return visited;
}

/* User data first, called from a thread the library makes, as XAudio2 calls from its own thread. */
typedef struct {
    void (*report)(void* context, int value, int on_another_thread);
    void* context;
    int value;
} ThreadCall;
#ifdef _WIN32
static DWORD caller_thread = 0;
static DWORD WINAPI thread_main(LPVOID argument) {
    ThreadCall* call = (ThreadCall*)argument;
    for (int step = 0; step < 3; step = step + 1) call->report(call->context, call->value + step, GetCurrentThreadId() != caller_thread);
    return 0;
}
static void run_thread(ThreadCall* call) {
    caller_thread = GetCurrentThreadId();
    HANDLE thread = CreateThread(0, 0, thread_main, call, 0, 0);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
}
#else
static pthread_t caller_thread;
static void* thread_main(void* argument) {
    ThreadCall* call = (ThreadCall*)argument;
    for (int step = 0; step < 3; step = step + 1) call->report(call->context, call->value + step, !pthread_equal(pthread_self(), caller_thread));
    return 0;
}
static void run_thread(ThreadCall* call) {
    pthread_t thread;
    caller_thread = pthread_self();
    pthread_create(&thread, 0, thread_main, call);
    pthread_join(thread, 0);
}
#endif
EXPORTED void run_on_thread(void (*report)(void*, int, int), void* context, int value) {
    ThreadCall call = { report, context, value };
    run_thread(&call);
}

/* A COM-style interface: an object whose first word is a table of functions, each given the object first. */
typedef struct Listener Listener;
typedef struct {
    void (*started)(Listener* listener, int voice);
    void (*ended)(Listener* listener, int voice, long long buffer);
} ListenerTable;
struct Listener { const ListenerTable* table; };
typedef struct { Listener* listener; int voice; } Playing;
#ifdef _WIN32
static DWORD WINAPI play_main(LPVOID argument) {
#else
static void* play_main(void* argument) {
#endif
    Playing* playing = (Playing*)argument;
    playing->listener->table->started(playing->listener, playing->voice);
    playing->listener->table->ended(playing->listener, playing->voice, playing->voice * 100);
    return 0;
}
EXPORTED void play_voice(Listener* listener, int voice) {
    Playing playing = { listener, voice };
#ifdef _WIN32
    HANDLE thread = CreateThread(0, 0, play_main, &playing, 0, 0);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    pthread_t thread;
    pthread_create(&thread, 0, play_main, &playing);
    pthread_join(thread, 0);
#endif
}
