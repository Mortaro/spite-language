/* All of the C the compiler writes from naive/ for this case, as an --optimized build for Windows does, with
 * the compiler's own numbered names numbered again from 1. Written by scripts/cases/extract.sh; check.sh
 * compares it with what the compiler writes now. */

#define SPITE_THREADS
/* The floor: the only C the Spite compiler writes by hand. Everything above it
 * is Spite from library/, and the tree shaker drops what a program never calls. */

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <fcntl.h>
#include <io.h>
#include <direct.h>
#include <windows.h>
#include <malloc.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <dlfcn.h>
#include <signal.h>
#include <sys/ucontext.h>
#endif

/* A program that starts a thread counts references atomically, and the compiler defines SPITE_THREADS for it;
 * every other program counts them with plain arithmetic, and so does a class no thread can reach
 * (docs/optimizations.md#plain-reference-counts-where-no-thread-reaches-a-class). */
#define SPITE_PLAIN_COUNT_UP(count) ((count) = (count) + 1)
#define SPITE_PLAIN_COUNT_DOWN(count) ((count) = (count) - 1)
#ifdef SPITE_THREADS
#define SPITE_COUNT_UP(count) __atomic_add_fetch(&(count), 1, __ATOMIC_RELAXED)
#define SPITE_COUNT_DOWN(count) __atomic_sub_fetch(&(count), 1, __ATOMIC_ACQ_REL)
#define SPITE_THREAD_LOCAL _Thread_local
#define SPITE_SINGLETON_FOUND(cache) __atomic_load_n(&(cache), __ATOMIC_ACQUIRE)
#define SPITE_SINGLETON_PUBLISH(cache, made) __atomic_store_n(&(cache), (made), __ATOMIC_RELEASE)
#define SPITE_LOCK(lock) while (__atomic_exchange_n(&(lock), 1, __ATOMIC_ACQUIRE) != 0) {}
#define SPITE_UNLOCK(lock) __atomic_store_n(&(lock), 0, __ATOMIC_RELEASE)
static void spite_enter_skipped(void* guard);
/* Each thread holds what one Console call writes and hands it over in one fwrite, so a line is never split by
 * another thread's output (docs/standard_library.md#console). */
typedef struct SpiteHeldText { char* bytes; size_t used; size_t room; char first[256]; } SpiteHeldText;
static SPITE_THREAD_LOCAL SpiteHeldText spite_held_output;
static SPITE_THREAD_LOCAL SpiteHeldText spite_held_error;
static inline void spite_hold_text(SpiteHeldText* held, const char* bytes, size_t count) {
if (held->bytes == 0) { held->bytes = held->first; held->room = sizeof(held->first); }
if (held->used + count > held->room) {
size_t room = (held->used + count) * 2;
char* grown = held->bytes == held->first ? (char*)malloc(room) : (char*)realloc(held->bytes, room);
if (grown == 0) { fputs("spite: out of memory holding printed text\n", stderr); exit(1); }
if (held->bytes == held->first) memcpy(grown, held->first, held->used);
held->bytes = grown; held->room = room;
}
memcpy(held->bytes + held->used, bytes, count);
held->used = held->used + count;
}
static inline void spite_write_held(SpiteHeldText* held, FILE* stream) {
if (held->used > 0) fwrite(held->bytes, 1, held->used, stream);
held->used = 0;
if (held->bytes != held->first) { free(held->bytes); held->bytes = held->first; held->room = sizeof(held->first); }
}
#else
#define SPITE_COUNT_UP(count) ((count) = (count) + 1)
#define SPITE_COUNT_DOWN(count) ((count) = (count) - 1)
#define SPITE_THREAD_LOCAL
#define SPITE_SINGLETON_FOUND(cache) (cache)
#define SPITE_SINGLETON_PUBLISH(cache, made) ((cache) = (made))
#define SPITE_LOCK(lock) (void)(lock)
#define SPITE_UNLOCK(lock) (void)(lock)
#endif

/* A native fault is reported, never silent (D244, docs/failure.md): main installs the handler first, each thread
 * the program starts gets room to report its own stack overflow, and each foreign call names itself here. */
static void spite_fault_install(void);
static void spite_fault_thread(void);
static SPITE_THREAD_LOCAL const char* spite_last_foreign_call = 0;

/* ---- allocation ---- */

/* Every Spite object is built on these. A library --hot-reload compiles for a running program
 * (SPITE_RELOADED) allocates through that program, which it is handed when it is loaded; otherwise the
 * compiler writes what this build needs in place of the comment below (docs/memory.md). */
#ifdef SPITE_RELOADED
static void* (*spite_host_realloc)(void* pointer, size_t size) = 0;
static void (*spite_host_free)(void* pointer) = 0;
#define SPITE_REALLOC(pointer, size) spite_host_realloc(pointer, size)
#define SPITE_FREE(pointer) spite_host_free(pointer)
#define SPITE_MALLOC(size) spite_host_realloc(0, size)
#else
#define SPITE_REALLOC(pointer, size) realloc(pointer, size)
#define SPITE_FREE(pointer) free(pointer)
#define SPITE_MALLOC(size) malloc(size)
#endif

/* ---- object model: reference counting (milestone 9a) ----
 *
 * D1 (Mortaro, 2026-09-19): reference counting is the default memory model.
 * Every non-scalar value (class instance, List<T>, Dictionary<T>, String,
 * an object literal) is a heap object whose first field is a `SpiteHeader`;
 * variables/attributes/elements/parameters hold pointers to these objects.
 * Passing, assigning, storing and returning share the same object: a copy
 * is only ever made by an explicit `copy()`/`deep_copy()` call
 * (docs/memory.md).
 *
 * Each concrete type gets its own small, readable, generated
 * `{Type}___retain`/`{Type}___release` pair; this
 * header only holds what is shared: the header layout itself. */

typedef struct SpiteHeader {
    int32_t ref_count;
    int32_t class_id;
} SpiteHeader;

/* A value of a type that holds no attributes (Anything, Printable): an object with its class id as the tag, or
 * a number, Boolean or enum value held in place with its class's id, never a box (docs/optimizations.md). */
typedef struct SpiteTagged {
    int32_t tag;
    int32_t plain;
    union { void* object; uint64_t bits; } value;
} SpiteTagged;
#define SPITE_TAGGED_NULL ((SpiteTagged){ 0, 0, { 0 } })
#define SPITE_TAGGED_PRESENT(tagged) ((tagged).plain != 0 || (tagged).value.object != 0)
#define SPITE_TAGGED_VALUE(tagged, type) ({ SpiteTagged spite_tagged_read = (tagged); type spite_tagged_value; memcpy(&spite_tagged_value, &spite_tagged_read.value, sizeof(type)); spite_tagged_value; })
static inline SpiteTagged spite_tagged_object(int32_t tag, void* object) { SpiteTagged tagged; tagged.tag = object == 0 ? 0 : tag; tagged.plain = 0; tagged.value.object = object; return tagged; }
static inline SpiteTagged spite_tagged_from_pointer(void* object) { SpiteTagged tagged; tagged.plain = 0; tagged.value.object = object; tagged.tag = object == 0 ? 0 : ((SpiteHeader*)object)->class_id; if (tagged.tag < 0) tagged.tag = 0; return tagged; }

/* String's layout is written after the enums, from the attributes library/string.spite declares. */
typedef struct SpiteString SpiteString;
#define SPITE_COLD_PATH __attribute__((noinline))
#define SPITE_CRASH_PATH __attribute__((cold))
#define SPITE_CRASH_REPORT __attribute__((noinline, cold, noreturn))
static const char* spite_assert_trace[32];
static uint64_t spite_assert_runs[32];
static int64_t spite_assert_total = 0;
static const char* spite_assert_newest = 0;
static uint64_t spite_assert_newest_count = 0;
static inline uint64_t spite_assert_run(const char* spite_site, int64_t spite_entry) { return ((uint64_t)(uintptr_t)spite_site << 32) | (((uint64_t)spite_entry & 0xffu) << 24) | 1u; }
#ifdef SPITE_THREADS
static SPITE_COLD_PATH void spite_ring_assert(const char* spite_site) {
for (int32_t spite_waited = 0; spite_waited < 1000;) {
int64_t spite_newest = __atomic_load_n(&spite_assert_total, __ATOMIC_ACQUIRE) - 1;
if (spite_newest < 0) break;
uint64_t spite_run = __atomic_load_n(&spite_assert_runs[spite_newest % 32], __ATOMIC_ACQUIRE);
if (((spite_run >> 24) & 0xffu) != ((uint64_t)spite_newest & 0xffu)) { spite_waited = spite_waited + 1; continue; }
if ((uint32_t)(spite_run >> 32) != (uint32_t)(uintptr_t)spite_site || (spite_run & 0xffffffu) == 0xffffffu) break;
if (__atomic_compare_exchange_n(&spite_assert_runs[spite_newest % 32], &spite_run, spite_run + 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) return;
}
int64_t spite_taken = __atomic_fetch_add(&spite_assert_total, 1, __ATOMIC_ACQ_REL);
__atomic_store_n(&spite_assert_trace[spite_taken % 32], spite_site, __ATOMIC_RELAXED);
__atomic_store_n(&spite_assert_runs[spite_taken % 32], spite_assert_run(spite_site, spite_taken), __ATOMIC_RELEASE);
}
static uint64_t spite_assert_count(int64_t spite_index) { return __atomic_load_n(&spite_assert_runs[spite_index % 32], __ATOMIC_ACQUIRE) & 0xffffffu; }
#else
static SPITE_COLD_PATH void spite_ring_assert(const char* spite_site) {
if (spite_site == spite_assert_newest) { spite_assert_newest_count = spite_assert_newest_count + 1; return; }
if (spite_assert_total > 0) spite_assert_runs[(spite_assert_total - 1) % 32] = spite_assert_newest_count;
spite_assert_trace[spite_assert_total % 32] = spite_site;
spite_assert_total = spite_assert_total + 1;
spite_assert_newest = spite_site;
spite_assert_newest_count = 1;
}
static uint64_t spite_assert_count(int64_t spite_index) { return spite_index == spite_assert_total - 1 ? spite_assert_newest_count : spite_assert_runs[spite_index % 32]; }
#endif
#define SPITE_RING_ASSERT(site) spite_ring_assert(site)
static int32_t spite_crashing = 0;
static void (*spite_crash_chain)(void) = 0;
static inline void spite_crash_text(const char* spite_bytes, int64_t spite_length) {
int64_t spite_kept = spite_length > 80 ? 80 : spite_length;
while (spite_kept > 0 && spite_kept < spite_length && (spite_bytes[spite_kept] & 0xC0) == 0x80) spite_kept = spite_kept - 1;
fwrite(spite_bytes, 1, (size_t)spite_kept, stderr);
if (spite_kept < spite_length) fprintf(stderr, "...(%lld bytes)", (long long)spite_length);
}
static SPITE_CRASH_PATH void spite_crash_begin(void) {
if (__atomic_exchange_n(&spite_crashing, 1, __ATOMIC_ACQ_REL) != 0) { for (;;) { } }
}
static void spite_report_assert_trace(void) {
int64_t spite_total = __atomic_load_n(&spite_assert_total, __ATOMIC_ACQUIRE);
int64_t spite_kept = spite_total < 32 ? spite_total : 32;
for (int64_t spite_index = spite_total - spite_kept; spite_index < spite_total; spite_index = spite_index + 1) {
const char* spite_site = spite_assert_trace[spite_index % 32];
if (spite_site == 0) continue;
uint64_t spite_repeats = spite_assert_count(spite_index);
while (spite_index + 1 < spite_total && spite_assert_trace[(spite_index + 1) % 32] == spite_site) { spite_index = spite_index + 1; spite_repeats = spite_repeats + spite_assert_count(spite_index); }
if (spite_repeats > 1) { fwrite(spite_site, 1, strlen(spite_site) - 1, stderr); fprintf(stderr, "\trepeated=%llu\n", (unsigned long long)spite_repeats); } else { fputs(spite_site, stderr); }
}
if (spite_total > 32) { fprintf(stderr, "spite.assert\tearlier=%lld\n", (long long)(spite_total - 32)); }
if (spite_crash_chain != 0) spite_crash_chain();
}
#define SPITE_TRACE_ASSERT(site) (SPITE_RING_ASSERT(site))
typedef enum {
Console_Stream_output,
Console_Stream_error,
} Console_Stream;
typedef enum {
Date_Weekday_monday,
Date_Weekday_tuesday,
Date_Weekday_wednesday,
Date_Weekday_thursday,
Date_Weekday_friday,
Date_Weekday_saturday,
Date_Weekday_sunday,
} Date_Weekday;
typedef enum {
DaylightChange_Kind_day_of_year,
DaylightChange_Kind_day_of_year_without_leap_day,
DaylightChange_Kind_weekday_of_month,
} DaylightChange_Kind;
typedef enum {
Duration_Unit_nanoseconds,
Duration_Unit_microseconds,
Duration_Unit_milliseconds,
Duration_Unit_seconds,
Duration_Unit_minutes,
Duration_Unit_hours,
} Duration_Unit;
typedef enum {
Period_Unit_days,
Period_Unit_weeks,
Period_Unit_months,
Period_Unit_years,
} Period_Unit;
typedef enum {
Quaternion_RotationOrder_xyz,
Quaternion_RotationOrder_xzy,
Quaternion_RotationOrder_yxz,
Quaternion_RotationOrder_yzx,
Quaternion_RotationOrder_zxy,
Quaternion_RotationOrder_zyx,
} Quaternion_RotationOrder;
typedef enum {
TimeZone_Ambiguity_compatible,
TimeZone_Ambiguity_earlier,
TimeZone_Ambiguity_later,
} TimeZone_Ambiguity;
typedef enum {
Spite_Memory_Section_heap,
Spite_Memory_Section_stack,
Spite_Memory_Section_constant,
} Spite_Memory_Section;
struct SpiteString {
int64_t _bytes_;
int64_t _length_;
};
typedef struct SpiteStringBlock { SpiteHeader header; int64_t capacity; char bytes[]; } SpiteStringBlock;
typedef struct SpiteBox_SpiteString { SpiteHeader header; SpiteString value; } SpiteBox_SpiteString;
_Static_assert(sizeof(SpiteString) == 16 && sizeof(SpiteStringBlock) == 16, "a String is 16 bytes and its block's characters start 16 bytes in");
#define SPITE_STRING_INLINE 15
#define SPITE_STRING_HEAP ((int64_t)0x4000000000000000)
#define SPITE_STRING_FORM(text) ((uint64_t)(text)._length_ >> 56)
#define SPITE_STRING_IS_INLINE(text) (SPITE_STRING_FORM(text) <= SPITE_STRING_INLINE)
#define SPITE_STRING_IS_HEAP(text) (SPITE_STRING_FORM(text) == 0x40)
#define SPITE_STRING_IS_CONSTANT(text) (SPITE_STRING_FORM(text) == 0x80)
#define SPITE_STRING_BLOCK(text) ((SpiteStringBlock*)(intptr_t)((text)._bytes_ - 16))
#define SPITE_STATIC_STRING(text, length) { (int64_t)(intptr_t)(text), (int64_t)(length) | INT64_MIN }
#define SPITE_STRING_NULL ((SpiteString){ 0, INT64_MIN })
#define SPITE_STRING_EMPTY ((SpiteString){ 0, (int64_t)SPITE_STRING_INLINE << 56 })
#define SPITE_STRING_IS_NULL(text) ((text)._bytes_ == 0 && (text)._length_ == INT64_MIN)
static inline int64_t spite_string_length(SpiteString text) {
uint64_t form = (uint64_t)text._length_ >> 56;
if (form <= SPITE_STRING_INLINE) return (int64_t)(SPITE_STRING_INLINE - form);
return (int64_t)((uint64_t)text._length_ & 0x00FFFFFFFFFFFFFF);
}
static inline const char* spite_string_bytes(const SpiteString* text) {
if (SPITE_STRING_IS_INLINE(*text)) return (const char*)text;
return (const char*)(intptr_t)text->_bytes_;
}
static inline int32_t spite_string_code_at(const SpiteString* text, int32_t index) {
const char* bytes = spite_string_bytes(text);
int32_t length = (int32_t)spite_string_length(*text);
if (index < 0 || index > length) index = length;
return (int32_t)(uint8_t)bytes[index];
}
typedef struct Launcher Launcher;
typedef struct Build Build;
typedef struct Clock Clock;
typedef struct Console Console;
typedef struct Dictionary Dictionary;
typedef struct DynamicLibrary DynamicLibrary;
typedef struct File File;
typedef struct List List;
typedef struct Lock Lock;
typedef struct Scheduler Scheduler;
typedef struct SchedulerLoop SchedulerLoop;
typedef struct Socket Socket;
typedef struct ThreadSlot ThreadSlot;
typedef struct Memory_Arena Memory_Arena;
typedef struct Memory_Heap Memory_Heap;
typedef struct Spite_Argument Spite_Argument;
typedef struct Spite_AttributeDeclaration Spite_AttributeDeclaration;
typedef struct Spite_Class Spite_Class;
typedef struct Spite_Function Spite_Function;
typedef struct Spite_Namespace Spite_Namespace;
typedef struct Naive Naive;
typedef struct List_String List_String;
typedef struct TypedMemory__String TypedMemory__String;
typedef struct {
    int64_t count;
    const char** items;
} SpiteArguments;

static SpiteArguments spite_program_arguments;
typedef SpiteTagged Console_Printable;
typedef SpiteTagged Console_Debuggable;
typedef void* Directory_Entry;
typedef void* FileSystemWatcher_Target;
typedef SpiteTagged Nothing_Anything;
typedef SpiteTagged Number_Number;
typedef void* ReadEvaluatePrintLoop_Answer;
typedef void* WebSocket_Message;
typedef void* Spite_Access_Target;
static Console* spite_singleton_Console_cache = 0;
static bool spite_singleton_Console_destroyed = false;
static int32_t spite_singleton_Console_lock = 0;
typedef struct List_Long List_Long;
typedef struct TypedMemory__Long TypedMemory__Long;
static DynamicLibrary* spite_foreign_library_1_cache = 0;
static bool spite_foreign_library_1_tracked = false;
static DynamicLibrary* spite_foreign_library_2_cache = 0;
static bool spite_foreign_library_2_tracked = false;
static SpiteString spite_lit_1 = SPITE_STATIC_STRING("", 0);
static SpiteString spite_lit_2 = SPITE_STATIC_STRING("", 0);
static Clock* spite_singleton_Clock_cache = 0;
static bool spite_singleton_Clock_destroyed = false;
static int32_t spite_singleton_Clock_lock = 0;
typedef struct List_Memory_Address List_Memory_Address;
typedef struct TypedMemory__Memory_Address TypedMemory__Memory_Address;
static Scheduler* spite_singleton_Scheduler_cache = 0;
static bool spite_singleton_Scheduler_destroyed = false;
static int32_t spite_singleton_Scheduler_lock = 0;
typedef struct ThreadLocal__SchedulerLoop ThreadLocal__SchedulerLoop;
typedef struct TypedMemory__SchedulerLoop TypedMemory__SchedulerLoop;
typedef struct List_SchedulerLoop List_SchedulerLoop;
static SpiteString spite_symbol_1 = { (int64_t)0x0064656d616e6e75ULL, (int64_t)0x0800000000000000ULL };
static SpiteString spite_symbol_2 = { (int64_t)0x00676e6968746f4eULL, (int64_t)0x0800000000000000ULL };
static SpiteString spite_lit_3 = SPITE_STATIC_STRING("", 0);
typedef struct List_Spite_AttributeDeclaration List_Spite_AttributeDeclaration;
typedef struct TypedMemory__Spite_AttributeDeclaration TypedMemory__Spite_AttributeDeclaration;
typedef struct List_Spite_Function List_Spite_Function;
typedef struct TypedMemory__Spite_Function TypedMemory__Spite_Function;
typedef struct List_Spite_Argument List_Spite_Argument;
typedef struct TypedMemory__Spite_Argument TypedMemory__Spite_Argument;
static SpiteString spite_lit_4 = SPITE_STATIC_STRING("", 0);
static SpiteString spite_lit_5 = SPITE_STATIC_STRING("", 0);
typedef struct List_Spite_Class List_Spite_Class;
typedef struct TypedMemory__Spite_Class TypedMemory__Spite_Class;
typedef struct List_Spite_Namespace List_Spite_Namespace;
typedef struct TypedMemory__Spite_Namespace TypedMemory__Spite_Namespace;
typedef void* Spite_Allocator;
struct Launcher {
SpiteHeader header;
Build* build_;
};
typedef struct List_Console_Printable List_Console_Printable;
typedef struct TypedMemory__Console_Printable TypedMemory__Console_Printable;
typedef struct SpiteBox_SpiteLong { SpiteHeader header; int64_t value; } SpiteBox_SpiteLong;
#define SPITE_FRAMED_COUNT 1073741824
static SpiteString spite_lit_6 = SPITE_STATIC_STRING("true", 4);
static SpiteString spite_lit_7 = SPITE_STATIC_STRING("false", 5);
struct Build {
SpiteHeader header;
bool check_;
bool build_;
SpiteString executable_path_;
bool c_source_;
SpiteString c_path_;
SpiteString final_classes_;
SpiteString optimization_report_;
bool optimized_;
bool development_;
bool repl_;
int32_t repl_port_;
bool hot_reload_;
bool debug_memory_;
bool trace_asserts_;
SpiteString operating_system_;
SpiteString target_operating_system_;
SpiteString program_;
};
#define SpiteByte_to_integer(self) ((int32_t)(self))
#define SpiteByte_to_long(self) ((int64_t)(self))
struct Clock {
SpiteHeader header;
Memory_Heap* heap_;
DynamicLibrary* kernel_;
int64_t _ticks_per_second_;
};
static void* spite_foreign_1_0 = 0;
static void* spite_foreign_1_1 = 0;
#define spite_site_1() "library/windows/clock.spite:16 in Clock.elapsed_nanoseconds"
struct Console {
SpiteHeader header;
Memory_Heap* heap_;
DynamicLibrary* library_;
int64_t input_;
};
static SpiteString spite_lit_8 = SPITE_STATIC_STRING("\n", 1);
static SpiteString spite_lit_9 = SPITE_STATIC_STRING("\n", 1);
static SpiteString spite_lit_10 = SPITE_STATIC_STRING(" ", 1);
#define spite_site_2() "library/console.spite:56 in Console._write_values"

struct DynamicLibrary {
SpiteHeader header;
SpiteString file_name_;
int64_t handle_;
};
struct File {
SpiteHeader header;
SpiteString path_;
Memory_Heap* heap_;
DynamicLibrary* library_;
DynamicLibrary* kernel_;
};
typedef struct { bool has_value; int64_t value; } Nullable_Long;
static SpiteString spite_lit_11 = SPITE_STATIC_STRING("rb", 2);
#define spite_site_3() "library/file.spite:39 in File.read"
static SpiteString spite_lit_12 = SPITE_STATIC_STRING("wb", 2);
static void* spite_foreign_2_11 = 0;
static void* spite_foreign_2_12 = 0;
static void* spite_foreign_2_13 = 0;
static void* spite_foreign_2_14 = 0;
static void* spite_foreign_2_15 = 0;
static void* spite_foreign_2_16 = 0;
static void* spite_foreign_2_18 = 0;
static void* spite_foreign_1_22 = 0;
static void* spite_foreign_1_24 = 0;
#define SpiteFloat_to_long(self) ((int64_t)(self))
#define SpiteFloat_to_double(self) ((double)(self))
static int64_t spite_described_owner = 0;
static int32_t spite_described_depth = 0;
static SPITE_THREAD_LOCAL char spite_described_thread;
static void spite_described_enter(void) {
#ifdef SPITE_THREADS
int64_t spite_me = (int64_t)(intptr_t)&spite_described_thread;
if (__atomic_load_n(&spite_described_owner, __ATOMIC_ACQUIRE) != spite_me) {
int64_t spite_free = 0;
while (!__atomic_compare_exchange_n(&spite_described_owner, &spite_free, spite_me, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) { spite_free = 0; }
}
#endif
spite_described_depth = spite_described_depth + 1;
}
static void spite_described_leave(bool* ready) {
spite_described_depth = spite_described_depth - 1;
if (spite_described_depth > 0) return;
#ifdef SPITE_THREADS
__atomic_store_n(ready, true, __ATOMIC_RELEASE);
__atomic_store_n(&spite_described_owner, 0, __ATOMIC_RELEASE);
#else
*ready = true;
#endif
}
#define SpiteInteger_to_long(self) ((int64_t)(self))
#define SpiteInteger_to_unsigned_long(self) ((uint64_t)(self))
#define SpiteInteger_to_unsigned_integer(self) ((uint32_t)(self))
#define SpiteInteger_to_byte(self) ((uint8_t)(self))
#define SpiteInteger_to_float(self) ((float)(self))
struct Lock {
SpiteHeader header;
Memory_Heap* _heap_;
int64_t _handle_;
DynamicLibrary* _kernel_;
};
static void* spite_foreign_1_34 = 0;
static void* spite_foreign_1_35 = 0;
static void* spite_foreign_1_36 = 0;
static SpiteString spite_lit_13 = SPITE_STATIC_STRING("0", 1);
#define spite_site_4() "library/long.spite:17 in Long.to_string"
#define spite_site_5() "library/long.spite:18 in Long.to_string"
#define spite_site_6() "library/long.spite:22 in Long.to_string"
#define spite_site_7() "library/long.spite:26 in Long.to_string"
#define SpiteLong_to_unsigned_integer(self) ((uint32_t)(self))
#define SpiteLong_to_unsigned_long(self) ((uint64_t)(self))
static void* spite_foreign_1_45 = 0;
static void* spite_foreign_1_46 = 0;
struct Scheduler {
SpiteHeader header;
bool started_;
int64_t scheduler_thread_;
bool serving_;
int32_t waits_begun_;
int32_t check_points_passed_;
bool answering_in_wait_;
Spite_Function* commands_;
Spite_Function* reloads_;
bool paused_;
ThreadLocal__SchedulerLoop* _loops_;
List_SchedulerLoop* _every_loop_;
Lock* _loops_lock_;
DynamicLibrary* kernel_;
};
#define spite_site_8() "library/scheduler.spite:127 in Scheduler.forget_wait"
#define spite_site_9() "library/scheduler.spite:155 in Scheduler.step_ready"
#define spite_site_10() "library/scheduler.spite:158 in Scheduler.step_ready"
#define spite_site_11() "library/scheduler.spite:200 in Scheduler.remove_one"
#define spite_site_12() "spite.crash\t21a70157"
#define spite_site_13() "library/scheduler.spite:259 in Scheduler.offload_start"
#define spite_site_14() "library/scheduler.spite:269 in Scheduler.offload_over"
#define spite_site_15() "library/scheduler.spite:280 in Scheduler.begin_wait"
#define spite_site_16() "library/scheduler.spite:298 in Scheduler.signal"
#define spite_site_17() "spite.crash\t42e51121"
#define spite_site_18() "library/scheduler.spite:340 in Scheduler.timeout"
#define spite_site_19() "library/scheduler.spite:343 in Scheduler.timeout"
#define spite_site_20() "library/scheduler.spite:352 in Scheduler.timeout"
static void* spite_foreign_1_47 = 0;
static void* spite_foreign_1_48 = 0;
static void* spite_foreign_1_49 = 0;
struct SchedulerLoop {
SpiteHeader header;
bool started_;
int64_t thread_;
List_Long* tasks_;
List_Long* deadlines_;
int32_t offloads_running_;
int64_t stepping_;
List_Long* waiting_frames_;
List_Long* awaited_frames_;
bool resumes_when_asked_;
int32_t unfinished_polls_with_no_frame_stepped_;
int64_t wake_;
int64_t wake_signal_;
};
#define SpiteShort_to_integer(self) ((int32_t)(self))
#define SpiteShort_to_long(self) ((int64_t)(self))
struct Socket {
SpiteHeader header;
int64_t handle_;
SpiteString pending_;
bool closed_;
Memory_Heap* heap_;
int64_t _received_;
DynamicLibrary* sockets_;
bool nonblocking_;
};
#define spite_site_21() "library/string.spite:5 in String.length"
#define SpiteString_code_at(self, index) spite_string_code_at(&(self), (index))
struct ThreadSlot {
SpiteHeader header;
int64_t _key_;
DynamicLibrary* _kernel_;
};
static void* spite_foreign_1_72 = 0;
#define spite_site_22() "spite.crash\t3df2aa50"
static void* spite_foreign_1_73 = 0;
static void* spite_foreign_1_74 = 0;
static void* spite_foreign_1_75 = 0;
#define SpiteTiny_to_long(self) ((int64_t)(self))
#define SpiteUnsignedInteger_to_long(self) ((int64_t)(self))
#define SpiteUnsignedInteger_to_float(self) ((float)(self))
#define SpiteUnsignedShort_to_integer(self) ((int32_t)(self))
#define SpiteUnsignedShort_to_long(self) ((int64_t)(self))
#define SpiteUnsignedShort_to_unsigned_integer(self) ((uint32_t)(self))
#define SpiteMemory_Address_copy_to(self, target, bytes) memmove((void*)(intptr_t)(target), (void*)(intptr_t)(self), (size_t)(bytes))
#define SpiteMemory_Address_compare_bytes(self, other, bytes) ({ int64_t spite_bytes = (bytes); spite_bytes > 0 ? (int32_t)memcmp((const void*)(intptr_t)(self), (const void*)(intptr_t)(other), (size_t)spite_bytes) : 0; })
#define SpiteMemory_Address_exchange_long(self, offset, value) __atomic_exchange_n((int64_t*)((char*)(intptr_t)(self) + (offset)), (int64_t)(value), __ATOMIC_SEQ_CST)
#define SpiteMemory_Address_read_long_atomically(self, offset) __atomic_load_n((int64_t*)((char*)(intptr_t)(self) + (offset)), __ATOMIC_SEQ_CST)
#define SpiteMemory_Address_write_long_atomically(self, offset, value) __atomic_store_n((int64_t*)((char*)(intptr_t)(self) + (offset)), (int64_t)(value), __ATOMIC_SEQ_CST)
#define SpiteMemory_Address_add_long_atomically(self, offset, value) __atomic_add_fetch((int64_t*)((char*)(intptr_t)(self) + (offset)), (int64_t)(value), __ATOMIC_SEQ_CST)
#define SpiteMemory_Address_compare_and_swap_long(self, offset, expected, desired) ({ int64_t spite_expected = (int64_t)(expected); __atomic_compare_exchange_n((int64_t*)((char*)(intptr_t)(self) + (offset)), &spite_expected, (int64_t)(desired), false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); })
#define SpiteMemory_Address_read_byte(self, offset) ({ uint8_t spite_read; memcpy(&spite_read, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_byte(self, offset, value) memcpy(((char*)(intptr_t)(self) + (offset)), &(uint8_t){ (value) }, sizeof(uint8_t))
#define SpiteMemory_Address_read_tiny(self, offset) ({ int8_t spite_read; memcpy(&spite_read, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_tiny(self, offset, value) memcpy(((char*)(intptr_t)(self) + (offset)), &(int8_t){ (value) }, sizeof(int8_t))
#define SpiteMemory_Address_read_short(self, offset) ({ int16_t spite_read; memcpy(&spite_read, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_short(self, offset, value) memcpy(((char*)(intptr_t)(self) + (offset)), &(int16_t){ (value) }, sizeof(int16_t))
#define SpiteMemory_Address_read_short_big_endian(self, offset) ({ uint16_t spite_bits; memcpy(&spite_bits, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_bits)); spite_bits = __builtin_bswap16(spite_bits); int16_t spite_read; memcpy(&spite_read, &spite_bits, sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_short_big_endian(self, offset, value) ({ int16_t spite_value = (value); uint16_t spite_bits; memcpy(&spite_bits, &spite_value, sizeof(spite_bits)); spite_bits = __builtin_bswap16(spite_bits); memcpy(((char*)(intptr_t)(self) + (offset)), &spite_bits, sizeof(spite_bits)); })
#define SpiteMemory_Address_read_unsigned_short(self, offset) ({ uint16_t spite_read; memcpy(&spite_read, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_unsigned_short(self, offset, value) memcpy(((char*)(intptr_t)(self) + (offset)), &(uint16_t){ (value) }, sizeof(uint16_t))
#define SpiteMemory_Address_read_unsigned_short_big_endian(self, offset) ({ uint16_t spite_bits; memcpy(&spite_bits, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_bits)); spite_bits = __builtin_bswap16(spite_bits); uint16_t spite_read; memcpy(&spite_read, &spite_bits, sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_unsigned_short_big_endian(self, offset, value) ({ uint16_t spite_value = (value); uint16_t spite_bits; memcpy(&spite_bits, &spite_value, sizeof(spite_bits)); spite_bits = __builtin_bswap16(spite_bits); memcpy(((char*)(intptr_t)(self) + (offset)), &spite_bits, sizeof(spite_bits)); })
#define SpiteMemory_Address_read_integer(self, offset) ({ int32_t spite_read; memcpy(&spite_read, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_integer(self, offset, value) memcpy(((char*)(intptr_t)(self) + (offset)), &(int32_t){ (value) }, sizeof(int32_t))
#define SpiteMemory_Address_read_integer_big_endian(self, offset) ({ uint32_t spite_bits; memcpy(&spite_bits, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_bits)); spite_bits = __builtin_bswap32(spite_bits); int32_t spite_read; memcpy(&spite_read, &spite_bits, sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_integer_big_endian(self, offset, value) ({ int32_t spite_value = (value); uint32_t spite_bits; memcpy(&spite_bits, &spite_value, sizeof(spite_bits)); spite_bits = __builtin_bswap32(spite_bits); memcpy(((char*)(intptr_t)(self) + (offset)), &spite_bits, sizeof(spite_bits)); })
#define SpiteMemory_Address_read_unsigned_integer(self, offset) ({ uint32_t spite_read; memcpy(&spite_read, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_unsigned_integer(self, offset, value) memcpy(((char*)(intptr_t)(self) + (offset)), &(uint32_t){ (value) }, sizeof(uint32_t))
#define SpiteMemory_Address_read_unsigned_integer_big_endian(self, offset) ({ uint32_t spite_bits; memcpy(&spite_bits, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_bits)); spite_bits = __builtin_bswap32(spite_bits); uint32_t spite_read; memcpy(&spite_read, &spite_bits, sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_unsigned_integer_big_endian(self, offset, value) ({ uint32_t spite_value = (value); uint32_t spite_bits; memcpy(&spite_bits, &spite_value, sizeof(spite_bits)); spite_bits = __builtin_bswap32(spite_bits); memcpy(((char*)(intptr_t)(self) + (offset)), &spite_bits, sizeof(spite_bits)); })
#define SpiteMemory_Address_read_long(self, offset) ({ int64_t spite_read; memcpy(&spite_read, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_long(self, offset, value) memcpy(((char*)(intptr_t)(self) + (offset)), &(int64_t){ (value) }, sizeof(int64_t))
#define SpiteMemory_Address_read_long_big_endian(self, offset) ({ uint64_t spite_bits; memcpy(&spite_bits, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_bits)); spite_bits = __builtin_bswap64(spite_bits); int64_t spite_read; memcpy(&spite_read, &spite_bits, sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_long_big_endian(self, offset, value) ({ int64_t spite_value = (value); uint64_t spite_bits; memcpy(&spite_bits, &spite_value, sizeof(spite_bits)); spite_bits = __builtin_bswap64(spite_bits); memcpy(((char*)(intptr_t)(self) + (offset)), &spite_bits, sizeof(spite_bits)); })
#define SpiteMemory_Address_read_unsigned_long(self, offset) ({ uint64_t spite_read; memcpy(&spite_read, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_unsigned_long(self, offset, value) memcpy(((char*)(intptr_t)(self) + (offset)), &(uint64_t){ (value) }, sizeof(uint64_t))
#define SpiteMemory_Address_read_unsigned_long_big_endian(self, offset) ({ uint64_t spite_bits; memcpy(&spite_bits, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_bits)); spite_bits = __builtin_bswap64(spite_bits); uint64_t spite_read; memcpy(&spite_read, &spite_bits, sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_unsigned_long_big_endian(self, offset, value) ({ uint64_t spite_value = (value); uint64_t spite_bits; memcpy(&spite_bits, &spite_value, sizeof(spite_bits)); spite_bits = __builtin_bswap64(spite_bits); memcpy(((char*)(intptr_t)(self) + (offset)), &spite_bits, sizeof(spite_bits)); })
#define SpiteMemory_Address_read_float(self, offset) ({ float spite_read; memcpy(&spite_read, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_float(self, offset, value) memcpy(((char*)(intptr_t)(self) + (offset)), &(float){ (value) }, sizeof(float))
#define SpiteMemory_Address_read_float_big_endian(self, offset) ({ uint32_t spite_bits; memcpy(&spite_bits, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_bits)); spite_bits = __builtin_bswap32(spite_bits); float spite_read; memcpy(&spite_read, &spite_bits, sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_float_big_endian(self, offset, value) ({ float spite_value = (value); uint32_t spite_bits; memcpy(&spite_bits, &spite_value, sizeof(spite_bits)); spite_bits = __builtin_bswap32(spite_bits); memcpy(((char*)(intptr_t)(self) + (offset)), &spite_bits, sizeof(spite_bits)); })
#define SpiteMemory_Address_read_double(self, offset) ({ double spite_read; memcpy(&spite_read, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_double(self, offset, value) memcpy(((char*)(intptr_t)(self) + (offset)), &(double){ (value) }, sizeof(double))
#define SpiteMemory_Address_read_double_big_endian(self, offset) ({ uint64_t spite_bits; memcpy(&spite_bits, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_bits)); spite_bits = __builtin_bswap64(spite_bits); double spite_read; memcpy(&spite_read, &spite_bits, sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_double_big_endian(self, offset, value) ({ double spite_value = (value); uint64_t spite_bits; memcpy(&spite_bits, &spite_value, sizeof(spite_bits)); spite_bits = __builtin_bswap64(spite_bits); memcpy(((char*)(intptr_t)(self) + (offset)), &spite_bits, sizeof(spite_bits)); })
#define SpiteMemory_Address_to_long(self) ((int64_t)(self))
struct Memory_Arena {
SpiteHeader header;
int64_t _block_;
int64_t _block_bytes_;
int64_t _end_;
int64_t _used_;
Memory_Heap* heap_;
};
#define spite_site_23() "library/memory/arena.spite:12 in Memory.Arena.allocate"
#define spite_site_24() "library/memory/arena.spite:13 in Memory.Arena.allocate"
#define spite_site_25() "library/memory/arena.spite:17 in Memory.Arena.allocate"
#define spite_site_26() "library/memory/arena.spite:25 in Memory.Arena.start_block"
#define spite_site_27() "library/memory/arena.spite:26 in Memory.Arena.start_block"
struct Memory_Heap {
SpiteHeader header;
};
struct Spite_Argument {
SpiteHeader header;
SpiteString _name_;
Spite_Class* _class_;
int32_t _index_;
bool _mutated_;
};
struct Spite_AttributeDeclaration {
SpiteHeader header;
SpiteString _name_;
Spite_Class* _class_;
int32_t _index_;
};
struct Spite_Class {
SpiteHeader header;
SpiteString _name_;
Spite_Namespace* _namespace_;
bool _singleton_;
bool _fits_vector_;
bool _stateful_;
bool _list_;
bool _dictionary_;
bool _optional_;
bool _enum_;
SpiteString _source_folder_;
List_String* _source_paths_;
List_Spite_AttributeDeclaration* _attributes_;
List_Spite_Function* _functions_;
List_Spite_Function* _unbound_functions_;
};
typedef struct List_Symbol List_Symbol;
typedef struct TypedMemory__Symbol TypedMemory__Symbol;
struct Spite_Function {
SpiteHeader header;
SpiteString _name_;
List_Spite_Argument* _arguments_;
Spite_Class* _returns_;
bool _waits_;
SpiteString _returned_literal_;
bool _has_returned_literal_;
SpiteString _accessed_;
List_Symbol* _accessed_names_;
List_Spite_Class* _accessed_classes_;
void* spite_owner;
void (*spite_release_owner)(void*);
void (*spite_call)(void*);
void* spite_typed_call;
void* spite_text_call;
void (*spite_add_arguments)(Spite_Function*);
int32_t spite_arguments_lock;
};
struct Spite_Namespace {
SpiteHeader header;
SpiteString _name_;
SpiteString _name_with_namespaces_;
Spite_Namespace* _parent_;
List_Spite_Class* _classes_;
List_Spite_Namespace* _namespaces_;
List_Spite_Class* _enums_;
List_String* _source_paths_;
};
struct Naive {
SpiteHeader header;
Console* console_;
Clock* clock_;
};
static SpiteString spite_lit_14 = SPITE_STATIC_STRING(".spite/reads_in_a_row_first.txt", 31);
static SpiteString spite_lit_15 = SPITE_STATIC_STRING(".spite/reads_in_a_row_second.txt", 32);
static SpiteString spite_lit_16 = SPITE_STATIC_STRING("first", 5);
static SpiteString spite_lit_17 = SPITE_STATIC_STRING("second", 6);
#define spite_site_28() "benchmarks/reads_in_a_row_overlap/naive/naive.spite:13 in Naive.Naive"
static SpiteBox_SpiteString spite_lit_18_box = { { 0, -1 }, SPITE_STATIC_STRING("characters", 10) };
static SpiteBox_SpiteString spite_lit_19_box = { { 0, -1 }, SPITE_STATIC_STRING("removed", 7) };
typedef struct SpiteBox_SpiteBoolean { SpiteHeader header; bool value; } SpiteBox_SpiteBoolean;
static SpiteString spite_lit_20 = SPITE_STATIC_STRING("microseconds ", 13);
static SpiteString spite_lit_21 = SPITE_STATIC_STRING(" line number ", 13);
static SpiteString spite_lit_22 = SPITE_STATIC_STRING("\n", 1);
static SpiteString spite_symbol_3 = { (int64_t)0x0000000064616572ULL, (int64_t)0x0b00000000000000ULL };
static Spite_Class* spite_class_object_Nullable_String_cache = 0;
static bool spite_class_object_Nullable_String_ready = false;
typedef struct Concurrent__Nullable_String Concurrent__Nullable_String;
typedef struct List_Nullable_String List_Nullable_String;
typedef struct TypedMemory__Nullable_String TypedMemory__Nullable_String;
typedef struct SpiteFrame { bool (*step)(void*); int32_t state; int32_t flags; void* work; } SpiteFrame;
#define SPITE_SUSPEND(point) do { spite_frame->spite_head.state = (point); return false; } while (0)
#define spite_site_29() "spite.crash\t27b0ebb9"
#define spite_site_30() "spite.crash\t28129d08"
#define spite_site_31() "benchmarks/reads_in_a_row_overlap/naive/naive.spite:46 in Naive.read_both"
struct List_String {
SpiteHeader header;
Memory_Heap* heap_;
TypedMemory__String* values_;
int64_t items_;
int32_t item_count_;
int32_t capacity_;
};
struct TypedMemory__String {
SpiteHeader header;
};
struct List_Long {
SpiteHeader header;
Memory_Heap* heap_;
TypedMemory__Long* values_;
int64_t items_;
int32_t item_count_;
int32_t capacity_;
};
struct TypedMemory__Long {
SpiteHeader header;
};
struct List_Memory_Address {
SpiteHeader header;
Memory_Heap* heap_;
TypedMemory__Memory_Address* values_;
int64_t items_;
int32_t item_count_;
int32_t capacity_;
};
typedef struct { bool has_value; int64_t value; } Nullable_Memory_Address;
struct TypedMemory__Memory_Address {
SpiteHeader header;
};
struct ThreadLocal__SchedulerLoop {
SpiteHeader header;
ThreadSlot* _slot_;
Lock* _lock_;
Memory_Heap* _heap_;
TypedMemory__SchedulerLoop* _values_;
int64_t _published_;
int32_t _count_;
int32_t _capacity_;
List_Memory_Address* _retired_;
};
struct TypedMemory__SchedulerLoop {
SpiteHeader header;
};
struct List_SchedulerLoop {
SpiteHeader header;
Memory_Heap* heap_;
TypedMemory__SchedulerLoop* values_;
int64_t items_;
int32_t item_count_;
int32_t capacity_;
};
struct List_Spite_AttributeDeclaration {
SpiteHeader header;
Memory_Heap* heap_;
TypedMemory__Spite_AttributeDeclaration* values_;
int64_t items_;
int32_t item_count_;
int32_t capacity_;
};
struct TypedMemory__Spite_AttributeDeclaration {
SpiteHeader header;
};
struct List_Spite_Function {
SpiteHeader header;
Memory_Heap* heap_;
TypedMemory__Spite_Function* values_;
int64_t items_;
int32_t item_count_;
int32_t capacity_;
};
struct TypedMemory__Spite_Function {
SpiteHeader header;
};
struct List_Spite_Argument {
SpiteHeader header;
Memory_Heap* heap_;
TypedMemory__Spite_Argument* values_;
int64_t items_;
int32_t item_count_;
int32_t capacity_;
};
struct TypedMemory__Spite_Argument {
SpiteHeader header;
};
struct List_Spite_Class {
SpiteHeader header;
Memory_Heap* heap_;
TypedMemory__Spite_Class* values_;
int64_t items_;
int32_t item_count_;
int32_t capacity_;
};
struct TypedMemory__Spite_Class {
SpiteHeader header;
};
struct List_Spite_Namespace {
SpiteHeader header;
Memory_Heap* heap_;
TypedMemory__Spite_Namespace* values_;
int64_t items_;
int32_t item_count_;
int32_t capacity_;
};
struct TypedMemory__Spite_Namespace {
SpiteHeader header;
};
struct List_Console_Printable {
SpiteHeader header;
Memory_Heap* heap_;
TypedMemory__Console_Printable* values_;
int64_t items_;
int32_t item_count_;
int32_t capacity_;
};
struct TypedMemory__Console_Printable {
SpiteHeader header;
};
struct List_Symbol {
SpiteHeader header;
Memory_Heap* heap_;
TypedMemory__Symbol* values_;
int64_t items_;
int32_t item_count_;
int32_t capacity_;
};
struct TypedMemory__Symbol {
SpiteHeader header;
};
struct Concurrent__Nullable_String {
SpiteHeader header;
Scheduler* _scheduler_;
Spite_Function* _work_;
List_Nullable_String* _results_;
int64_t _frame_;
bool _finished_;
};
struct List_Nullable_String {
SpiteHeader header;
Memory_Heap* heap_;
TypedMemory__Nullable_String* values_;
int64_t items_;
int32_t item_count_;
int32_t capacity_;
};
struct TypedMemory__Nullable_String {
SpiteHeader header;
};
#define SpiteByte_to_unsigned_integer(self) ((uint32_t)(self))
#define SpiteDouble_to_float(self) ((float)(self))
#define SpiteInteger_to_short(self) ((int16_t)(self))
#define SpiteLong_to_double(self) ((double)(self))
#define spite_site_32() "library/list.spite:20 in List.append"
#define spite_site_33() "spite.crash\t4d4e4520"
#define spite_site_34() "spite.crash\t2a307341"
#define spite_site_35() "library/list.spite:57 in List.remove_at"
#define spite_site_36() "library/list.spite:58 in List.remove_at"
#define spite_site_37() "library/list.spite:185 in List.join"
#define spite_site_38() "library/list.spite:187 in List.join"
#define spite_site_39() "library/list.spite:194 in List.join"
#define spite_site_40() "library/list.spite:615 in List.write_text"
#define spite_site_41() "library/list.spite:618 in List.write_text"
#define spite_site_42() "library/list.spite:856 in List._grow"
#define spite_site_43() "library/list.spite:861 in List._grow"
#define spite_site_44() "library/list.spite:879 in List.move_items"
#define spite_site_45() "library/list.spite:880 in List.move_items"
#define spite_site_46() "library/thread_local.spite:20 in ThreadLocal.get"
#define spite_site_47() "library/thread_local.spite:29 in ThreadLocal.set"
#define spite_site_48() "library/thread_local.spite:33 in ThreadLocal.set"
#define spite_site_49() "library/thread_local.spite:34 in ThreadLocal.set"
#define spite_site_50() "library/thread_local.spite:42 in ThreadLocal._make_room"
#define spite_site_51() "library/thread_local.spite:47 in ThreadLocal._make_room"
#define spite_site_52() "library/thread_local.spite:49 in ThreadLocal._make_room"
#define spite_site_53() "spite.crash\t3d65dd5b"
#define spite_site_54() "spite.crash\t058a607d"
#define spite_site_55() "spite.crash\t53d916b5"
#define spite_site_56() "library/list.spite:75 in List.remove_last"
#define spite_site_57() "library/list.spite:76 in List.remove_last"
#define spite_site_58() "library/list.spite:77 in List.remove_last"
static SpiteString spite_symbol_4 = { (int64_t)0x003f676e69727453ULL, (int64_t)0x0800000000000000ULL };
static SpiteString spite_symbol_5 = { (int64_t)0x797469746e656469ULL, (int64_t)0x0700000000000000ULL };
static SpiteString spite_lit_23 = SPITE_STATIC_STRING("rb", 2);
typedef struct File_read___frame { SpiteFrame spite_head; SpiteString spite_result; File* self; int64_t handle_; int64_t length_; int64_t buffer_; int64_t spite_temp_1; int64_t got_; SpiteString content_; SpiteString spite_temp_2; void* spite_wait_1; void* spite_wait_2; } File_read___frame;
typedef struct SpiteOffload { int64_t done; void (*perform)(void*); void* scheduler; } SpiteOffload;
typedef struct Console_read_line_into___call { SpiteOffload offload; Console* self; int64_t buffer_; int32_t bytes_; bool result; } Console_read_line_into___call;
typedef struct File_read_into___call { SpiteOffload offload; File* self; int64_t buffer_; int64_t bytes_; int64_t handle_; int64_t result; } File_read_into___call;
typedef struct File_write_from___call { SpiteOffload offload; File* self; int64_t address_; int64_t bytes_; int64_t handle_; int64_t result; } File_write_from___call;
typedef struct File_write_text___call { SpiteOffload offload; File* self; SpiteString text_; int64_t handle_; int64_t result; } File_write_text___call;
typedef struct Socket_accept_handle___call { SpiteOffload offload; Socket* self; int64_t result; } Socket_accept_handle___call;
typedef struct Socket_receive_into___call { SpiteOffload offload; Socket* self; int64_t buffer_; int32_t bytes_; int32_t result; } Socket_receive_into___call;
typedef struct Socket_first_readable___call { SpiteOffload offload; Socket* self; List_Long* handles_; int32_t milliseconds_; int32_t result; } Socket_first_readable___call;
typedef struct File_read_into___frame { SpiteFrame spite_head; int64_t spite_result; File* self; int64_t buffer_; int64_t bytes_; int64_t handle_; File_read_into___call spite_call; int64_t spite_thread; } File_read_into___frame;
#define SPITE_GUARDS_HELD() 0
#define SPITE_GUARDS_COUNT(change) ((void)0)
Memory_Heap* spite_singleton_Memory_Heap(void);
TypedMemory__String* spite_singleton_TypedMemory__String(void);
Console_Printable Console_Printable___retain(Console_Printable self);
void Console_Printable___release(Console_Printable self);
Build* spite_singleton_Build(void);
Console* spite_singleton_Console(void);
TypedMemory__Long* spite_singleton_TypedMemory__Long(void);
DynamicLibrary* spite_foreign_library_1(void);
DynamicLibrary* spite_foreign_library_2(void);
Clock* spite_singleton_Clock(void);
TypedMemory__Memory_Address* spite_singleton_TypedMemory__Memory_Address(void);
Scheduler* spite_singleton_Scheduler(void);
TypedMemory__SchedulerLoop* spite_singleton_TypedMemory__SchedulerLoop(void);
TypedMemory__Spite_AttributeDeclaration* spite_singleton_TypedMemory__Spite_AttributeDeclaration(void);
TypedMemory__Spite_Function* spite_singleton_TypedMemory__Spite_Function(void);
TypedMemory__Spite_Argument* spite_singleton_TypedMemory__Spite_Argument(void);
void Launcher___init(Launcher* self);
Launcher* Launcher___allocate(void);
static inline void Launcher___release(Launcher* self);
void Launcher___free(Launcher* self);
void Launcher_Launcher(Launcher* self);
static void spite_overflowed(const char* operation, const char* type, const char* symbol, int64_t left, int64_t right, const char* where) __attribute__((noreturn, cold));
TypedMemory__Console_Printable* spite_singleton_TypedMemory__Console_Printable(void);
static List_Console_Printable* List_Console_Printable___framed(List_Console_Printable* self, int64_t items, int32_t count);
void* spite_box_SpiteString(SpiteString value);
void spite_string_box_release(void* self);
static inline SpiteTagged spite_tagged_SpiteLong(int64_t value);
static void spite_narrowed(int64_t value, const char* from, const char* to, const char* where) __attribute__((noreturn, cold));
static void spite_divided_by_zero(const char* operation, const char* where);
static void spite_outside_list(const char* read, const char* where) __attribute__((noreturn, cold));
SpiteString SpiteBoolean_to_string(bool self);
void Build___release(Build* self);
void Clock___init(Clock* self);
Clock* Clock___allocate(void);
Clock* Clock___make(void);
void Clock___release(Clock* self);
void Clock_Clock(Clock* self);
int64_t Clock_elapsed_nanoseconds(Clock* self);
void Clock___destroy(Clock* self);
void Clock___discard(Clock* self);
void Console___init(Console* self);
Console* Console___allocate(void);
Console* Console___make(void);
void Console___release(Console* self);
void Console_print(Console* self, List_Console_Printable* values_);
void Console_error(Console* self, List_Console_Printable* values_);
void Console__write_values(Console* self, List_Console_Printable* values_, Console_Stream stream_);
void Console__write_to(Console* self, SpiteString text_, Console_Stream stream_);
void Console__write_output(Console* self, SpiteString text_);
void Console__write_error(Console* self, SpiteString text_);
void Console__flush(Console* self);
void Console___destroy(Console* self);
void Console___discard(Console* self);
SpiteString Console_Printable___call_to_string(Console_Printable self);
static File* File___framed(File* self);
static File* File___make_into(File* self, SpiteString starting_path_);
static void File___unframe(File* self);
void DynamicLibrary___init(DynamicLibrary* self);
DynamicLibrary* DynamicLibrary___allocate(void);
DynamicLibrary* DynamicLibrary___make(SpiteString file_, SpiteString _naming_, SpiteString _header_);
void DynamicLibrary___release(DynamicLibrary* self);
void DynamicLibrary_drop(DynamicLibrary* self);
void DynamicLibrary_DynamicLibrary(DynamicLibrary* self, SpiteString file_, SpiteString _naming_, SpiteString _header_);
void DynamicLibrary_drop(DynamicLibrary* self);
int64_t DynamicLibrary_open_library(DynamicLibrary* self, SpiteString file_);
int64_t DynamicLibrary_find_symbol(DynamicLibrary* self, SpiteString name_, SpiteString wanted_by_);
void DynamicLibrary_close_library(DynamicLibrary* self, int64_t opened_);
void DynamicLibrary___destroy(DynamicLibrary* self);
void DynamicLibrary___discard(DynamicLibrary* self);
void File___init(File* self);
static inline File* File___retain(File* self);
static inline void File___release(File* self);
void File___free(File* self);
void File_File(File* self, SpiteString starting_path_);
SpiteString File_read(File* self);
bool File_write(File* self, SpiteString text_);
bool File_put(File* self, SpiteString text_, SpiteString mode_);
int64_t File_open_file(File* self, SpiteString mode_);
void File_close_file(File* self, int64_t handle_);
void File_seek(File* self, int64_t handle_, int64_t offset_, int32_t origin_);
int64_t File_tell(File* self, int64_t handle_);
int64_t File_read_into(File* self, int64_t buffer_, int64_t bytes_, int64_t handle_);
int64_t File_write_text(File* self, SpiteString text_, int64_t handle_);
bool File_remove(File* self);
SpiteString SpiteInteger_to_string(int32_t self);
void Lock___init(Lock* self);
Lock* Lock___allocate(void);
Lock* Lock___make(void);
static inline void Lock___release(Lock* self);
void Lock___free(Lock* self);
void Lock_drop(Lock* self);
void Lock_Lock(Lock* self);
void Lock_lock(Lock* self);
void Lock_unlock(Lock* self);
void Lock_drop(Lock* self);
int64_t Lock_create_lock(Lock* self);
void Lock_acquire(Lock* self, int64_t held_);
void Lock_release_lock(Lock* self, int64_t held_);
void Lock_destroy_lock(Lock* self, int64_t held_);
SpiteString SpiteLong_to_string(int64_t self);
void Scheduler___init(Scheduler* self);
Scheduler* Scheduler___allocate(void);
Scheduler* Scheduler___make(void);
void Scheduler___release(Scheduler* self);
SchedulerLoop* Scheduler_loop(Scheduler* self);
void Scheduler_start(Scheduler* self);
bool Scheduler_on_main_thread(Scheduler* self);
bool Scheduler_waits_here(Scheduler* self);
void Scheduler_begin(Scheduler* self, int64_t frame_);
bool Scheduler_run_frame(Scheduler* self, int64_t frame_);
bool Scheduler_frame_done(Scheduler* self, int64_t frame_);
void Scheduler_forget_wait(Scheduler* self, int64_t frame_);
int32_t Scheduler_step_ready(Scheduler* self);
bool Scheduler_wait_for(Scheduler* self, int64_t frame_);
bool Scheduler_runs_below(Scheduler* self, int64_t frame_);
void Scheduler_remove_one(Scheduler* self, List_Long* values_, int64_t value_);
bool Scheduler_offload(Scheduler* self, int64_t call_, int64_t entry_);
int64_t Scheduler_offload_start(Scheduler* self, int64_t call_, int64_t entry_);
bool Scheduler_offload_over(Scheduler* self, int64_t call_, int64_t thread_);
void Scheduler_offload_done(Scheduler* self, int64_t call_);
void Scheduler_begin_wait(Scheduler* self);
void Scheduler_signal(Scheduler* self);
void Scheduler_idle(Scheduler* self);
void Scheduler_idle_stepping(Scheduler* self, bool steps_);
int32_t Scheduler_timeout(Scheduler* self);
int64_t Scheduler_current_thread(Scheduler* self);
void Scheduler_create_event(Scheduler* self, SchedulerLoop* event_loop_);
void Scheduler_signal_event(Scheduler* self, SchedulerLoop* event_loop_);
void Scheduler_wait_event(Scheduler* self, SchedulerLoop* event_loop_, int32_t milliseconds_);
int64_t Scheduler_clock(Scheduler* self);
int64_t Scheduler_start_thread(Scheduler* self, int64_t entry_, int64_t argument_);
void Scheduler_end_thread(Scheduler* self, int64_t thread_);
bool Scheduler_step_frame(Scheduler* self, int64_t frame_);
void Scheduler_release_work(Scheduler* self, int64_t frame_);
void Scheduler__want_check_point(Scheduler* self);
void Scheduler___destroy(Scheduler* self);
void Scheduler___discard(Scheduler* self);
static SPITE_CRASH_REPORT void spite_failed_1(int64_t thread_, int64_t call_, int64_t entry_, Scheduler* self);
static SPITE_CRASH_REPORT void spite_failed_2(int32_t wait_limit_, SchedulerLoop* here_, Scheduler* self, bool spite_crash_reached_1, bool main_, bool steps_, int32_t finished_);
void SchedulerLoop___init(SchedulerLoop* self);
SchedulerLoop* SchedulerLoop___allocate(void);
SchedulerLoop* SchedulerLoop___make(void);
static inline SchedulerLoop* SchedulerLoop___retain(SchedulerLoop* self);
static inline void SchedulerLoop___release(SchedulerLoop* self);
void SchedulerLoop___free(SchedulerLoop* self);
int32_t SpiteString_length(SpiteString self);
SpiteString SpiteString_to_string(SpiteString self);
SpiteString SpiteString___retain(SpiteString self);
void SpiteString___release(SpiteString self);
SpiteString spite_string_from_bytes(const char* bytes, int64_t length);
SpiteString spite_string_join(int32_t count, const SpiteString* pieces);
static int64_t spite_long_digits(char* digits, int64_t value);
static SpiteStringBlock* spite_string_block(int64_t length);
static SpiteString spite_string_held(SpiteStringBlock* block, int64_t length);
void ThreadSlot___init(ThreadSlot* self);
ThreadSlot* ThreadSlot___allocate(void);
ThreadSlot* ThreadSlot___make(void);
static inline void ThreadSlot___release(ThreadSlot* self);
void ThreadSlot___free(ThreadSlot* self);
void ThreadSlot_drop(ThreadSlot* self);
void ThreadSlot_ThreadSlot(ThreadSlot* self);
int64_t ThreadSlot_read(ThreadSlot* self);
void ThreadSlot_write(ThreadSlot* self, int64_t value_);
void ThreadSlot_drop(ThreadSlot* self);
int64_t ThreadSlot_create_key(ThreadSlot* self);
int64_t ThreadSlot_read_key(ThreadSlot* self, int64_t key_);
void ThreadSlot_write_key(ThreadSlot* self, int64_t key_, int64_t value_);
void ThreadSlot_delete_key(ThreadSlot* self, int64_t key_);
static SPITE_CRASH_REPORT void spite_failed_3(int64_t created_, ThreadSlot* self);
SpiteString SpiteMemory_Address_text(int64_t self, int64_t length_);
SpiteString SpiteMemory_Address_to_string(int64_t self);
int64_t Memory_Arena_allocate(Memory_Arena* self, int64_t bytes_);
void Memory_Arena_free(Memory_Arena* self, int64_t _address_);
void Memory_Arena_start_block(Memory_Arena* self, int64_t at_least_);
void Memory_Heap___release(Memory_Heap* self);
int64_t Memory_Heap_allocate(Memory_Heap* self, int64_t bytes_);
int64_t Memory_Heap_resize(Memory_Heap* self, int64_t address_, int64_t bytes_);
void Memory_Heap_free(Memory_Heap* self, int64_t address_);
static inline void Spite_Argument___release(Spite_Argument* self);
void Spite_Argument___free(Spite_Argument* self);
static inline void Spite_AttributeDeclaration___release(Spite_AttributeDeclaration* self);
void Spite_AttributeDeclaration___free(Spite_AttributeDeclaration* self);
void Spite_Class___init(Spite_Class* self);
Spite_Class* Spite_Class___allocate(void);
Spite_Class* Spite_Class___make(SpiteString starting_name_);
static inline Spite_Class* Spite_Class___retain(Spite_Class* self);
static inline void Spite_Class___release(Spite_Class* self);
void Spite_Class___free(Spite_Class* self);
void Spite_Class_Class(Spite_Class* self, SpiteString starting_name_);
Spite_Function* Spite_Function___make(SpiteString starting_name_, Spite_Class* starting_returns_);
static inline Spite_Function* Spite_Function___retain(Spite_Function* self);
static inline void Spite_Function___release(Spite_Function* self);
void Spite_Function___free(Spite_Function* self);
void Spite_Function_Function(Spite_Function* self, SpiteString starting_name_, Spite_Class* starting_returns_);
static inline void Spite_Namespace___release(Spite_Namespace* self);
void Spite_Namespace___free(Spite_Namespace* self);
void Naive___init(Naive* self);
Naive* Naive___allocate(void);
static inline void Naive___release(Naive* self);
void Naive___free(Naive* self);
void Naive_Naive(Naive* self);
SpiteString Naive_make_text(Naive* self, SpiteString word_, int32_t count_);
int64_t Naive_read_rounds___held_0_1(Naive* self, File* first_file_, File* second_file_, int32_t rounds_);
static inline SpiteTagged spite_tagged_SpiteBoolean(bool value);
int32_t Naive_read_both___held_0_1(Naive* self, File* first_file_, File* second_file_);
Spite_Class* spite_class_object_Nullable_String(void);
void File_read___dropping_call(void* owner);
Spite_Function* spite_function_value_File_read(File* owner);
static void* spite_frame_start(Spite_Function* spite_work);
TypedMemory__Nullable_String* spite_singleton_TypedMemory__Nullable_String(void);
static SPITE_CRASH_REPORT void spite_failed_4(SpiteString second_);
static SPITE_CRASH_REPORT void spite_failed_5(SpiteString spite_temp_3_);
void List_String___init(List_String* self);
List_String* List_String___allocate(void);
List_String* List_String___make(void);
static inline void List_String___release(List_String* self);
void List_String___free(List_String* self);
void List_String_drop(List_String* self);
int32_t List_String_count(List_String* self);
void List_String_append(List_String* self, SpiteString value_);
SpiteString List_String_get_at(List_String* self, int32_t index_);
void List_String_clear(List_String* self);
SpiteString List_String_join(List_String* self, SpiteString separator_);
int64_t List_String_write_text(List_String* self, SpiteString text_, int64_t address_, int64_t position_);
void List_String_drop(List_String* self);
void List_String_make_room(List_String* self);
void List_String__grow(List_String* self);
int64_t List_String__resized(List_String* self, int64_t old_bytes_, int64_t new_bytes_);
void TypedMemory__String___release(TypedMemory__String* self);
SpiteString TypedMemory__String_read_value(TypedMemory__String* self, int64_t address_, int32_t index_);
void TypedMemory__String_write_value(TypedMemory__String* self, int64_t address_, int32_t index_, SpiteString value_);
void TypedMemory__String_release_value(TypedMemory__String* self, int64_t address_, int32_t index_);
int64_t TypedMemory__String_value_bytes(TypedMemory__String* self);
void List_Long___init(List_Long* self);
List_Long* List_Long___allocate(void);
List_Long* List_Long___make(void);
static inline List_Long* List_Long___retain(List_Long* self);
static inline void List_Long___release(List_Long* self);
void List_Long___free(List_Long* self);
void List_Long_drop(List_Long* self);
int32_t List_Long_count(List_Long* self);
bool List_Long_is_empty(List_Long* self);
void List_Long_append(List_Long* self, int64_t value_);
Nullable_Long List_Long_get_at(List_Long* self, int32_t index_);
void List_Long_remove_at(List_Long* self, int32_t index_);
void List_Long_clear(List_Long* self);
void List_Long_drop(List_Long* self);
void List_Long_make_room(List_Long* self);
void List_Long__grow(List_Long* self);
int64_t List_Long__resized(List_Long* self, int64_t old_bytes_, int64_t new_bytes_);
void List_Long_move_items(List_Long* self, int32_t from_, int32_t to_, int32_t moved_count_);
void TypedMemory__Long___release(TypedMemory__Long* self);
int64_t TypedMemory__Long_read_value(TypedMemory__Long* self, int64_t address_, int32_t index_);
void TypedMemory__Long_write_value(TypedMemory__Long* self, int64_t address_, int32_t index_, int64_t value_);
void TypedMemory__Long_release_value(TypedMemory__Long* self, int64_t address_, int32_t index_);
int64_t TypedMemory__Long_value_bytes(TypedMemory__Long* self);
void List_Memory_Address___init(List_Memory_Address* self);
List_Memory_Address* List_Memory_Address___allocate(void);
List_Memory_Address* List_Memory_Address___make(void);
static inline void List_Memory_Address___release(List_Memory_Address* self);
void List_Memory_Address___free(List_Memory_Address* self);
void List_Memory_Address_drop(List_Memory_Address* self);
bool List_Memory_Address_is_empty(List_Memory_Address* self);
void List_Memory_Address_append(List_Memory_Address* self, int64_t value_);
Nullable_Memory_Address List_Memory_Address_remove_last(List_Memory_Address* self);
void List_Memory_Address_clear(List_Memory_Address* self);
void List_Memory_Address_drop(List_Memory_Address* self);
void List_Memory_Address_make_room(List_Memory_Address* self);
void List_Memory_Address__grow(List_Memory_Address* self);
int64_t List_Memory_Address__resized(List_Memory_Address* self, int64_t old_bytes_, int64_t new_bytes_);
void TypedMemory__Memory_Address___release(TypedMemory__Memory_Address* self);
int64_t TypedMemory__Memory_Address_read_value(TypedMemory__Memory_Address* self, int64_t address_, int32_t index_);
void TypedMemory__Memory_Address_write_value(TypedMemory__Memory_Address* self, int64_t address_, int32_t index_, int64_t value_);
void TypedMemory__Memory_Address_release_value(TypedMemory__Memory_Address* self, int64_t address_, int32_t index_);
int64_t TypedMemory__Memory_Address_value_bytes(TypedMemory__Memory_Address* self);
void ThreadLocal__SchedulerLoop___init(ThreadLocal__SchedulerLoop* self);
ThreadLocal__SchedulerLoop* ThreadLocal__SchedulerLoop___allocate(void);
ThreadLocal__SchedulerLoop* ThreadLocal__SchedulerLoop___make(void);
static inline void ThreadLocal__SchedulerLoop___release(ThreadLocal__SchedulerLoop* self);
void ThreadLocal__SchedulerLoop___free(ThreadLocal__SchedulerLoop* self);
void ThreadLocal__SchedulerLoop_drop(ThreadLocal__SchedulerLoop* self);
void ThreadLocal__SchedulerLoop_ThreadLocal(ThreadLocal__SchedulerLoop* self);
SchedulerLoop* ThreadLocal__SchedulerLoop_get(ThreadLocal__SchedulerLoop* self);
void ThreadLocal__SchedulerLoop_set(ThreadLocal__SchedulerLoop* self, SchedulerLoop* value_);
int64_t ThreadLocal__SchedulerLoop__make_room(ThreadLocal__SchedulerLoop* self);
void ThreadLocal__SchedulerLoop_drop(ThreadLocal__SchedulerLoop* self);
void TypedMemory__SchedulerLoop___release(TypedMemory__SchedulerLoop* self);
SchedulerLoop* TypedMemory__SchedulerLoop_read_value(TypedMemory__SchedulerLoop* self, int64_t address_, int32_t index_);
void TypedMemory__SchedulerLoop_write_value(TypedMemory__SchedulerLoop* self, int64_t address_, int32_t index_, SchedulerLoop* value_);
void TypedMemory__SchedulerLoop_release_value(TypedMemory__SchedulerLoop* self, int64_t address_, int32_t index_);
int64_t TypedMemory__SchedulerLoop_value_bytes(TypedMemory__SchedulerLoop* self);
void List_SchedulerLoop___init(List_SchedulerLoop* self);
List_SchedulerLoop* List_SchedulerLoop___allocate(void);
List_SchedulerLoop* List_SchedulerLoop___make(void);
static inline void List_SchedulerLoop___release(List_SchedulerLoop* self);
void List_SchedulerLoop___free(List_SchedulerLoop* self);
void List_SchedulerLoop_drop(List_SchedulerLoop* self);
int32_t List_SchedulerLoop_count(List_SchedulerLoop* self);
void List_SchedulerLoop_append(List_SchedulerLoop* self, SchedulerLoop* value_);
SchedulerLoop* List_SchedulerLoop_get_at(List_SchedulerLoop* self, int32_t index_);
void List_SchedulerLoop_clear(List_SchedulerLoop* self);
void List_SchedulerLoop_drop(List_SchedulerLoop* self);
void List_SchedulerLoop_make_room(List_SchedulerLoop* self);
void List_SchedulerLoop__grow(List_SchedulerLoop* self);
int64_t List_SchedulerLoop__resized(List_SchedulerLoop* self, int64_t old_bytes_, int64_t new_bytes_);
void List_Spite_AttributeDeclaration___init(List_Spite_AttributeDeclaration* self);
List_Spite_AttributeDeclaration* List_Spite_AttributeDeclaration___allocate(void);
List_Spite_AttributeDeclaration* List_Spite_AttributeDeclaration___make(void);
static inline void List_Spite_AttributeDeclaration___release(List_Spite_AttributeDeclaration* self);
void List_Spite_AttributeDeclaration___free(List_Spite_AttributeDeclaration* self);
void List_Spite_AttributeDeclaration_drop(List_Spite_AttributeDeclaration* self);
void List_Spite_AttributeDeclaration_clear(List_Spite_AttributeDeclaration* self);
void List_Spite_AttributeDeclaration_drop(List_Spite_AttributeDeclaration* self);
void TypedMemory__Spite_AttributeDeclaration___release(TypedMemory__Spite_AttributeDeclaration* self);
void TypedMemory__Spite_AttributeDeclaration_release_value(TypedMemory__Spite_AttributeDeclaration* self, int64_t address_, int32_t index_);
void List_Spite_Function___init(List_Spite_Function* self);
List_Spite_Function* List_Spite_Function___allocate(void);
List_Spite_Function* List_Spite_Function___make(void);
static inline void List_Spite_Function___release(List_Spite_Function* self);
void List_Spite_Function___free(List_Spite_Function* self);
void List_Spite_Function_drop(List_Spite_Function* self);
void List_Spite_Function_clear(List_Spite_Function* self);
void List_Spite_Function_drop(List_Spite_Function* self);
void TypedMemory__Spite_Function___release(TypedMemory__Spite_Function* self);
void TypedMemory__Spite_Function_release_value(TypedMemory__Spite_Function* self, int64_t address_, int32_t index_);
void List_Spite_Argument___init(List_Spite_Argument* self);
List_Spite_Argument* List_Spite_Argument___allocate(void);
List_Spite_Argument* List_Spite_Argument___make(void);
static inline void List_Spite_Argument___release(List_Spite_Argument* self);
void List_Spite_Argument___free(List_Spite_Argument* self);
void List_Spite_Argument_drop(List_Spite_Argument* self);
void List_Spite_Argument_clear(List_Spite_Argument* self);
void List_Spite_Argument_drop(List_Spite_Argument* self);
void TypedMemory__Spite_Argument___release(TypedMemory__Spite_Argument* self);
void TypedMemory__Spite_Argument_release_value(TypedMemory__Spite_Argument* self, int64_t address_, int32_t index_);
static inline void List_Spite_Class___release(List_Spite_Class* self);
void List_Spite_Class___free(List_Spite_Class* self);
void List_Spite_Class_drop(List_Spite_Class* self);
void List_Spite_Class_clear(List_Spite_Class* self);
void List_Spite_Class_drop(List_Spite_Class* self);
void TypedMemory__Spite_Class___release(TypedMemory__Spite_Class* self);
void TypedMemory__Spite_Class_release_value(TypedMemory__Spite_Class* self, int64_t address_, int32_t index_);
static inline void List_Spite_Namespace___release(List_Spite_Namespace* self);
void List_Spite_Namespace___free(List_Spite_Namespace* self);
void List_Spite_Namespace_drop(List_Spite_Namespace* self);
void List_Spite_Namespace_clear(List_Spite_Namespace* self);
void List_Spite_Namespace_drop(List_Spite_Namespace* self);
void TypedMemory__Spite_Namespace___release(TypedMemory__Spite_Namespace* self);
void TypedMemory__Spite_Namespace_release_value(TypedMemory__Spite_Namespace* self, int64_t address_, int32_t index_);
void List_Console_Printable___init(List_Console_Printable* self);
static inline List_Console_Printable* List_Console_Printable___retain(List_Console_Printable* self);
static inline void List_Console_Printable___release(List_Console_Printable* self);
void List_Console_Printable___free(List_Console_Printable* self);
void List_Console_Printable_drop(List_Console_Printable* self);
int32_t List_Console_Printable_count(List_Console_Printable* self);
Console_Printable List_Console_Printable_get_at(List_Console_Printable* self, int32_t index_);
void List_Console_Printable_clear(List_Console_Printable* self);
void List_Console_Printable_drop(List_Console_Printable* self);
void TypedMemory__Console_Printable___release(TypedMemory__Console_Printable* self);
Console_Printable TypedMemory__Console_Printable_read_value(TypedMemory__Console_Printable* self, int64_t address_, int32_t index_);
void TypedMemory__Console_Printable_release_value(TypedMemory__Console_Printable* self, int64_t address_, int32_t index_);
static inline void List_Symbol___release(List_Symbol* self);
void List_Symbol___free(List_Symbol* self);
void List_Symbol_drop(List_Symbol* self);
void List_Symbol_clear(List_Symbol* self);
void List_Symbol_drop(List_Symbol* self);
void TypedMemory__Symbol___release(TypedMemory__Symbol* self);
void TypedMemory__Symbol_release_value(TypedMemory__Symbol* self, int64_t address_, int32_t index_);
void Concurrent__Nullable_String___init(Concurrent__Nullable_String* self);
Concurrent__Nullable_String* Concurrent__Nullable_String___allocate(void);
Concurrent__Nullable_String* Concurrent__Nullable_String___make(Spite_Function* starting_work_);
static inline void Concurrent__Nullable_String___release(Concurrent__Nullable_String* self);
void Concurrent__Nullable_String___free(Concurrent__Nullable_String* self);
void Concurrent__Nullable_String_drop(Concurrent__Nullable_String* self);
void Concurrent__Nullable_String_Concurrent(Concurrent__Nullable_String* self, Spite_Function* starting_work_);
void Concurrent__Nullable_String__run(Concurrent__Nullable_String* self);
void Concurrent__Nullable_String__collect(Concurrent__Nullable_String* self);
SpiteString Concurrent__Nullable_String__result(Concurrent__Nullable_String* self);
void Concurrent__Nullable_String__join(Concurrent__Nullable_String* self);
void Concurrent__Nullable_String_drop(Concurrent__Nullable_String* self);
int64_t Concurrent__Nullable_String__start_frame(Concurrent__Nullable_String* self);
SpiteString Concurrent__Nullable_String__frame_result(Concurrent__Nullable_String* self);
void Concurrent__Nullable_String__free_frame(Concurrent__Nullable_String* self);
void List_Nullable_String___init(List_Nullable_String* self);
List_Nullable_String* List_Nullable_String___allocate(void);
List_Nullable_String* List_Nullable_String___make(void);
static inline void List_Nullable_String___release(List_Nullable_String* self);
void List_Nullable_String___free(List_Nullable_String* self);
void List_Nullable_String_drop(List_Nullable_String* self);
int32_t List_Nullable_String_count(List_Nullable_String* self);
void List_Nullable_String_append(List_Nullable_String* self, SpiteString value_);
SpiteString List_Nullable_String_get_at(List_Nullable_String* self, int32_t index_);
void List_Nullable_String_clear(List_Nullable_String* self);
void List_Nullable_String_drop(List_Nullable_String* self);
void List_Nullable_String_make_room(List_Nullable_String* self);
void List_Nullable_String__grow(List_Nullable_String* self);
int64_t List_Nullable_String__resized(List_Nullable_String* self, int64_t old_bytes_, int64_t new_bytes_);
void TypedMemory__Nullable_String___release(TypedMemory__Nullable_String* self);
SpiteString TypedMemory__Nullable_String_read_value(TypedMemory__Nullable_String* self, int64_t address_, int32_t index_);
void TypedMemory__Nullable_String_write_value(TypedMemory__Nullable_String* self, int64_t address_, int32_t index_, SpiteString value_);
void TypedMemory__Nullable_String_release_value(TypedMemory__Nullable_String* self, int64_t address_, int32_t index_);
int64_t TypedMemory__Nullable_String_value_bytes(TypedMemory__Nullable_String* self);
static SPITE_CRASH_REPORT void spite_failed_6(int32_t index_, List_Long* self);
static SPITE_CRASH_REPORT void spite_failed_7(int32_t index_, List_Long* self);
static SPITE_CRASH_REPORT void spite_failed_8(int64_t items_, ThreadLocal__SchedulerLoop* self);
static SPITE_CRASH_REPORT void spite_failed_9(bool spite_crash_set_1, int32_t spite_crash_value_1, Concurrent__Nullable_String* self);
static SPITE_CRASH_REPORT void spite_failed_10(bool waits_for_its_own_caller_, Concurrent__Nullable_String* self);
static void* File_read___begin(File* self);
static bool File_read___step(void* spite_raw);
static void* spite_offload_thread(void* call);
int64_t File_read_into___waiting(File* self, int64_t buffer_, int64_t bytes_, int64_t handle_);
static void File_read_into___perform(void* raw);
int64_t File_write_text___waiting(File* self, SpiteString text_, int64_t handle_);
static void File_write_text___perform(void* raw);
static void* File_read_into___begin(File* self, int64_t buffer_, int64_t bytes_, int64_t handle_);
static bool File_read_into___step(void* spite_raw);
bool spite_singleton_tracked(void);
void spite_singleton_created(void (*teardown)(void));
void spite_singleton_used_after_exit(const char* name);
void spite_singletons_destroy(void);
void spite_singleton_free_later(void* object);
void spite_singleton_check_circle(const char* name);
void spite_singleton_making(const char* name);
void spite_singleton_made(void);
#define SPITE_ALLOCATOR_List_String(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Long(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Integer(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Byte(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Memory_Address(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Socket(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Spite_Attribute(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_SchedulerLoop(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_UnsignedInteger(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_ThreadPoolJob(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Spite_AttributeDeclaration(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Spite_Function(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Spite_Argument(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Spite_Class(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Spite_Namespace(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Console_Printable(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Console_Debuggable(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Directory_Entry(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_File(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Symbol(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Spite_Access(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Directory(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Nullable_String(object, heap) ((void)(object), ((Spite_Allocator)heap()))
static __typeof__(&TypedMemory__String___release) spite_folded_TypedMemory__String___release = ((__typeof__(&TypedMemory__String___release))&Memory_Heap___release);
static __typeof__(&TypedMemory__Long___release) spite_folded_TypedMemory__Long___release = ((__typeof__(&TypedMemory__Long___release))&Memory_Heap___release);
static __typeof__(&TypedMemory__Memory_Address___release) spite_folded_TypedMemory__Memory_Address___release = ((__typeof__(&TypedMemory__Memory_Address___release))&Memory_Heap___release);
static __typeof__(&TypedMemory__SchedulerLoop___release) spite_folded_TypedMemory__SchedulerLoop___release = ((__typeof__(&TypedMemory__SchedulerLoop___release))&Memory_Heap___release);
static __typeof__(&TypedMemory__Spite_AttributeDeclaration___release) spite_folded_TypedMemory__Spite_AttributeDeclaration___release = ((__typeof__(&TypedMemory__Spite_AttributeDeclaration___release))&Memory_Heap___release);
static __typeof__(&TypedMemory__Spite_Function___release) spite_folded_TypedMemory__Spite_Function___release = ((__typeof__(&TypedMemory__Spite_Function___release))&Memory_Heap___release);
static __typeof__(&TypedMemory__Spite_Argument___release) spite_folded_TypedMemory__Spite_Argument___release = ((__typeof__(&TypedMemory__Spite_Argument___release))&Memory_Heap___release);
static __typeof__(&TypedMemory__Spite_Class___release) spite_folded_TypedMemory__Spite_Class___release = ((__typeof__(&TypedMemory__Spite_Class___release))&Memory_Heap___release);
static __typeof__(&TypedMemory__Spite_Namespace___release) spite_folded_TypedMemory__Spite_Namespace___release = ((__typeof__(&TypedMemory__Spite_Namespace___release))&Memory_Heap___release);
static __typeof__(&TypedMemory__Console_Printable___release) spite_folded_TypedMemory__Console_Printable___release = ((__typeof__(&TypedMemory__Console_Printable___release))&Memory_Heap___release);
static __typeof__(&TypedMemory__Symbol___release) spite_folded_TypedMemory__Symbol___release = ((__typeof__(&TypedMemory__Symbol___release))&Memory_Heap___release);
static __typeof__(&TypedMemory__Nullable_String___release) spite_folded_TypedMemory__Nullable_String___release = ((__typeof__(&TypedMemory__Nullable_String___release))&Memory_Heap___release);
static __typeof__(&List_Long_count) spite_folded_List_Long_count = ((__typeof__(&List_Long_count))&List_String_count);
static __typeof__(&List_Memory_Address_is_empty) spite_folded_List_Memory_Address_is_empty = ((__typeof__(&List_Memory_Address_is_empty))&List_Long_is_empty);
static __typeof__(&TypedMemory__Memory_Address_read_value) spite_folded_TypedMemory__Memory_Address_read_value = ((__typeof__(&TypedMemory__Memory_Address_read_value))&TypedMemory__Long_read_value);
static __typeof__(&TypedMemory__Memory_Address_write_value) spite_folded_TypedMemory__Memory_Address_write_value = ((__typeof__(&TypedMemory__Memory_Address_write_value))&TypedMemory__Long_write_value);
static __typeof__(&TypedMemory__Memory_Address_value_bytes) spite_folded_TypedMemory__Memory_Address_value_bytes = ((__typeof__(&TypedMemory__Memory_Address_value_bytes))&TypedMemory__Long_value_bytes);
static __typeof__(&List_SchedulerLoop_count) spite_folded_List_SchedulerLoop_count = ((__typeof__(&List_SchedulerLoop_count))&List_String_count);
static __typeof__(&List_Console_Printable_count) spite_folded_List_Console_Printable_count = ((__typeof__(&List_Console_Printable_count))&List_String_count);
static __typeof__(&List_Nullable_String_count) spite_folded_List_Nullable_String_count = ((__typeof__(&List_Nullable_String_count))&List_String_count);
static __typeof__(&List_Nullable_String_get_at) spite_folded_List_Nullable_String_get_at = ((__typeof__(&List_Nullable_String_get_at))&List_String_get_at);
static __typeof__(&TypedMemory__Nullable_String_read_value) spite_folded_TypedMemory__Nullable_String_read_value = ((__typeof__(&TypedMemory__Nullable_String_read_value))&TypedMemory__String_read_value);
static __typeof__(&TypedMemory__Nullable_String_write_value) spite_folded_TypedMemory__Nullable_String_write_value = ((__typeof__(&TypedMemory__Nullable_String_write_value))&TypedMemory__String_write_value);
static __typeof__(&TypedMemory__Nullable_String_value_bytes) spite_folded_TypedMemory__Nullable_String_value_bytes = ((__typeof__(&TypedMemory__Nullable_String_value_bytes))&TypedMemory__String_value_bytes);
static __typeof__(&List_Memory_Address_clear) spite_folded_List_Memory_Address_clear = ((__typeof__(&List_Memory_Address_clear))&List_Long_clear);
static __typeof__(&TypedMemory__Memory_Address_release_value) spite_folded_TypedMemory__Memory_Address_release_value = ((__typeof__(&TypedMemory__Memory_Address_release_value))&TypedMemory__Long_release_value);
static __typeof__(&List_Symbol_clear) spite_folded_List_Symbol_clear = ((__typeof__(&List_Symbol_clear))&List_String_clear);
static __typeof__(&TypedMemory__Symbol_release_value) spite_folded_TypedMemory__Symbol_release_value = ((__typeof__(&TypedMemory__Symbol_release_value))&TypedMemory__String_release_value);
static __typeof__(&List_Nullable_String_clear) spite_folded_List_Nullable_String_clear = ((__typeof__(&List_Nullable_String_clear))&List_String_clear);
static __typeof__(&TypedMemory__Nullable_String_release_value) spite_folded_TypedMemory__Nullable_String_release_value = ((__typeof__(&TypedMemory__Nullable_String_release_value))&TypedMemory__String_release_value);
Memory_Heap* spite_singleton_Memory_Heap(void) {
static Memory_Heap spite_object = { { 1, 94 } };
return &spite_object;
}
TypedMemory__String* spite_singleton_TypedMemory__String(void) {
static TypedMemory__String spite_object = { { 1, 111 } };
return &spite_object;
}


Console_Printable Console_Printable___retain(Console_Printable self) {
if (self.plain == 0 && self.value.object != 0) SPITE_COUNT_UP(((SpiteHeader*)self.value.object)->ref_count);
return self;
}
Build* spite_singleton_Build(void) {
static Build spite_object = { { 1, 12 } };
return &spite_object;
}
static void spite_singleton_Console_teardown(void) {
Console* object = spite_singleton_Console_cache;
spite_singleton_Console_cache = 0;
spite_singleton_Console_destroyed = true;
Console___destroy(object);
}
Console* spite_singleton_Console(void) {
Console* found = SPITE_SINGLETON_FOUND(spite_singleton_Console_cache);
if (found != 0) return found;
spite_singleton_check_circle("Console");
SPITE_LOCK(spite_singleton_Console_lock);
if (spite_singleton_Console_cache == 0) {
if (spite_singleton_Console_destroyed) spite_singleton_used_after_exit("Console");
spite_singleton_making("Console");
Console* made = Console___make();
spite_singleton_made();
spite_singleton_created(spite_singleton_Console_teardown);
SPITE_SINGLETON_PUBLISH(spite_singleton_Console_cache, made);
}
SPITE_UNLOCK(spite_singleton_Console_lock);
return spite_singleton_Console_cache;
}
TypedMemory__Long* spite_singleton_TypedMemory__Long(void) {
static TypedMemory__Long spite_object = { { 1, 119 } };
return &spite_object;
}
static void spite_singleton_Clock_teardown(void) {
Clock* object = spite_singleton_Clock_cache;
spite_singleton_Clock_cache = 0;
spite_singleton_Clock_destroyed = true;
Clock___destroy(object);
}
Clock* spite_singleton_Clock(void) {
Clock* found = SPITE_SINGLETON_FOUND(spite_singleton_Clock_cache);
if (found != 0) return found;
spite_singleton_check_circle("Clock");
SPITE_LOCK(spite_singleton_Clock_lock);
if (spite_singleton_Clock_cache == 0) {
if (spite_singleton_Clock_destroyed) spite_singleton_used_after_exit("Clock");
spite_singleton_making("Clock");
Clock* made = Clock___make();
spite_singleton_made();
spite_singleton_created(spite_singleton_Clock_teardown);
SPITE_SINGLETON_PUBLISH(spite_singleton_Clock_cache, made);
}
SPITE_UNLOCK(spite_singleton_Clock_lock);
return spite_singleton_Clock_cache;
}
TypedMemory__Memory_Address* spite_singleton_TypedMemory__Memory_Address(void) {
static TypedMemory__Memory_Address spite_object = { { 1, 125 } };
return &spite_object;
}
static void spite_singleton_Scheduler_teardown(void) {
Scheduler* object = spite_singleton_Scheduler_cache;
spite_singleton_Scheduler_cache = 0;
spite_singleton_Scheduler_destroyed = true;
Scheduler___destroy(object);
}
Scheduler* spite_singleton_Scheduler(void) {
Scheduler* found = SPITE_SINGLETON_FOUND(spite_singleton_Scheduler_cache);
if (found != 0) return found;
spite_singleton_check_circle("Scheduler");
SPITE_LOCK(spite_singleton_Scheduler_lock);
if (spite_singleton_Scheduler_cache == 0) {
if (spite_singleton_Scheduler_destroyed) spite_singleton_used_after_exit("Scheduler");
spite_singleton_making("Scheduler");
Scheduler* made = Scheduler___make();
spite_singleton_made();
spite_singleton_created(spite_singleton_Scheduler_teardown);
SPITE_SINGLETON_PUBLISH(spite_singleton_Scheduler_cache, made);
}
SPITE_UNLOCK(spite_singleton_Scheduler_lock);
return spite_singleton_Scheduler_cache;
}
TypedMemory__SchedulerLoop* spite_singleton_TypedMemory__SchedulerLoop(void) {
static TypedMemory__SchedulerLoop spite_object = { { 1, 132 } };
return &spite_object;
}
TypedMemory__Spite_AttributeDeclaration* spite_singleton_TypedMemory__Spite_AttributeDeclaration(void) {
static TypedMemory__Spite_AttributeDeclaration spite_object = { { 1, 139 } };
return &spite_object;
}
TypedMemory__Spite_Function* spite_singleton_TypedMemory__Spite_Function(void) {
static TypedMemory__Spite_Function spite_object = { { 1, 141 } };
return &spite_object;
}
TypedMemory__Spite_Argument* spite_singleton_TypedMemory__Spite_Argument(void) {
static TypedMemory__Spite_Argument spite_object = { { 1, 143 } };
return &spite_object;
}
void Launcher___init(Launcher* self) {
self->build_ = spite_singleton_Build();
}
Launcher* Launcher___allocate(void) {
Launcher* self = (Launcher*)SPITE_MALLOC(sizeof(Launcher));
self->header.ref_count = 1;
self->header.class_id = 1;
Launcher___init(self);
#ifdef SPITE_TRACKS_Launcher
spite_track_Launcher(self);
#endif
return self;
}
static inline void Launcher___release(Launcher* self) {
if (self == 0) return;
if (SPITE_PLAIN_COUNT_DOWN(self->header.ref_count) > 0) return;
Launcher___free(self);
}
void Launcher___free(Launcher* self) {
Build___release(self->build_);
#ifdef SPITE_TRACKS_Launcher
spite_untrack_Launcher(self);
#endif
#ifdef SPITE_WEAK_Launcher
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
static void spite_overflowed(const char* operation, const char* type, const char* symbol, int64_t left, int64_t right, const char* where) {
fflush(stdout);
fprintf(stderr, "spite: '%s' does not fit in %s (%lld %s %lld), at %s\n", operation, type, (long long)left, symbol, (long long)right, where);
exit(1);
}
TypedMemory__Console_Printable* spite_singleton_TypedMemory__Console_Printable(void) {
static TypedMemory__Console_Printable spite_object = { { 1, 149 } };
return &spite_object;
}
static List_Console_Printable* List_Console_Printable___framed(List_Console_Printable* self, int64_t items, int32_t count) {
List_Console_Printable___init(self);
self->header.ref_count = 2;
self->header.class_id = 148;
self->items_ = items;
self->item_count_ = count;
self->capacity_ = count;
return self;
}
void* spite_box_SpiteString(SpiteString value) {
SpiteBox_SpiteString* self = (SpiteBox_SpiteString*)SPITE_MALLOC(sizeof(SpiteBox_SpiteString));
self->header.ref_count = 1;
self->header.class_id = 0;
self->value = value;
return self;
}
void spite_string_box_release(void* self) {
SpiteBox_SpiteString* box = (SpiteBox_SpiteString*)self;
if (box->header.class_id < 0) return;
if (SPITE_COUNT_DOWN(box->header.ref_count) > 0) return;
SpiteString___release(box->value);
SPITE_FREE(box);
}
static inline SpiteTagged spite_tagged_SpiteLong(int64_t value) {
SpiteTagged tagged;
tagged.tag = 150;
tagged.plain = 1;
tagged.value.bits = 0;
memcpy(&tagged.value, &value, sizeof(value));
return tagged;
}
static void spite_narrowed(int64_t value, const char* from, const char* to, const char* where) {
fflush(stdout);
fprintf(stderr, "spite: %lld, %s, does not fit in %s, at %s\n", (long long)value, from, to, where);
exit(1);
}
static void spite_divided_by_zero(const char* operation, const char* where) {
fflush(stdout);
fprintf(stderr, "spite: '%s' divided by zero, at %s\n", operation, where);
exit(1);
}
static void spite_outside_list(const char* read, const char* where) {
fflush(stdout);
fprintf(stderr, "spite: '%s' is outside its list: a bound proves only the top of an index, and this one is below 0 or the list changed, at %s\n", read, where);
exit(1);
}
void Build___release(Build* self) { (void)self; }
void Clock___init(Clock* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->kernel_ = spite_foreign_library_1();
self->_ticks_per_second_ = SpiteInteger_to_long(1);
}
Clock* Clock___allocate(void) {
Clock* self = (Clock*)SPITE_MALLOC(sizeof(Clock));
self->header.ref_count = 1;
self->header.class_id = 13;
Clock___init(self);
#ifdef SPITE_TRACKS_Clock
spite_track_Clock(self);
#endif
return self;
}
Clock* Clock___make(void) {
Clock* self = Clock___allocate();
Clock_Clock(self);
return self;
}
void Clock___release(Clock* self) { (void)self; }
void Clock___destroy(Clock* self) {
if (self == 0) return;
Clock___discard(self);
}
void Clock___discard(Clock* self) {
if (self == 0) return;
Memory_Heap___release(self->heap_);
DynamicLibrary___release(self->kernel_);
#ifdef SPITE_TRACKS_Clock
spite_untrack_Clock(self);
#endif
#ifdef SPITE_WEAK_Clock
spite_weak_object_freed(self);
#endif
spite_singleton_free_later(self);
}
void Console___init(Console* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->library_ = spite_foreign_library_2();
self->input_ = SpiteInteger_to_long(0);
}
Console* Console___allocate(void) {
Console* self = (Console*)SPITE_MALLOC(sizeof(Console));
self->header.ref_count = 1;
self->header.class_id = 17;
Console___init(self);
#ifdef SPITE_TRACKS_Console
spite_track_Console(self);
#endif
return self;
}
Console* Console___make(void) {
Console* self = Console___allocate();
return self;
}
void Console___release(Console* self) { (void)self; }
void Console___destroy(Console* self) {
if (self == 0) return;
Console___discard(self);
}
void Console___discard(Console* self) {
if (self == 0) return;
Memory_Heap___release(self->heap_);
DynamicLibrary___release(self->library_);
#ifdef SPITE_TRACKS_Console
spite_untrack_Console(self);
#endif
#ifdef SPITE_WEAK_Console
spite_weak_object_freed(self);
#endif
spite_singleton_free_later(self);
}
static File* File___framed(File* self) {
self->header.ref_count = SPITE_FRAMED_COUNT;
self->header.class_id = 30;
return self;
}
static void File___unframe(File* self) {
SpiteString___release(self->path_);
Memory_Heap___release(self->heap_);
DynamicLibrary___release(self->library_);
DynamicLibrary___release(self->kernel_);
(void)self;
}
static File* File___make_into(File* self, SpiteString starting_path_) {
File___framed(self);
File___init(self);
File_File(self, starting_path_);
return self;
}
void DynamicLibrary___init(DynamicLibrary* self) {
self->file_name_ = spite_lit_1;
self->handle_ = SpiteInteger_to_long(0);
}
DynamicLibrary* DynamicLibrary___allocate(void) {
DynamicLibrary* self = (DynamicLibrary*)SPITE_MALLOC(sizeof(DynamicLibrary));
self->header.ref_count = 1;
self->header.class_id = 27;
DynamicLibrary___init(self);
#ifdef SPITE_TRACKS_DynamicLibrary
spite_track_DynamicLibrary(self);
#endif
return self;
}
DynamicLibrary* DynamicLibrary___make(SpiteString file_, SpiteString _naming_, SpiteString _header_) {
DynamicLibrary* self = DynamicLibrary___allocate();
DynamicLibrary_DynamicLibrary(self, file_, _naming_, _header_);
return self;
}
void DynamicLibrary___release(DynamicLibrary* self) { (void)self; }
void DynamicLibrary___destroy(DynamicLibrary* self) {
if (self == 0) return;
DynamicLibrary_drop(self);
DynamicLibrary___discard(self);
}
void DynamicLibrary___discard(DynamicLibrary* self) {
if (self == 0) return;
SpiteString___release(self->file_name_);
#ifdef SPITE_TRACKS_DynamicLibrary
spite_untrack_DynamicLibrary(self);
#endif
#ifdef SPITE_WEAK_DynamicLibrary
spite_weak_object_freed(self);
#endif
spite_singleton_free_later(self);
}
void File___init(File* self) {
self->path_ = spite_lit_2;
self->heap_ = spite_singleton_Memory_Heap();
self->library_ = spite_foreign_library_2();
self->kernel_ = spite_foreign_library_1();
}
static inline File* File___retain(File* self) {
if (self != 0) SPITE_COUNT_UP(self->header.ref_count);
return self;
}
static inline void File___release(File* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
File___free(self);
}
void File___free(File* self) {
SpiteString___release(self->path_);
Memory_Heap___release(self->heap_);
DynamicLibrary___release(self->library_);
DynamicLibrary___release(self->kernel_);
#ifdef SPITE_TRACKS_File
spite_untrack_File(self);
#endif
#ifdef SPITE_WEAK_File
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void Lock___init(Lock* self) {
self->_heap_ = spite_singleton_Memory_Heap();
self->_handle_ = SpiteInteger_to_long(0);
self->_kernel_ = spite_foreign_library_1();
}
Lock* Lock___allocate(void) {
Lock* self = (Lock*)SPITE_MALLOC(sizeof(Lock));
self->header.ref_count = 1;
self->header.class_id = 48;
Lock___init(self);
#ifdef SPITE_TRACKS_Lock
spite_track_Lock(self);
#endif
return self;
}
Lock* Lock___make(void) {
Lock* self = Lock___allocate();
Lock_Lock(self);
return self;
}
static inline void Lock___release(Lock* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Lock___free(self);
}
void Lock___free(Lock* self) {
Lock_drop(self);
Memory_Heap___release(self->_heap_);
DynamicLibrary___release(self->_kernel_);
#ifdef SPITE_TRACKS_Lock
spite_untrack_Lock(self);
#endif
#ifdef SPITE_WEAK_Lock
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void Scheduler___init(Scheduler* self) {
self->started_ = false;
self->scheduler_thread_ = SpiteInteger_to_long(0);
self->serving_ = false;
self->waits_begun_ = 0;
self->check_points_passed_ = 0;
self->answering_in_wait_ = false;
self->commands_ = 0;
self->reloads_ = 0;
self->paused_ = false;
self->_loops_ = ThreadLocal__SchedulerLoop___make();
self->_every_loop_ = List_SchedulerLoop___make();
self->_loops_lock_ = Lock___make();
self->kernel_ = spite_foreign_library_1();
}
Scheduler* Scheduler___allocate(void) {
Scheduler* self = (Scheduler*)SPITE_MALLOC(sizeof(Scheduler));
self->header.ref_count = 1;
self->header.class_id = 66;
Scheduler___init(self);
#ifdef SPITE_TRACKS_Scheduler
spite_track_Scheduler(self);
#endif
return self;
}
Scheduler* Scheduler___make(void) {
Scheduler* self = Scheduler___allocate();
return self;
}
void Scheduler___release(Scheduler* self) { (void)self; }
void Scheduler___destroy(Scheduler* self) {
if (self == 0) return;
Scheduler___discard(self);
}
void Scheduler___discard(Scheduler* self) {
if (self == 0) return;
Spite_Function___release(self->commands_);
Spite_Function___release(self->reloads_);
ThreadLocal__SchedulerLoop___release(self->_loops_);
List_SchedulerLoop___release(self->_every_loop_);
Lock___release(self->_loops_lock_);
DynamicLibrary___release(self->kernel_);
#ifdef SPITE_TRACKS_Scheduler
spite_untrack_Scheduler(self);
#endif
#ifdef SPITE_WEAK_Scheduler
spite_weak_object_freed(self);
#endif
spite_singleton_free_later(self);
}
void SchedulerLoop___init(SchedulerLoop* self) {
self->started_ = false;
self->thread_ = SpiteInteger_to_long(0);
self->tasks_ = List_Long___make();
self->deadlines_ = List_Long___make();
self->offloads_running_ = 0;
self->stepping_ = SpiteInteger_to_long(0);
self->waiting_frames_ = List_Long___make();
self->awaited_frames_ = List_Long___make();
self->resumes_when_asked_ = false;
self->unfinished_polls_with_no_frame_stepped_ = 0;
self->wake_ = SpiteInteger_to_long(0);
self->wake_signal_ = SpiteInteger_to_long(0);
}
SchedulerLoop* SchedulerLoop___allocate(void) {
SchedulerLoop* self = (SchedulerLoop*)SPITE_MALLOC(sizeof(SchedulerLoop));
self->header.ref_count = 1;
self->header.class_id = 67;
SchedulerLoop___init(self);
#ifdef SPITE_TRACKS_SchedulerLoop
spite_track_SchedulerLoop(self);
#endif
return self;
}
SchedulerLoop* SchedulerLoop___make(void) {
SchedulerLoop* self = SchedulerLoop___allocate();
return self;
}
static inline SchedulerLoop* SchedulerLoop___retain(SchedulerLoop* self) {
if (self != 0) SPITE_COUNT_UP(self->header.ref_count);
return self;
}
static inline void SchedulerLoop___release(SchedulerLoop* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
SchedulerLoop___free(self);
}
void SchedulerLoop___free(SchedulerLoop* self) {
List_Long___release(self->tasks_);
List_Long___release(self->deadlines_);
List_Long___release(self->waiting_frames_);
List_Long___release(self->awaited_frames_);
#ifdef SPITE_TRACKS_SchedulerLoop
spite_untrack_SchedulerLoop(self);
#endif
#ifdef SPITE_WEAK_SchedulerLoop
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
static int64_t spite_long_digits(char* digits, int64_t value) {
char reversed[24];
int64_t count = 0;
uint64_t rest = value < 0 ? (uint64_t)0 - (uint64_t)value : (uint64_t)value;
do { reversed[count] = (char)('0' + rest % 10); count = count + 1; rest = rest / 10; } while (rest != 0);
int64_t length = 0;
if (value < 0) { digits[0] = '-'; length = 1; }
while (count > 0) { count = count - 1; digits[length] = reversed[count]; length = length + 1; }
return length;
}
static SpiteStringBlock* spite_string_block(int64_t length) {
size_t size = ((size_t)length + 17 + 15) & ~(size_t)15;
SpiteStringBlock* block = (SpiteStringBlock*)SPITE_MALLOC(size);
block->header.ref_count = 1;
block->header.class_id = 0;
block->capacity = (int64_t)size - 17;
return block;
}
static SpiteString spite_string_held(SpiteStringBlock* block, int64_t length) {
block->bytes[length] = 0;
SpiteString held;
held._bytes_ = (int64_t)(intptr_t)block->bytes;
held._length_ = length | SPITE_STRING_HEAP;
return held;
}
SpiteString SpiteString___retain(SpiteString self) {
if (SPITE_STRING_IS_HEAP(self)) SPITE_COUNT_UP(SPITE_STRING_BLOCK(self)->header.ref_count);
return self;
}
void SpiteString___release(SpiteString self) {
if (!SPITE_STRING_IS_HEAP(self)) return;
SpiteStringBlock* block = SPITE_STRING_BLOCK(self);
if (SPITE_COUNT_DOWN(block->header.ref_count) > 0) return;
SPITE_FREE(block);
}
SpiteString spite_string_from_bytes(const char* bytes, int64_t length) {
if (length <= SPITE_STRING_INLINE) {
SpiteString made = { 0, 0 };
if (length > 0) memcpy(&made, bytes, (size_t)length);
((char*)&made)[15] = (char)(SPITE_STRING_INLINE - length);
return made;
}
SpiteStringBlock* block = spite_string_block(length);
memcpy(block->bytes, bytes, (size_t)length);
return spite_string_held(block, length);
}
SpiteString spite_string_join(int32_t count, const SpiteString* pieces) {
int64_t total = 0;
for (int32_t index = 0; index < count; index++) total += spite_string_length(pieces[index]);
SpiteString made = { 0, 0 };
SpiteStringBlock* block = 0;
char* at = (char*)&made;
if (total > SPITE_STRING_INLINE) { block = spite_string_block(total); at = block->bytes; }
for (int32_t index = 0; index < count; index++) {
int64_t length = spite_string_length(pieces[index]);
if (length > 0) memcpy(at, spite_string_bytes(&pieces[index]), (size_t)length);
at += length;
}
if (block != 0) return spite_string_held(block, total);
((char*)&made)[15] = (char)(SPITE_STRING_INLINE - total);
return made;
}
void ThreadSlot___init(ThreadSlot* self) {
self->_key_ = SpiteInteger_to_long((-(1)));
self->_kernel_ = spite_foreign_library_1();
}
ThreadSlot* ThreadSlot___allocate(void) {
ThreadSlot* self = (ThreadSlot*)SPITE_MALLOC(sizeof(ThreadSlot));
self->header.ref_count = 1;
self->header.class_id = 74;
ThreadSlot___init(self);
#ifdef SPITE_TRACKS_ThreadSlot
spite_track_ThreadSlot(self);
#endif
return self;
}
ThreadSlot* ThreadSlot___make(void) {
ThreadSlot* self = ThreadSlot___allocate();
ThreadSlot_ThreadSlot(self);
return self;
}
static inline void ThreadSlot___release(ThreadSlot* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
ThreadSlot___free(self);
}
void ThreadSlot___free(ThreadSlot* self) {
ThreadSlot_drop(self);
DynamicLibrary___release(self->_kernel_);
#ifdef SPITE_TRACKS_ThreadSlot
spite_untrack_ThreadSlot(self);
#endif
#ifdef SPITE_WEAK_ThreadSlot
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void Memory_Heap___release(Memory_Heap* self) { (void)self; }
static inline void Spite_Argument___release(Spite_Argument* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Spite_Argument___free(self);
}
void Spite_Argument___free(Spite_Argument* self) {
SpiteString___release(self->_name_);
Spite_Class___release(self->_class_);
#ifdef SPITE_TRACKS_Spite_Argument
spite_untrack_Spite_Argument(self);
#endif
#ifdef SPITE_WEAK_Spite_Argument
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
static inline void Spite_AttributeDeclaration___release(Spite_AttributeDeclaration* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Spite_AttributeDeclaration___free(self);
}
void Spite_AttributeDeclaration___free(Spite_AttributeDeclaration* self) {
SpiteString___release(self->_name_);
Spite_Class___release(self->_class_);
#ifdef SPITE_TRACKS_Spite_AttributeDeclaration
spite_untrack_Spite_AttributeDeclaration(self);
#endif
#ifdef SPITE_WEAK_Spite_AttributeDeclaration
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void Spite_Class___init(Spite_Class* self) {
self->_name_ = spite_symbol_2;
self->_namespace_ = 0;
self->_singleton_ = false;
self->_fits_vector_ = false;
self->_stateful_ = false;
self->_list_ = false;
self->_dictionary_ = false;
self->_optional_ = false;
self->_enum_ = false;
self->_source_folder_ = spite_lit_3;
self->_source_paths_ = 0;
self->_attributes_ = List_Spite_AttributeDeclaration___make();
self->_functions_ = List_Spite_Function___make();
self->_unbound_functions_ = 0;
}
Spite_Class* Spite_Class___allocate(void) {
Spite_Class* self = (Spite_Class*)SPITE_MALLOC(sizeof(Spite_Class));
self->header.ref_count = 1;
self->header.class_id = 100;
Spite_Class___init(self);
#ifdef SPITE_TRACKS_Spite_Class
spite_track_Spite_Class(self);
#endif
return self;
}
Spite_Class* Spite_Class___make(SpiteString starting_name_) {
Spite_Class* self = Spite_Class___allocate();
Spite_Class_Class(self, starting_name_);
return self;
}
static inline Spite_Class* Spite_Class___retain(Spite_Class* self) {
if (self != 0) SPITE_COUNT_UP(self->header.ref_count);
return self;
}
static inline void Spite_Class___release(Spite_Class* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Spite_Class___free(self);
}
void Spite_Class___free(Spite_Class* self) {
SpiteString___release(self->_name_);
Spite_Namespace___release(self->_namespace_);
SpiteString___release(self->_source_folder_);
List_String___release(self->_source_paths_);
List_Spite_AttributeDeclaration___release(self->_attributes_);
List_Spite_Function___release(self->_functions_);
List_Spite_Function___release(self->_unbound_functions_);
#ifdef SPITE_TRACKS_Spite_Class
spite_untrack_Spite_Class(self);
#endif
#ifdef SPITE_WEAK_Spite_Class
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
static void Spite_Function___init_constructed(Spite_Function* self) {
self->_name_ = spite_symbol_1;
self->_arguments_ = List_Spite_Argument___make();
self->_returns_ = 0;
self->_waits_ = false;
self->_returned_literal_ = spite_lit_4;
self->_has_returned_literal_ = false;
self->_accessed_ = spite_lit_5;
self->_accessed_names_ = 0;
self->_accessed_classes_ = 0;
self->spite_owner = 0;
self->spite_release_owner = 0;
self->spite_call = 0;
self->spite_typed_call = 0;
self->spite_text_call = 0;
self->spite_add_arguments = 0;
self->spite_arguments_lock = 0;
}
static Spite_Function* Spite_Function___allocate_constructed(void) {
Spite_Function* self = (Spite_Function*)SPITE_MALLOC(sizeof(Spite_Function));
self->header.ref_count = 1;
self->header.class_id = 104;
Spite_Function___init_constructed(self);
#ifdef SPITE_TRACKS_Spite_Function
spite_track_Spite_Function(self);
#endif
return self;
}
Spite_Function* Spite_Function___make(SpiteString starting_name_, Spite_Class* starting_returns_) {
Spite_Function* self = Spite_Function___allocate_constructed();
Spite_Function_Function(self, starting_name_, starting_returns_);
return self;
}
static inline Spite_Function* Spite_Function___retain(Spite_Function* self) {
if (self != 0) SPITE_COUNT_UP(self->header.ref_count);
return self;
}
static inline void Spite_Function___release(Spite_Function* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Spite_Function___free(self);
}
void Spite_Function___free(Spite_Function* self) {
SpiteString___release(self->_name_);
List_Spite_Argument___release(self->_arguments_);
Spite_Class___release(self->_returns_);
SpiteString___release(self->_returned_literal_);
SpiteString___release(self->_accessed_);
List_Symbol___release(self->_accessed_names_);
List_Spite_Class___release(self->_accessed_classes_);
if (self->spite_owner != 0 && self->spite_release_owner != 0) self->spite_release_owner(self->spite_owner);
#ifdef SPITE_TRACKS_Spite_Function
spite_untrack_Spite_Function(self);
#endif
#ifdef SPITE_WEAK_Spite_Function
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
static inline void Spite_Namespace___release(Spite_Namespace* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Spite_Namespace___free(self);
}
void Spite_Namespace___free(Spite_Namespace* self) {
SpiteString___release(self->_name_);
SpiteString___release(self->_name_with_namespaces_);
Spite_Namespace___release(self->_parent_);
List_Spite_Class___release(self->_classes_);
List_Spite_Namespace___release(self->_namespaces_);
List_Spite_Class___release(self->_enums_);
List_String___release(self->_source_paths_);
#ifdef SPITE_TRACKS_Spite_Namespace
spite_untrack_Spite_Namespace(self);
#endif
#ifdef SPITE_WEAK_Spite_Namespace
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void Naive___init(Naive* self) {
self->console_ = spite_singleton_Console();
self->clock_ = spite_singleton_Clock();
}
Naive* Naive___allocate(void) {
Naive* self = (Naive*)SPITE_MALLOC(sizeof(Naive));
self->header.ref_count = 1;
self->header.class_id = 109;
Naive___init(self);
#ifdef SPITE_TRACKS_Naive
spite_track_Naive(self);
#endif
return self;
}
static inline void Naive___release(Naive* self) {
if (self == 0) return;
if (SPITE_PLAIN_COUNT_DOWN(self->header.ref_count) > 0) return;
Naive___free(self);
}
void Naive___free(Naive* self) {
Console___release(self->console_);
Clock___release(self->clock_);
#ifdef SPITE_TRACKS_Naive
spite_untrack_Naive(self);
#endif
#ifdef SPITE_WEAK_Naive
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
static inline SpiteTagged spite_tagged_SpiteBoolean(bool value) {
SpiteTagged tagged;
tagged.tag = 165;
tagged.plain = 1;
tagged.value.bits = 0;
memcpy(&tagged.value, &value, sizeof(value));
return tagged;
}
void File_read___dropping_call(void* owner) {
SpiteString returned = File_read((File*)owner);
SpiteString___release(returned);
}
Spite_Function* spite_function_value_File_read(File* owner) {
Spite_Function* described = Spite_Function___make(spite_symbol_3, spite_class_object_Nullable_String());
described->spite_owner = File___retain(owner);
described->spite_release_owner = (void (*)(void*))File___release;
described->spite_call = (void (*)(void*))File_read___dropping_call;
described->spite_typed_call = (void*)File_read;
return described;
}
TypedMemory__Nullable_String* spite_singleton_TypedMemory__Nullable_String(void) {
static TypedMemory__Nullable_String spite_object = { { 1, 168 } };
return &spite_object;
}
void List_String___init(List_String* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->values_ = spite_singleton_TypedMemory__String();
self->items_ = ((int64_t)(0));
self->item_count_ = 0;
self->capacity_ = 0;
}
List_String* List_String___allocate(void) {
List_String* self = (List_String*)SPITE_MALLOC(sizeof(List_String));
self->header.ref_count = 1;
self->header.class_id = 110;
List_String___init(self);
#ifdef SPITE_TRACKS_List_String
spite_track_List_String(self);
#endif
return self;
}
List_String* List_String___make(void) {
List_String* self = List_String___allocate();
return self;
}
static inline void List_String___release(List_String* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
List_String___free(self);
}
void List_String___free(List_String* self) {
List_String_drop(self);
Memory_Heap___release(self->heap_);
spite_folded_TypedMemory__String___release(self->values_);
#ifdef SPITE_TRACKS_List_String
spite_untrack_List_String(self);
#endif
#ifdef SPITE_WEAK_List_String
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void List_Long___init(List_Long* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->values_ = spite_singleton_TypedMemory__Long();
self->items_ = ((int64_t)(0));
self->item_count_ = 0;
self->capacity_ = 0;
}
List_Long* List_Long___allocate(void) {
List_Long* self = (List_Long*)SPITE_MALLOC(sizeof(List_Long));
self->header.ref_count = 1;
self->header.class_id = 118;
List_Long___init(self);
#ifdef SPITE_TRACKS_List_Long
spite_track_List_Long(self);
#endif
return self;
}
List_Long* List_Long___make(void) {
List_Long* self = List_Long___allocate();
return self;
}
static inline List_Long* List_Long___retain(List_Long* self) {
if (self != 0) SPITE_COUNT_UP(self->header.ref_count);
return self;
}
static inline void List_Long___release(List_Long* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
List_Long___free(self);
}
void List_Long___free(List_Long* self) {
List_Long_drop(self);
Memory_Heap___release(self->heap_);
spite_folded_TypedMemory__Long___release(self->values_);
#ifdef SPITE_TRACKS_List_Long
spite_untrack_List_Long(self);
#endif
#ifdef SPITE_WEAK_List_Long
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void List_Memory_Address___init(List_Memory_Address* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->values_ = spite_singleton_TypedMemory__Memory_Address();
self->items_ = ((int64_t)(0));
self->item_count_ = 0;
self->capacity_ = 0;
}
List_Memory_Address* List_Memory_Address___allocate(void) {
List_Memory_Address* self = (List_Memory_Address*)SPITE_MALLOC(sizeof(List_Memory_Address));
self->header.ref_count = 1;
self->header.class_id = 124;
List_Memory_Address___init(self);
#ifdef SPITE_TRACKS_List_Memory_Address
spite_track_List_Memory_Address(self);
#endif
return self;
}
List_Memory_Address* List_Memory_Address___make(void) {
List_Memory_Address* self = List_Memory_Address___allocate();
return self;
}
static inline void List_Memory_Address___release(List_Memory_Address* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
List_Memory_Address___free(self);
}
void List_Memory_Address___free(List_Memory_Address* self) {
List_Memory_Address_drop(self);
Memory_Heap___release(self->heap_);
spite_folded_TypedMemory__Memory_Address___release(self->values_);
#ifdef SPITE_TRACKS_List_Memory_Address
spite_untrack_List_Memory_Address(self);
#endif
#ifdef SPITE_WEAK_List_Memory_Address
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void ThreadLocal__SchedulerLoop___init(ThreadLocal__SchedulerLoop* self) {
self->_slot_ = ThreadSlot___make();
self->_lock_ = Lock___make();
self->_heap_ = spite_singleton_Memory_Heap();
self->_values_ = spite_singleton_TypedMemory__SchedulerLoop();
self->_published_ = Memory_Heap_allocate(self->_heap_, SpiteInteger_to_long(8));
self->_count_ = 0;
self->_capacity_ = 0;
self->_retired_ = List_Memory_Address___make();
}
ThreadLocal__SchedulerLoop* ThreadLocal__SchedulerLoop___allocate(void) {
ThreadLocal__SchedulerLoop* self = (ThreadLocal__SchedulerLoop*)SPITE_MALLOC(sizeof(ThreadLocal__SchedulerLoop));
self->header.ref_count = 1;
self->header.class_id = 131;
ThreadLocal__SchedulerLoop___init(self);
#ifdef SPITE_TRACKS_ThreadLocal__SchedulerLoop
spite_track_ThreadLocal__SchedulerLoop(self);
#endif
return self;
}
ThreadLocal__SchedulerLoop* ThreadLocal__SchedulerLoop___make(void) {
ThreadLocal__SchedulerLoop* self = ThreadLocal__SchedulerLoop___allocate();
ThreadLocal__SchedulerLoop_ThreadLocal(self);
return self;
}
static inline void ThreadLocal__SchedulerLoop___release(ThreadLocal__SchedulerLoop* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
ThreadLocal__SchedulerLoop___free(self);
}
void ThreadLocal__SchedulerLoop___free(ThreadLocal__SchedulerLoop* self) {
ThreadLocal__SchedulerLoop_drop(self);
ThreadSlot___release(self->_slot_);
Lock___release(self->_lock_);
Memory_Heap___release(self->_heap_);
spite_folded_TypedMemory__SchedulerLoop___release(self->_values_);
List_Memory_Address___release(self->_retired_);
#ifdef SPITE_TRACKS_ThreadLocal__SchedulerLoop
spite_untrack_ThreadLocal__SchedulerLoop(self);
#endif
#ifdef SPITE_WEAK_ThreadLocal__SchedulerLoop
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void List_SchedulerLoop___init(List_SchedulerLoop* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->values_ = spite_singleton_TypedMemory__SchedulerLoop();
self->items_ = ((int64_t)(0));
self->item_count_ = 0;
self->capacity_ = 0;
}
List_SchedulerLoop* List_SchedulerLoop___allocate(void) {
List_SchedulerLoop* self = (List_SchedulerLoop*)SPITE_MALLOC(sizeof(List_SchedulerLoop));
self->header.ref_count = 1;
self->header.class_id = 133;
List_SchedulerLoop___init(self);
#ifdef SPITE_TRACKS_List_SchedulerLoop
spite_track_List_SchedulerLoop(self);
#endif
return self;
}
List_SchedulerLoop* List_SchedulerLoop___make(void) {
List_SchedulerLoop* self = List_SchedulerLoop___allocate();
return self;
}
static inline void List_SchedulerLoop___release(List_SchedulerLoop* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
List_SchedulerLoop___free(self);
}
void List_SchedulerLoop___free(List_SchedulerLoop* self) {
List_SchedulerLoop_drop(self);
Memory_Heap___release(self->heap_);
spite_folded_TypedMemory__SchedulerLoop___release(self->values_);
#ifdef SPITE_TRACKS_List_SchedulerLoop
spite_untrack_List_SchedulerLoop(self);
#endif
#ifdef SPITE_WEAK_List_SchedulerLoop
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void List_Spite_AttributeDeclaration___init(List_Spite_AttributeDeclaration* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->values_ = spite_singleton_TypedMemory__Spite_AttributeDeclaration();
self->items_ = ((int64_t)(0));
self->item_count_ = 0;
self->capacity_ = 0;
}
List_Spite_AttributeDeclaration* List_Spite_AttributeDeclaration___allocate(void) {
List_Spite_AttributeDeclaration* self = (List_Spite_AttributeDeclaration*)SPITE_MALLOC(sizeof(List_Spite_AttributeDeclaration));
self->header.ref_count = 1;
self->header.class_id = 138;
List_Spite_AttributeDeclaration___init(self);
#ifdef SPITE_TRACKS_List_Spite_AttributeDeclaration
spite_track_List_Spite_AttributeDeclaration(self);
#endif
return self;
}
List_Spite_AttributeDeclaration* List_Spite_AttributeDeclaration___make(void) {
List_Spite_AttributeDeclaration* self = List_Spite_AttributeDeclaration___allocate();
return self;
}
static inline void List_Spite_AttributeDeclaration___release(List_Spite_AttributeDeclaration* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
List_Spite_AttributeDeclaration___free(self);
}
void List_Spite_AttributeDeclaration___free(List_Spite_AttributeDeclaration* self) {
List_Spite_AttributeDeclaration_drop(self);
Memory_Heap___release(self->heap_);
spite_folded_TypedMemory__Spite_AttributeDeclaration___release(self->values_);
#ifdef SPITE_TRACKS_List_Spite_AttributeDeclaration
spite_untrack_List_Spite_AttributeDeclaration(self);
#endif
#ifdef SPITE_WEAK_List_Spite_AttributeDeclaration
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void List_Spite_Function___init(List_Spite_Function* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->values_ = spite_singleton_TypedMemory__Spite_Function();
self->items_ = ((int64_t)(0));
self->item_count_ = 0;
self->capacity_ = 0;
}
List_Spite_Function* List_Spite_Function___allocate(void) {
List_Spite_Function* self = (List_Spite_Function*)SPITE_MALLOC(sizeof(List_Spite_Function));
self->header.ref_count = 1;
self->header.class_id = 140;
List_Spite_Function___init(self);
#ifdef SPITE_TRACKS_List_Spite_Function
spite_track_List_Spite_Function(self);
#endif
return self;
}
List_Spite_Function* List_Spite_Function___make(void) {
List_Spite_Function* self = List_Spite_Function___allocate();
return self;
}
static inline void List_Spite_Function___release(List_Spite_Function* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
List_Spite_Function___free(self);
}
void List_Spite_Function___free(List_Spite_Function* self) {
List_Spite_Function_drop(self);
Memory_Heap___release(self->heap_);
spite_folded_TypedMemory__Spite_Function___release(self->values_);
#ifdef SPITE_TRACKS_List_Spite_Function
spite_untrack_List_Spite_Function(self);
#endif
#ifdef SPITE_WEAK_List_Spite_Function
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void List_Spite_Argument___init(List_Spite_Argument* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->values_ = spite_singleton_TypedMemory__Spite_Argument();
self->items_ = ((int64_t)(0));
self->item_count_ = 0;
self->capacity_ = 0;
}
List_Spite_Argument* List_Spite_Argument___allocate(void) {
List_Spite_Argument* self = (List_Spite_Argument*)SPITE_MALLOC(sizeof(List_Spite_Argument));
self->header.ref_count = 1;
self->header.class_id = 142;
List_Spite_Argument___init(self);
#ifdef SPITE_TRACKS_List_Spite_Argument
spite_track_List_Spite_Argument(self);
#endif
return self;
}
List_Spite_Argument* List_Spite_Argument___make(void) {
List_Spite_Argument* self = List_Spite_Argument___allocate();
return self;
}
static inline void List_Spite_Argument___release(List_Spite_Argument* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
List_Spite_Argument___free(self);
}
void List_Spite_Argument___free(List_Spite_Argument* self) {
List_Spite_Argument_drop(self);
Memory_Heap___release(self->heap_);
spite_folded_TypedMemory__Spite_Argument___release(self->values_);
#ifdef SPITE_TRACKS_List_Spite_Argument
spite_untrack_List_Spite_Argument(self);
#endif
#ifdef SPITE_WEAK_List_Spite_Argument
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
static inline void List_Spite_Class___release(List_Spite_Class* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
List_Spite_Class___free(self);
}
void List_Spite_Class___free(List_Spite_Class* self) {
List_Spite_Class_drop(self);
Memory_Heap___release(self->heap_);
spite_folded_TypedMemory__Spite_Class___release(self->values_);
#ifdef SPITE_TRACKS_List_Spite_Class
spite_untrack_List_Spite_Class(self);
#endif
#ifdef SPITE_WEAK_List_Spite_Class
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
static inline void List_Spite_Namespace___release(List_Spite_Namespace* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
List_Spite_Namespace___free(self);
}
void List_Spite_Namespace___free(List_Spite_Namespace* self) {
List_Spite_Namespace_drop(self);
Memory_Heap___release(self->heap_);
spite_folded_TypedMemory__Spite_Namespace___release(self->values_);
#ifdef SPITE_TRACKS_List_Spite_Namespace
spite_untrack_List_Spite_Namespace(self);
#endif
#ifdef SPITE_WEAK_List_Spite_Namespace
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void List_Console_Printable___init(List_Console_Printable* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->values_ = spite_singleton_TypedMemory__Console_Printable();
self->items_ = ((int64_t)(0));
self->item_count_ = 0;
self->capacity_ = 0;
}
static inline List_Console_Printable* List_Console_Printable___retain(List_Console_Printable* self) {
if (self != 0) SPITE_PLAIN_COUNT_UP(self->header.ref_count);
return self;
}
static inline void List_Console_Printable___release(List_Console_Printable* self) {
if (self == 0) return;
if (SPITE_PLAIN_COUNT_DOWN(self->header.ref_count) > 0) return;
List_Console_Printable___free(self);
}
void List_Console_Printable___free(List_Console_Printable* self) {
List_Console_Printable_drop(self);
Memory_Heap___release(self->heap_);
spite_folded_TypedMemory__Console_Printable___release(self->values_);
#ifdef SPITE_TRACKS_List_Console_Printable
spite_untrack_List_Console_Printable(self);
#endif
#ifdef SPITE_WEAK_List_Console_Printable
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
static inline void List_Symbol___release(List_Symbol* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
List_Symbol___free(self);
}
void List_Symbol___free(List_Symbol* self) {
List_Symbol_drop(self);
Memory_Heap___release(self->heap_);
spite_folded_TypedMemory__Symbol___release(self->values_);
#ifdef SPITE_TRACKS_List_Symbol
spite_untrack_List_Symbol(self);
#endif
#ifdef SPITE_WEAK_List_Symbol
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void Concurrent__Nullable_String___init(Concurrent__Nullable_String* self) {
self->_scheduler_ = spite_singleton_Scheduler();
self->_work_ = 0;
self->_results_ = List_Nullable_String___make();
self->_frame_ = SpiteInteger_to_long(0);
self->_finished_ = true;
}
Concurrent__Nullable_String* Concurrent__Nullable_String___allocate(void) {
Concurrent__Nullable_String* self = (Concurrent__Nullable_String*)SPITE_MALLOC(sizeof(Concurrent__Nullable_String));
self->header.ref_count = 1;
self->header.class_id = 166;
Concurrent__Nullable_String___init(self);
#ifdef SPITE_TRACKS_Concurrent__Nullable_String
spite_track_Concurrent__Nullable_String(self);
#endif
return self;
}
Concurrent__Nullable_String* Concurrent__Nullable_String___make(Spite_Function* starting_work_) {
Concurrent__Nullable_String* self = Concurrent__Nullable_String___allocate();
Concurrent__Nullable_String_Concurrent(self, starting_work_);
return self;
}
static inline void Concurrent__Nullable_String___release(Concurrent__Nullable_String* self) {
if (self == 0) return;
if (SPITE_PLAIN_COUNT_DOWN(self->header.ref_count) > 0) return;
Concurrent__Nullable_String___free(self);
}
void Concurrent__Nullable_String___free(Concurrent__Nullable_String* self) {
Concurrent__Nullable_String_drop(self);
Scheduler___release(self->_scheduler_);
Spite_Function___release(self->_work_);
List_Nullable_String___release(self->_results_);
#ifdef SPITE_TRACKS_Concurrent__Nullable_String
spite_untrack_Concurrent__Nullable_String(self);
#endif
#ifdef SPITE_WEAK_Concurrent__Nullable_String
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void List_Nullable_String___init(List_Nullable_String* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->values_ = spite_singleton_TypedMemory__Nullable_String();
self->items_ = ((int64_t)(0));
self->item_count_ = 0;
self->capacity_ = 0;
}
List_Nullable_String* List_Nullable_String___allocate(void) {
List_Nullable_String* self = (List_Nullable_String*)SPITE_MALLOC(sizeof(List_Nullable_String));
self->header.ref_count = 1;
self->header.class_id = 167;
List_Nullable_String___init(self);
#ifdef SPITE_TRACKS_List_Nullable_String
spite_track_List_Nullable_String(self);
#endif
return self;
}
List_Nullable_String* List_Nullable_String___make(void) {
List_Nullable_String* self = List_Nullable_String___allocate();
return self;
}
static inline void List_Nullable_String___release(List_Nullable_String* self) {
if (self == 0) return;
if (SPITE_PLAIN_COUNT_DOWN(self->header.ref_count) > 0) return;
List_Nullable_String___free(self);
}
void List_Nullable_String___free(List_Nullable_String* self) {
List_Nullable_String_drop(self);
Memory_Heap___release(self->heap_);
spite_folded_TypedMemory__Nullable_String___release(self->values_);
#ifdef SPITE_TRACKS_List_Nullable_String
spite_untrack_List_Nullable_String(self);
#endif
#ifdef SPITE_WEAK_List_Nullable_String
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
Spite_Class* spite_class_object_Nullable_String(void) {
if (SPITE_SINGLETON_FOUND(spite_class_object_Nullable_String_ready)) return Spite_Class___retain(spite_class_object_Nullable_String_cache);
spite_described_enter();
if (spite_class_object_Nullable_String_cache == 0) {
spite_class_object_Nullable_String_cache = Spite_Class___make(spite_symbol_4);
}
spite_described_leave(&spite_class_object_Nullable_String_ready);
return Spite_Class___retain(spite_class_object_Nullable_String_cache);
}
void Console_Printable___release(Console_Printable self) {
if (self.plain != 0 || self.value.object == 0) return;
if (((self).tag == 0) && ((self).plain == 0)) { spite_string_box_release(self.value.object); return; }
}
SpiteString Console_Printable___call_to_string(Console_Printable self) {
if (((self).tag == 0) && ((self).plain == 0)) return SpiteString_to_string((((SpiteBox_SpiteString*)(self).value.object)->value));
if ((self).tag == 150) return SpiteLong_to_string(SPITE_TAGGED_VALUE(self, int64_t));
if ((self).tag == 165) return SpiteBoolean_to_string(SPITE_TAGGED_VALUE(self, bool));
fputs("spite.crash\tPrintable.to_string was called on a value of a class it was not compiled for\n", stderr);
abort();
}
static int32_t spite_foreign_library_1_lock = 0;
DynamicLibrary* spite_foreign_library_1(void) {
DynamicLibrary* found = SPITE_SINGLETON_FOUND(spite_foreign_library_1_cache);
if (found != 0) return found;
SPITE_LOCK(spite_foreign_library_1_lock);
if (spite_foreign_library_1_cache == 0) {
DynamicLibrary* made = DynamicLibrary___make(((SpiteString)SPITE_STATIC_STRING("kernel32.dll", 12)), spite_symbol_5, ((SpiteString)SPITE_STATIC_STRING("", 0)));
spite_foreign_library_1_tracked = spite_singleton_tracked();
(void)&DynamicLibrary_find_symbol;
spite_foreign_1_0 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("QueryPerformanceFrequency", 25)), ((SpiteString)SPITE_STATIC_STRING("Clock.Clock", 11)));
spite_foreign_1_1 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("QueryPerformanceCounter", 23)), ((SpiteString)SPITE_STATIC_STRING("Clock.elapsed_nanoseconds", 25)));











spite_foreign_1_22 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("CloseHandle", 11)), ((SpiteString)SPITE_STATIC_STRING("File.map", 8)));

spite_foreign_1_24 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("CreateEventA", 12)), ((SpiteString)SPITE_STATIC_STRING("FileSystemWatcher.open_watch", 28)));









spite_foreign_1_34 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("InitializeSRWLock", 17)), ((SpiteString)SPITE_STATIC_STRING("Lock.create_lock", 16)));
spite_foreign_1_35 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("AcquireSRWLockExclusive", 23)), ((SpiteString)SPITE_STATIC_STRING("Lock.acquire", 12)));
spite_foreign_1_36 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("ReleaseSRWLockExclusive", 23)), ((SpiteString)SPITE_STATIC_STRING("Lock.release_lock", 17)));



spite_foreign_1_45 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("CreateThread", 12)), ((SpiteString)SPITE_STATIC_STRING("ReadEvaluatePrintLoop.start_thread", 34)));
spite_foreign_1_46 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("WaitForSingleObject", 19)), ((SpiteString)SPITE_STATIC_STRING("ReadEvaluatePrintLoop.join_thread", 33)));
spite_foreign_1_47 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("GetCurrentThreadId", 18)), ((SpiteString)SPITE_STATIC_STRING("Scheduler.current_thread", 24)));
spite_foreign_1_48 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("SetEvent", 8)), ((SpiteString)SPITE_STATIC_STRING("Scheduler.signal_event", 22)));
spite_foreign_1_49 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("GetTickCount64", 14)), ((SpiteString)SPITE_STATIC_STRING("Scheduler.clock", 15)));





spite_foreign_1_72 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("TlsAlloc", 8)), ((SpiteString)SPITE_STATIC_STRING("ThreadSlot.create_key", 21)));
spite_foreign_1_73 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("TlsGetValue", 11)), ((SpiteString)SPITE_STATIC_STRING("ThreadSlot.read_key", 19)));
spite_foreign_1_74 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("TlsSetValue", 11)), ((SpiteString)SPITE_STATIC_STRING("ThreadSlot.write_key", 20)));
spite_foreign_1_75 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("TlsFree", 7)), ((SpiteString)SPITE_STATIC_STRING("ThreadSlot.delete_key", 21)));

SPITE_SINGLETON_PUBLISH(spite_foreign_library_1_cache, made);
}
SPITE_UNLOCK(spite_foreign_library_1_lock);
return spite_foreign_library_1_cache;
}
static int32_t spite_foreign_library_2_lock = 0;
DynamicLibrary* spite_foreign_library_2(void) {
DynamicLibrary* found = SPITE_SINGLETON_FOUND(spite_foreign_library_2_cache);
if (found != 0) return found;
SPITE_LOCK(spite_foreign_library_2_lock);
if (spite_foreign_library_2_cache == 0) {
DynamicLibrary* made = DynamicLibrary___make(((SpiteString)SPITE_STATIC_STRING("ucrtbase.dll", 12)), spite_symbol_5, ((SpiteString)SPITE_STATIC_STRING("", 0)));
spite_foreign_library_2_tracked = spite_singleton_tracked();
(void)&DynamicLibrary_find_symbol;


spite_foreign_2_11 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("fopen", 5)), ((SpiteString)SPITE_STATIC_STRING("File.open_file", 14)));
spite_foreign_2_12 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("fclose", 6)), ((SpiteString)SPITE_STATIC_STRING("File.close_file", 15)));
spite_foreign_2_13 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("_fseeki64", 9)), ((SpiteString)SPITE_STATIC_STRING("File.seek", 9)));
spite_foreign_2_14 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("_ftelli64", 9)), ((SpiteString)SPITE_STATIC_STRING("File.tell", 9)));
spite_foreign_2_15 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("fread", 5)), ((SpiteString)SPITE_STATIC_STRING("File.read_into", 14)));
spite_foreign_2_16 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("fwrite", 6)), ((SpiteString)SPITE_STATIC_STRING("File.write_from", 15)));
spite_foreign_2_18 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("remove", 6)), ((SpiteString)SPITE_STATIC_STRING("File.remove", 11)));






SPITE_SINGLETON_PUBLISH(spite_foreign_library_2_cache, made);
}
SPITE_UNLOCK(spite_foreign_library_2_lock);
return spite_foreign_library_2_cache;
}
static void* spite_offload_thread(void* call) {
spite_fault_thread();
SpiteOffload* offload = (SpiteOffload*)call;
offload->perform(call);
Scheduler_offload_done((Scheduler*)offload->scheduler, (int64_t)(intptr_t)call);
return 0;
}
static void File_read_into___perform(void* raw) {
File_read_into___call* call = (File_read_into___call*)raw;
call->result = File_read_into___waiting(call->self, call->buffer_, call->bytes_, call->handle_);
}
static void File_write_text___perform(void* raw) {
File_write_text___call* call = (File_write_text___call*)raw;
call->result = File_write_text___waiting(call->self, call->text_, call->handle_);
}
static SPITE_THREAD_LOCAL const char* spite_singletons_making[64];
static SPITE_THREAD_LOCAL int32_t spite_singletons_making_count = 0;
void spite_singleton_check_circle(const char* name) {
int32_t kept = spite_singletons_making_count < 64 ? spite_singletons_making_count : 64;
int32_t first = 0;
while (first < kept && strcmp(spite_singletons_making[first], name) != 0) first = first + 1;
if (first == kept) return;
fflush(stdout);
if (first + 1 == kept) {
fprintf(stderr, "spite: singleton '%s' is asked for while it is being made: a singleton cannot reach itself as it initialises\n", name);
exit(1);
}
fprintf(stderr, "spite: singleton '%s'", name);
for (int32_t step = first + 1; step < kept; step = step + 1) {
fprintf(stderr, step == first + 1 ? " binds '%s'" : ", which binds '%s'", spite_singletons_making[step]);
}
fprintf(stderr, ", which binds '%s'", name);
fputs(": singletons initialise each other in a circle\n", stderr);
exit(1);
}
void spite_singleton_making(const char* name) {
if (spite_singletons_making_count < 64) spite_singletons_making[spite_singletons_making_count] = name;
spite_singletons_making_count = spite_singletons_making_count + 1;
}
void spite_singleton_made(void) {
spite_singletons_making_count = spite_singletons_making_count - 1;
}
static void (**spite_singleton_teardowns)(void) = 0;
static int32_t spite_singleton_total = 0;
static int32_t spite_singleton_capacity = 0;
static int32_t spite_singleton_list_lock = 0;
bool spite_singleton_tracked(void) {
return true;
}
void spite_singleton_created(void (*teardown)(void)) {
if (!spite_singleton_tracked()) return;
SPITE_LOCK(spite_singleton_list_lock);
if (spite_singleton_total == spite_singleton_capacity) {
spite_singleton_capacity = spite_singleton_capacity == 0 ? 16 : spite_singleton_capacity * 2;
spite_singleton_teardowns = (void (**)(void))realloc(spite_singleton_teardowns, (size_t)spite_singleton_capacity * sizeof(void (*)(void)));
}
spite_singleton_teardowns[spite_singleton_total] = teardown;
spite_singleton_total = spite_singleton_total + 1;
SPITE_UNLOCK(spite_singleton_list_lock);
}
void spite_singleton_used_after_exit(const char* name) {
fflush(stdout);
fprintf(stderr, "spite: a drop() at exit used the singleton %s after it was destroyed: singletons are destroyed in reverse order of when they were made, so keep %s in an attribute of the singleton whose drop() uses it\n", name, name);
exit(1);
}
static bool spite_singletons_ending = false;
static void** spite_singletons_freed = 0;
static int32_t spite_singletons_freed_total = 0;
static int32_t spite_singletons_freed_capacity = 0;
void spite_singleton_free_later(void* object) {
if (!spite_singletons_ending) { SPITE_FREE(object); return; }
if (spite_singletons_freed_total == spite_singletons_freed_capacity) {
spite_singletons_freed_capacity = spite_singletons_freed_capacity == 0 ? 16 : spite_singletons_freed_capacity * 2;
spite_singletons_freed = (void**)realloc(spite_singletons_freed, (size_t)spite_singletons_freed_capacity * sizeof(void*));
}
spite_singletons_freed[spite_singletons_freed_total] = object;
spite_singletons_freed_total = spite_singletons_freed_total + 1;
}
void spite_singletons_destroy(void) {
fflush(stdout);
spite_singletons_ending = true;
while (spite_singleton_total > 0) {
spite_singleton_total = spite_singleton_total - 1;
spite_singleton_teardowns[spite_singleton_total]();
}
free(spite_singleton_teardowns);
for (int32_t index = 0; index < spite_singletons_freed_total; index = index + 1) SPITE_FREE(spite_singletons_freed[index]);
free(spite_singletons_freed);
spite_singletons_ending = false;
}
static void spite_enter_skipped(void* guard) { (void)guard; }
void Launcher_Launcher(Launcher* self) {
(void)0;
(void)0;
({ Naive* spite_entry_instance = Naive___allocate(); Naive_Naive(spite_entry_instance); Naive___release(spite_entry_instance); (void)0; });
}
SpiteString SpiteBoolean_to_string(bool self) {
if ((self)) {
SpiteString spite_temp_4 = spite_lit_6;
return spite_temp_4;
}
SpiteString spite_temp_5 = spite_lit_7;
return spite_temp_5;
}
void Clock_Clock(Clock* self) {
int64_t spite_temp_6[1];
int64_t spite_temp_7 = SpiteInteger_to_long(8);
int64_t frequency_ = spite_temp_7 <= 8 ? (int64_t)(intptr_t)spite_temp_6 : Memory_Heap_allocate(self->heap_, spite_temp_7);
(void)(({ spite_last_foreign_call = "QueryPerformanceFrequency\tlibrary=kernel32.dll\tfrom=library/windows/clock.spite:6"; int32_t spite_temp_8 = ((int32_t (*)(int64_t))spite_foreign_1_0)((int64_t)(frequency_));  int32_t spite_foreign_result = spite_temp_8;  (void)spite_foreign_result; spite_temp_8; }));
self->_ticks_per_second_ = SpiteMemory_Address_read_long(frequency_, SpiteInteger_to_long(0));
if (frequency_ != (int64_t)(intptr_t)spite_temp_6) Memory_Heap_free(self->heap_, frequency_);
}
int64_t Clock_elapsed_nanoseconds(Clock* self) {
int64_t spite_temp_9[1];
int64_t spite_temp_10 = SpiteInteger_to_long(8);
int64_t counter_ = spite_temp_10 <= 8 ? (int64_t)(intptr_t)spite_temp_9 : Memory_Heap_allocate(self->heap_, spite_temp_10);
(void)(({ spite_last_foreign_call = "QueryPerformanceCounter\tlibrary=kernel32.dll\tfrom=library/windows/clock.spite:13"; int32_t spite_temp_11 = ((int32_t (*)(int64_t))spite_foreign_1_1)((int64_t)(counter_));  int32_t spite_foreign_result = spite_temp_11;  (void)spite_foreign_result; spite_temp_11; }));
int64_t ticks_ = SpiteMemory_Address_read_long(counter_, SpiteInteger_to_long(0));
if (counter_ != (int64_t)(intptr_t)spite_temp_9) Memory_Heap_free(self->heap_, counter_);
int64_t spite_temp_12 = ({ int64_t spite_temp_13 = ({ int64_t spite_temp_14 = ({ int64_t spite_temp_15 = ticks_; int64_t spite_temp_16 = self->_ticks_per_second_; if (spite_temp_16 == 0) spite_divided_by_zero("ticks / _ticks_per_second", spite_site_1()); int64_t spite_temp_17 = 0; if (__builtin_expect(spite_temp_16 == -1 && __builtin_sub_overflow((int64_t)0, spite_temp_15, &spite_temp_17), 0)) spite_overflowed("ticks / _ticks_per_second", "a Long", "/", (int64_t)spite_temp_15, (int64_t)spite_temp_16, spite_site_1()); (int64_t)(spite_temp_16 == -1 ? spite_temp_17 : spite_temp_15 / spite_temp_16); }); int64_t spite_temp_18 = SpiteInteger_to_long(1000000000); int64_t spite_temp_19; if (__builtin_expect(__builtin_mul_overflow(spite_temp_14, spite_temp_18, &spite_temp_19), 0)) spite_overflowed("ticks / _ticks_per_second * 1000000000", "a Long", "*", (int64_t)spite_temp_14, (int64_t)spite_temp_18, spite_site_1()); spite_temp_19; }); int64_t spite_temp_20 = ({ int64_t spite_temp_21 = ({ int64_t spite_temp_22 = ({ int64_t spite_temp_23 = ticks_; int64_t spite_temp_24 = self->_ticks_per_second_; if (spite_temp_24 == 0) spite_divided_by_zero("ticks % _ticks_per_second", spite_site_1()); (int64_t)(spite_temp_24 == -1 ? (int64_t)0 : spite_temp_23 % spite_temp_24); }); int64_t spite_temp_25 = SpiteInteger_to_long(1000000000); int64_t spite_temp_26; if (__builtin_expect(__builtin_mul_overflow(spite_temp_22, spite_temp_25, &spite_temp_26), 0)) spite_overflowed("ticks % _ticks_per_second * 1000000000", "a Long", "*", (int64_t)spite_temp_22, (int64_t)spite_temp_25, spite_site_1()); spite_temp_26; }); int64_t spite_temp_27 = self->_ticks_per_second_; if (spite_temp_27 == 0) spite_divided_by_zero("ticks % _ticks_per_second * 1000000000 / _ticks_per_second", spite_site_1()); int64_t spite_temp_28 = 0; if (__builtin_expect(spite_temp_27 == -1 && __builtin_sub_overflow((int64_t)0, spite_temp_21, &spite_temp_28), 0)) spite_overflowed("ticks % _ticks_per_second * 1000000000 / _ticks_per_second", "a Long", "/", (int64_t)spite_temp_21, (int64_t)spite_temp_27, spite_site_1()); (int64_t)(spite_temp_27 == -1 ? spite_temp_28 : spite_temp_21 / spite_temp_27); }); int64_t spite_temp_29; if (__builtin_expect(__builtin_add_overflow(spite_temp_13, spite_temp_20, &spite_temp_29), 0)) spite_overflowed("ticks / _ticks_per_second * 1000000000 + ticks % _ticks_per_second * 1000000000 / _ticks_per_second", "a Long", "+", (int64_t)spite_temp_13, (int64_t)spite_temp_20, spite_site_1()); spite_temp_29; });
return spite_temp_12;
}
void Console_print(Console* self, List_Console_Printable* values_) {
Console__write_values(self, List_Console_Printable___retain(values_), Console_Stream_output);
Console__write_output(self, spite_lit_8);
Console__flush(self);
List_Console_Printable___release(values_);
}
void Console_error(Console* self, List_Console_Printable* values_) {
Console__flush(self);
Console__write_values(self, List_Console_Printable___retain(values_), Console_Stream_error);
Console__write_error(self, spite_lit_9);
Console__flush(self);
List_Console_Printable___release(values_);
}
void Console__write_values(Console* self, List_Console_Printable* values_, Console_Stream stream_) {
int32_t index_ = 0;
while (((index_ < spite_folded_List_Console_Printable_count(values_)))) {
if (((index_ > 0))) {
Console__write_to(self, spite_lit_10, stream_);
}
SpiteString text_ = ({ Console_Printable spite_temp_30 = ({ Console_Printable spite_temp_31 = List_Console_Printable_get_at(values_, index_); if (__builtin_expect(!(SPITE_TAGGED_PRESENT(spite_temp_31)), 0)) spite_outside_list("values[index]", spite_site_2()); spite_temp_31; }); SpiteString spite_temp_32 = Console_Printable___call_to_string(spite_temp_30); Console_Printable___release(spite_temp_30); spite_temp_32; });
Console__write_to(self, SpiteString___retain(text_), stream_);
index_ = (index_ + 1);
SpiteString___release(text_);
}
List_Console_Printable___release(values_);
}
void Console__write_to(Console* self, SpiteString text_, Console_Stream stream_) {
if (((stream_ == Console_Stream_error))) {
Console__write_error(self, SpiteString___retain(text_));
}
else {
Console__write_output(self, SpiteString___retain(text_));
}
SpiteString___release(text_);
}
void Console__write_output(Console* self, SpiteString text_) {
#ifdef SPITE_THREADS
spite_hold_text(&spite_held_output, spite_string_bytes(&text_), (size_t)spite_string_length(text_));
#else
fwrite(spite_string_bytes(&text_), 1, (size_t)spite_string_length(text_), stdout);
#endif
SpiteString___release(text_);
}
void Console__write_error(Console* self, SpiteString text_) {
#ifdef SPITE_THREADS
spite_hold_text(&spite_held_error, spite_string_bytes(&text_), (size_t)spite_string_length(text_));
#else
fwrite(spite_string_bytes(&text_), 1, (size_t)spite_string_length(text_), stderr);
#endif
SpiteString___release(text_);
}
void Console__flush(Console* self) {
#ifdef SPITE_THREADS
spite_write_held(&spite_held_output, stdout);
spite_write_held(&spite_held_error, stderr);
#endif
fflush(stdout);
fflush(stderr);
}
void DynamicLibrary_DynamicLibrary(DynamicLibrary* self, SpiteString file_, SpiteString _naming_, SpiteString _header_) {
SpiteString spite_temp_33 = SpiteString___retain(file_);
SpiteString___release(self->file_name_);
self->file_name_ = spite_temp_33;
self->handle_ = DynamicLibrary_open_library(self, SpiteString___retain(file_));
SpiteString___release(_header_);
SpiteString___release(_naming_);
SpiteString___release(file_);
}
void DynamicLibrary_drop(DynamicLibrary* self) {
DynamicLibrary_close_library(self, self->handle_);
}
int64_t DynamicLibrary_open_library(DynamicLibrary* self, SpiteString file_) {
#ifdef _WIN32
void* opened = (void*)LoadLibraryA(spite_string_bytes(&file_));
#else
void* opened = dlopen(spite_string_bytes(&file_), RTLD_NOW);
#endif
if (opened == 0) {
fflush(stdout);
fprintf(stderr, "spite: could not open the library '%s'\n", spite_string_bytes(&file_));
exit(1);
}
SpiteString___release(file_);
return (int64_t)(intptr_t)opened;
}
int64_t DynamicLibrary_find_symbol(DynamicLibrary* self, SpiteString name_, SpiteString wanted_by_) {
#ifdef _WIN32
void* found = (void*)GetProcAddress((HMODULE)(intptr_t)self->handle_, spite_string_bytes(&name_));
#else
void* found = dlsym((void*)(intptr_t)self->handle_, spite_string_bytes(&name_));
#endif
if (found == 0) {
fflush(stdout);
fprintf(stderr, "spite: the library '%s' has no '%s', which %s calls\n", spite_string_bytes(&self->file_name_), spite_string_bytes(&name_), spite_string_bytes(&wanted_by_));
exit(1);
}
SpiteString___release(name_);
SpiteString___release(wanted_by_);
return (int64_t)(intptr_t)found;
}
void DynamicLibrary_close_library(DynamicLibrary* self, int64_t opened_) {
if (opened_ == 0) return;
#ifdef _WIN32
FreeLibrary((HMODULE)(intptr_t)opened_);
#else
dlclose((void*)(intptr_t)opened_);
#endif
}
void File_File(File* self, SpiteString starting_path_) {
SpiteString spite_temp_34 = SpiteString___retain(starting_path_);
SpiteString___release(self->path_);
self->path_ = spite_temp_34;
SpiteString___release(starting_path_);
}
SpiteString File_read(File* self) {
int64_t handle_ = File_open_file(self, spite_lit_11);
if (!(((handle_ != SpiteInteger_to_long(0))))) {
return SPITE_STRING_NULL;
}
File_seek(self, handle_, SpiteInteger_to_long(0), 2);
int64_t length_ = File_tell(self, handle_);
File_seek(self, handle_, SpiteInteger_to_long(0), 0);
int64_t buffer_ = Memory_Heap_allocate(self->heap_, ({ int64_t spite_temp_35 = length_; int64_t spite_temp_36 = SpiteInteger_to_long(1); int64_t spite_temp_37; if (__builtin_expect(__builtin_add_overflow(spite_temp_35, spite_temp_36, &spite_temp_37), 0)) spite_overflowed("length + 1", "a Long", "+", (int64_t)spite_temp_35, (int64_t)spite_temp_36, spite_site_3()); spite_temp_37; }));
int64_t got_ = File_read_into(self, SpiteMemory_Address_to_long(buffer_), length_, handle_);
File_close_file(self, handle_);
SpiteString content_ = SpiteMemory_Address_text(buffer_, got_);
Memory_Heap_free(self->heap_, buffer_);
SpiteString spite_temp_38 = SpiteString___retain(content_);
SpiteString___release(content_);
return spite_temp_38;
}
bool File_write(File* self, SpiteString text_) {
bool spite_temp_39 = File_put(self, SpiteString___retain(text_), spite_lit_12);
SpiteString___release(text_);
return spite_temp_39;
}
bool File_put(File* self, SpiteString text_, SpiteString mode_) {
int64_t handle_ = File_open_file(self, SpiteString___retain(mode_));
if (((handle_ == SpiteInteger_to_long(0)))) {
bool spite_temp_40 = false;
SpiteString___release(mode_);
SpiteString___release(text_);
return spite_temp_40;
}
int64_t written_ = File_write_text(self, SpiteString___retain(text_), handle_);
File_close_file(self, handle_);
bool spite_temp_41 = (written_ == SpiteInteger_to_long(SpiteString_length(text_)));
SpiteString___release(mode_);
SpiteString___release(text_);
return spite_temp_41;
}
int64_t File_open_file(File* self, SpiteString mode_) {
int64_t spite_temp_42 = ({ SpiteString spite_temp_43 = self->path_; SpiteString spite_temp_44 = mode_; spite_last_foreign_call = "fopen\tlibrary=ucrtbase.dll\tfrom=library/windows/file.spite:5"; int64_t spite_temp_45 = ((int64_t (*)(const char*, const char*))spite_foreign_2_11)(spite_string_bytes(&spite_temp_43), spite_string_bytes(&spite_temp_44));  int64_t spite_foreign_result = spite_temp_45;  (void)spite_foreign_result; spite_temp_45; });
SpiteString___release(mode_);
return spite_temp_42;
}
void File_close_file(File* self, int64_t handle_) {
(void)(({ spite_last_foreign_call = "fclose\tlibrary=ucrtbase.dll\tfrom=library/windows/file.spite:9"; int32_t spite_temp_46 = ((int32_t (*)(int64_t))spite_foreign_2_12)((int64_t)(handle_));  int32_t spite_foreign_result = spite_temp_46;  (void)spite_foreign_result; spite_temp_46; }));
}
void File_seek(File* self, int64_t handle_, int64_t offset_, int32_t origin_) {
(void)(({ spite_last_foreign_call = "_fseeki64\tlibrary=ucrtbase.dll\tfrom=library/windows/file.spite:13"; int32_t spite_temp_47 = ((int32_t (*)(int64_t, int64_t, int64_t))spite_foreign_2_13)((int64_t)(handle_), (int64_t)(offset_), (int64_t)(origin_));  int32_t spite_foreign_result = spite_temp_47;  (void)spite_foreign_result; spite_temp_47; }));
}
int64_t File_tell(File* self, int64_t handle_) {
int64_t spite_temp_48 = ({ spite_last_foreign_call = "_ftelli64\tlibrary=ucrtbase.dll\tfrom=library/windows/file.spite:17"; int64_t spite_temp_49 = ((int64_t (*)(int64_t))spite_foreign_2_14)((int64_t)(handle_));  int64_t spite_foreign_result = spite_temp_49;  (void)spite_foreign_result; spite_temp_49; });
return spite_temp_48;
}
int64_t File_read_into___waiting(File* self, int64_t buffer_, int64_t bytes_, int64_t handle_) {
int64_t spite_temp_50 = ({ spite_last_foreign_call = "fread\tlibrary=ucrtbase.dll\tfrom=library/windows/file.spite:21"; int64_t spite_temp_51 = ((int64_t (*)(int64_t, int64_t, int64_t, int64_t))spite_foreign_2_15)((int64_t)(buffer_), (int64_t)(1), (int64_t)(bytes_), (int64_t)(handle_));  int64_t spite_foreign_result = spite_temp_51;  (void)spite_foreign_result; spite_temp_51; });
return spite_temp_50;
}
int64_t File_write_text___waiting(File* self, SpiteString text_, int64_t handle_) {
int32_t text_length_ = SpiteString_length(text_);
int64_t spite_temp_52 = ({ SpiteString spite_temp_53 = text_; spite_last_foreign_call = "fwrite\tlibrary=ucrtbase.dll\tfrom=library/windows/file.spite:30"; int64_t spite_temp_54 = ((int64_t (*)(const char*, int64_t, int64_t, int64_t))spite_foreign_2_16)(spite_string_bytes(&spite_temp_53), (int64_t)(1), (int64_t)(text_length_), (int64_t)(handle_));  int64_t spite_foreign_result = spite_temp_54;  (void)spite_foreign_result; spite_temp_54; });
SpiteString___release(text_);
return spite_temp_52;
}
bool File_remove(File* self) {
bool spite_temp_55 = (({ SpiteString spite_temp_56 = self->path_; spite_last_foreign_call = "remove\tlibrary=ucrtbase.dll\tfrom=library/windows/file.spite:44"; int32_t spite_temp_57 = ((int32_t (*)(const char*))spite_foreign_2_18)(spite_string_bytes(&spite_temp_56));  int32_t spite_foreign_result = spite_temp_57;  (void)spite_foreign_result; spite_temp_57; }) == 0);
return spite_temp_55;
}
SpiteString SpiteInteger_to_string(int32_t self) {
int64_t wide_ = SpiteInteger_to_long(self);
SpiteString spite_temp_58 = SpiteLong_to_string(wide_);
return spite_temp_58;
}
void Lock_Lock(Lock* self) {
self->_handle_ = Lock_create_lock(self);
}
void Lock_lock(Lock* self) {
Lock_acquire(self, self->_handle_);
}
void Lock_unlock(Lock* self) {
Lock_release_lock(self, self->_handle_);
}
void Lock_drop(Lock* self) {
if (!(((self->_handle_ != SpiteInteger_to_long(0))))) {
return;
}
Lock_destroy_lock(self, self->_handle_);
}
int64_t Lock_create_lock(Lock* self) {
int64_t created_ = Memory_Heap_allocate(self->_heap_, SpiteInteger_to_long(8));
(void)(({ spite_last_foreign_call = "InitializeSRWLock\tlibrary=kernel32.dll\tfrom=library/windows/lock.spite:5"; int32_t spite_temp_59 = ((int32_t (*)(int64_t))spite_foreign_1_34)((int64_t)(created_));  int32_t spite_foreign_result = spite_temp_59;  (void)spite_foreign_result; spite_temp_59; }));
int64_t spite_temp_60 = SpiteMemory_Address_to_long(created_);
return spite_temp_60;
}
void Lock_acquire(Lock* self, int64_t held_) {
(void)(({ spite_last_foreign_call = "AcquireSRWLockExclusive\tlibrary=kernel32.dll\tfrom=library/windows/lock.spite:10"; int32_t spite_temp_61 = ((int32_t (*)(int64_t))spite_foreign_1_35)((int64_t)(held_));  int32_t spite_foreign_result = spite_temp_61;  (void)spite_foreign_result; spite_temp_61; }));
}
void Lock_release_lock(Lock* self, int64_t held_) {
(void)(({ spite_last_foreign_call = "ReleaseSRWLockExclusive\tlibrary=kernel32.dll\tfrom=library/windows/lock.spite:14"; int32_t spite_temp_62 = ((int32_t (*)(int64_t))spite_foreign_1_36)((int64_t)(held_));  int32_t spite_foreign_result = spite_temp_62;  (void)spite_foreign_result; spite_temp_62; }));
}
void Lock_destroy_lock(Lock* self, int64_t held_) {
Memory_Heap_free(self->_heap_, ((int64_t)(held_)));
}
SpiteString SpiteLong_to_string(int64_t self) {
if (((self == SpiteInteger_to_long(0)))) {
SpiteString spite_temp_63 = spite_lit_13;
return spite_temp_63;
}
Memory_Heap* heap_ = spite_singleton_Memory_Heap();
int64_t buffer_bytes_ = SpiteInteger_to_long(24);
int64_t spite_temp_64[32];
int64_t spite_temp_65 = buffer_bytes_;
int64_t address_ = spite_temp_65 <= 256 ? (int64_t)(intptr_t)spite_temp_64 : Memory_Heap_allocate(heap_, spite_temp_65);
int64_t position_ = buffer_bytes_;
int64_t rest_ = self;
while (((rest_ != SpiteInteger_to_long(0)))) {
int64_t digit_ = (rest_ % SpiteInteger_to_long(10));
if (((digit_ < SpiteInteger_to_long(0)))) {
digit_ = (-(digit_));
}
position_ = ({ int64_t spite_temp_66 = position_; int64_t spite_temp_67 = SpiteInteger_to_long(1); int64_t spite_temp_68; if (__builtin_expect(__builtin_sub_overflow(spite_temp_66, spite_temp_67, &spite_temp_68), 0)) spite_overflowed("position - 1", "a Long", "-", (int64_t)spite_temp_66, (int64_t)spite_temp_67, spite_site_4()); spite_temp_68; });
SpiteMemory_Address_write_byte(address_, position_, ({ int64_t spite_temp_69 = (digit_ + SpiteInteger_to_long(48)); if (__builtin_expect(spite_temp_69 < 0 || spite_temp_69 > UINT8_MAX, 0)) spite_narrowed((int64_t)spite_temp_69, "a Long", "a Byte", spite_site_5()); (uint8_t)spite_temp_69; }));
rest_ = (rest_ / SpiteInteger_to_long(10));
}
if (((self < SpiteInteger_to_long(0)))) {
position_ = ({ int64_t spite_temp_70 = position_; int64_t spite_temp_71 = SpiteInteger_to_long(1); int64_t spite_temp_72; if (__builtin_expect(__builtin_sub_overflow(spite_temp_70, spite_temp_71, &spite_temp_72), 0)) spite_overflowed("position - 1", "a Long", "-", (int64_t)spite_temp_70, (int64_t)spite_temp_71, spite_site_6()); spite_temp_72; });
SpiteMemory_Address_write_byte(address_, position_, SpiteInteger_to_byte(45));
}
int64_t first_digit_ = (address_ + ((int64_t)(position_)));
SpiteString text_ = SpiteMemory_Address_text(first_digit_, ({ int64_t spite_temp_73 = buffer_bytes_; int64_t spite_temp_74 = position_; int64_t spite_temp_75; if (__builtin_expect(__builtin_sub_overflow(spite_temp_73, spite_temp_74, &spite_temp_75), 0)) spite_overflowed("buffer_bytes - position", "a Long", "-", (int64_t)spite_temp_73, (int64_t)spite_temp_74, spite_site_7()); spite_temp_75; }));
if (address_ != (int64_t)(intptr_t)spite_temp_64) Memory_Heap_free(heap_, address_);
SpiteString spite_temp_76 = SpiteString___retain(text_);
SpiteString___release(text_);
Memory_Heap___release(heap_);
return spite_temp_76;
}
SchedulerLoop* Scheduler_loop(Scheduler* self) {
SchedulerLoop* found_ = ThreadLocal__SchedulerLoop_get(self->_loops_);
if (((found_) != 0)) {
SchedulerLoop* spite_temp_77 = SchedulerLoop___retain(found_);
SchedulerLoop___release(found_);
return spite_temp_77;
}
SchedulerLoop* made_ = SchedulerLoop___make();
(made_)->thread_ = Scheduler_current_thread(self);
ThreadLocal__SchedulerLoop_set(self->_loops_, SchedulerLoop___retain(made_));
Lock_lock(self->_loops_lock_);
List_SchedulerLoop_append(self->_every_loop_, SchedulerLoop___retain(made_));
Lock_unlock(self->_loops_lock_);
SchedulerLoop* spite_temp_78 = SchedulerLoop___retain(made_);
SchedulerLoop___release(made_);
SchedulerLoop___release(found_);
return spite_temp_78;
}
void Scheduler_start(Scheduler* self) {
SchedulerLoop* here_ = Scheduler_loop(self);
if (((!((here_)->started_)))) {
(here_)->started_ = true;
Scheduler_create_event(self, SchedulerLoop___retain(here_));
}
if (((!(self->started_)))) {
self->started_ = true;
self->scheduler_thread_ = (here_)->thread_;
}
SchedulerLoop___release(here_);
}
bool Scheduler_on_main_thread(Scheduler* self) {
bool spite_temp_79 = ((self->started_) && ((Scheduler_current_thread(self) == self->scheduler_thread_)));
return spite_temp_79;
}
bool Scheduler_waits_here(Scheduler* self) {
SchedulerLoop* here_ = Scheduler_loop(self);
bool serves_here_ = ((self->serving_) && (Scheduler_on_main_thread(self)));
if (!(!(((!((here_)->started_)))) && !((self->paused_)) && !((((((List_Long_is_empty((here_)->tasks_)) || ((here_)->resumes_when_asked_))) && ((!(serves_here_)))))))) {
bool spite_temp_80 = false;
SchedulerLoop___release(here_);
return spite_temp_80;
}
bool spite_temp_81 = true;
SchedulerLoop___release(here_);
return spite_temp_81;
}
void Scheduler_begin(Scheduler* self, int64_t frame_) {
Scheduler_start(self);
if (((!(Scheduler_run_frame(self, ((int64_t)(frame_))))))) {
SchedulerLoop* here_ = Scheduler_loop(self);
List_Long_append((here_)->tasks_, frame_);
SchedulerLoop___release(here_);
}
}
bool Scheduler_run_frame(Scheduler* self, int64_t frame_) {
SchedulerLoop* here_ = Scheduler_loop(self);
(here_)->unfinished_polls_with_no_frame_stepped_ = 0;
SpiteMemory_Address_write_integer(frame_, SpiteInteger_to_long(12), 1);
int64_t outer_ = (here_)->stepping_;
(here_)->stepping_ = SpiteMemory_Address_to_long(frame_);
bool done_ = Scheduler_step_frame(self, SpiteMemory_Address_to_long(frame_));
(here_)->stepping_ = outer_;
if ((done_)) {
SpiteMemory_Address_write_integer(frame_, SpiteInteger_to_long(12), 2);
Scheduler_forget_wait(self, SpiteMemory_Address_to_long(frame_));
Scheduler_release_work(self, SpiteMemory_Address_to_long(frame_));
}
else {
SpiteMemory_Address_write_integer(frame_, SpiteInteger_to_long(12), 0);
}
bool spite_temp_82 = done_;
SchedulerLoop___release(here_);
return spite_temp_82;
}
bool Scheduler_frame_done(Scheduler* self, int64_t frame_) {
bool spite_temp_83 = (SpiteMemory_Address_read_integer(frame_, SpiteInteger_to_long(12)) == 2);
return spite_temp_83;
}
void Scheduler_forget_wait(Scheduler* self, int64_t frame_) {
SchedulerLoop* here_ = Scheduler_loop(self);
int32_t index_ = 0;
while (((index_ < spite_folded_List_Long_count((here_)->waiting_frames_)))) {
if (((({ Nullable_Long spite_temp_84 = List_Long_get_at((here_)->waiting_frames_, index_); if (__builtin_expect(!spite_temp_84.has_value, 0)) spite_outside_list("here.waiting_frames[index]", spite_site_8()); spite_temp_84.value; }) == frame_))) {
List_Long_remove_at((here_)->waiting_frames_, index_);
List_Long_remove_at((here_)->awaited_frames_, index_);
SchedulerLoop___release(here_);
return;
}
index_ = (index_ + 1);
}
SchedulerLoop___release(here_);
}
int32_t Scheduler_step_ready(Scheduler* self) {
SchedulerLoop* here_ = Scheduler_loop(self);
int32_t finished_ = 0;
int32_t index_ = 0;
while (((index_ < spite_folded_List_Long_count((here_)->tasks_)))) {
int64_t frame_ = ((int64_t)(({ Nullable_Long spite_temp_85 = List_Long_get_at((here_)->tasks_, index_); if (__builtin_expect(!spite_temp_85.has_value, 0)) spite_outside_list("here.tasks[index]", spite_site_9()); spite_temp_85.value; })));
if ((((SpiteMemory_Address_read_integer(frame_, SpiteInteger_to_long(12)) == 0))) && ((Scheduler_run_frame(self, frame_)))) {
Scheduler_remove_one(self, List_Long___retain((here_)->tasks_), SpiteMemory_Address_to_long(frame_));
finished_ = ({ int32_t spite_temp_86 = finished_; int32_t spite_temp_87 = 1; int32_t spite_temp_88; if (__builtin_expect(__builtin_add_overflow(spite_temp_86, spite_temp_87, &spite_temp_88), 0)) spite_overflowed("finished + 1", "an Integer", "+", (int64_t)spite_temp_86, (int64_t)spite_temp_87, spite_site_10()); spite_temp_88; });
}
else {
index_ = (index_ + 1);
}
}
int32_t spite_temp_89 = finished_;
SchedulerLoop___release(here_);
return spite_temp_89;
}
bool Scheduler_wait_for(Scheduler* self, int64_t frame_) {
if ((Scheduler_runs_below(self, ((int64_t)(frame_))))) {
bool spite_temp_90 = false;
return spite_temp_90;
}
Scheduler_begin_wait(self);
while (((!(Scheduler_frame_done(self, ((int64_t)(frame_))))))) {
Scheduler_idle_stepping(self, true);
}
bool spite_temp_91 = true;
return spite_temp_91;
}
bool Scheduler_runs_below(Scheduler* self, int64_t frame_) {
SchedulerLoop* here_ = Scheduler_loop(self);
bool spite_temp_92 = (((SpiteMemory_Address_read_integer(frame_, SpiteInteger_to_long(12)) == 1)) && ((here_)->started_));
SchedulerLoop___release(here_);
return spite_temp_92;
}
void Scheduler_remove_one(Scheduler* self, List_Long* values_, int64_t value_) {
int32_t index_ = 0;
while (((((index_ < spite_folded_List_Long_count(values_))) && ((({ int32_t spite_temp_93 = index_; if (__builtin_expect(spite_temp_93 >= (values_)->item_count_, 0)) spite_outside_list("values[index]", spite_site_11()); ((int64_t*)(intptr_t)(values_)->items_)[spite_temp_93]; }) != value_))))) {
index_ = (index_ + 1);
}
if (((index_ < spite_folded_List_Long_count(values_)))) {
List_Long_remove_at(values_, index_);
}
List_Long___release(values_);
}
bool Scheduler_offload(Scheduler* self, int64_t call_, int64_t entry_) {
Scheduler_begin_wait(self);
int64_t thread_ = Scheduler_offload_start(self, call_, entry_);
while (((!(Scheduler_offload_over(self, call_, thread_))))) {
Scheduler_idle(self);
}
bool spite_temp_94 = true;
return spite_temp_94;
}
int64_t Scheduler_offload_start(Scheduler* self, int64_t call_, int64_t entry_) {
int64_t thread_ = Scheduler_start_thread(self, entry_, SpiteMemory_Address_to_long(call_));
if (!(((thread_ != SpiteInteger_to_long(0))))) {
spite_failed_1(thread_, call_, entry_, self);
}
SchedulerLoop* here_ = Scheduler_loop(self);
(here_)->offloads_running_ = ({ int32_t spite_temp_95 = (here_)->offloads_running_; int32_t spite_temp_96 = 1; int32_t spite_temp_97; if (__builtin_expect(__builtin_add_overflow(spite_temp_95, spite_temp_96, &spite_temp_97), 0)) spite_overflowed("here.offloads_running + 1", "an Integer", "+", (int64_t)spite_temp_95, (int64_t)spite_temp_96, spite_site_13()); spite_temp_97; });
int64_t spite_temp_98 = thread_;
SchedulerLoop___release(here_);
return spite_temp_98;
}
static SPITE_CRASH_REPORT void spite_failed_1(int64_t thread_, int64_t call_, int64_t entry_, Scheduler* self) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_12(), stderr);
fputs("\tthread=", stderr);
{ SpiteString spite_temp_99 = SpiteLong_to_string(thread_); fwrite(spite_string_bytes(&spite_temp_99), 1, (size_t)spite_string_length(spite_temp_99), stderr); SpiteString___release(spite_temp_99); }
fputs("\tcall=", stderr);
{ SpiteString spite_temp_100 = SpiteMemory_Address_to_string(call_); spite_crash_text(spite_string_bytes(&spite_temp_100), spite_string_length(spite_temp_100)); SpiteString___release(spite_temp_100); }
fputs("\tentry=", stderr);
{ SpiteString spite_temp_101 = SpiteLong_to_string(entry_); spite_crash_text(spite_string_bytes(&spite_temp_101), spite_string_length(spite_temp_101)); SpiteString___release(spite_temp_101); }
fputs("\tstarted=", stderr);
{ SpiteString spite_temp_102 = SpiteBoolean_to_string(self->started_); spite_crash_text(spite_string_bytes(&spite_temp_102), spite_string_length(spite_temp_102)); SpiteString___release(spite_temp_102); }
fputs("\tscheduler_thread=", stderr);
{ SpiteString spite_temp_103 = SpiteLong_to_string(self->scheduler_thread_); spite_crash_text(spite_string_bytes(&spite_temp_103), spite_string_length(spite_temp_103)); SpiteString___release(spite_temp_103); }
fputs("\tserving=", stderr);
{ SpiteString spite_temp_104 = SpiteBoolean_to_string(self->serving_); spite_crash_text(spite_string_bytes(&spite_temp_104), spite_string_length(spite_temp_104)); SpiteString___release(spite_temp_104); }
fputs("\twaits_begun=", stderr);
{ SpiteString spite_temp_105 = SpiteInteger_to_string(self->waits_begun_); spite_crash_text(spite_string_bytes(&spite_temp_105), spite_string_length(spite_temp_105)); SpiteString___release(spite_temp_105); }
fputs("\tcheck_points_passed=", stderr);
{ SpiteString spite_temp_106 = SpiteInteger_to_string(self->check_points_passed_); spite_crash_text(spite_string_bytes(&spite_temp_106), spite_string_length(spite_temp_106)); SpiteString___release(spite_temp_106); }
fputs("\tanswering_in_wait=", stderr);
{ SpiteString spite_temp_107 = SpiteBoolean_to_string(self->answering_in_wait_); spite_crash_text(spite_string_bytes(&spite_temp_107), spite_string_length(spite_temp_107)); SpiteString___release(spite_temp_107); }
fputs("\tpaused=", stderr);
{ SpiteString spite_temp_108 = SpiteBoolean_to_string(self->paused_); spite_crash_text(spite_string_bytes(&spite_temp_108), spite_string_length(spite_temp_108)); SpiteString___release(spite_temp_108); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
bool Scheduler_offload_over(Scheduler* self, int64_t call_, int64_t thread_) {
if (((SpiteMemory_Address_read_long_atomically(call_, SpiteInteger_to_long(0)) != SpiteInteger_to_long(1)))) {
bool spite_temp_109 = false;
return spite_temp_109;
}
Scheduler_end_thread(self, thread_);
SchedulerLoop* here_ = Scheduler_loop(self);
(here_)->offloads_running_ = ({ int32_t spite_temp_110 = (here_)->offloads_running_; int32_t spite_temp_111 = 1; int32_t spite_temp_112; if (__builtin_expect(__builtin_sub_overflow(spite_temp_110, spite_temp_111, &spite_temp_112), 0)) spite_overflowed("here.offloads_running - 1", "an Integer", "-", (int64_t)spite_temp_110, (int64_t)spite_temp_111, spite_site_14()); spite_temp_112; });
bool spite_temp_113 = true;
SchedulerLoop___release(here_);
return spite_temp_113;
}
void Scheduler_offload_done(Scheduler* self, int64_t call_) {
SpiteMemory_Address_write_long_atomically(call_, SpiteInteger_to_long(0), SpiteInteger_to_long(1));
Scheduler_signal(self);
}
void Scheduler_begin_wait(Scheduler* self) {
if (((self->serving_)) && ((Scheduler_on_main_thread(self)))) {
self->waits_begun_ = ({ int32_t spite_temp_114 = self->waits_begun_; int32_t spite_temp_115 = 1; int32_t spite_temp_116; if (__builtin_expect(__builtin_add_overflow(spite_temp_114, spite_temp_115, &spite_temp_116), 0)) spite_overflowed("waits_begun + 1", "an Integer", "+", (int64_t)spite_temp_114, (int64_t)spite_temp_115, spite_site_15()); spite_temp_116; });
}
}
void Scheduler_signal(Scheduler* self) {
Scheduler___release(self);
Lock_lock(self->_loops_lock_);
int32_t index_ = 0;
while (((index_ < spite_folded_List_SchedulerLoop_count(self->_every_loop_)))) {
SchedulerLoop* each_loop_ = ({ SchedulerLoop* spite_temp_117 = List_SchedulerLoop_get_at(self->_every_loop_, index_); if (__builtin_expect(!(((spite_temp_117) != 0)), 0)) spite_outside_list("_every_loop[index]", spite_site_16()); spite_temp_117; });
if (((each_loop_)->started_)) {
Scheduler_signal_event(self, SchedulerLoop___retain(each_loop_));
}
index_ = (index_ + 1);
SchedulerLoop___release(each_loop_);
}
Lock_unlock(self->_loops_lock_);
}
void Scheduler_idle(Scheduler* self) {
SchedulerLoop* here_ = Scheduler_loop(self);
Scheduler_idle_stepping(self, (!((here_)->resumes_when_asked_)));
SchedulerLoop___release(here_);
}
void Scheduler_idle_stepping(Scheduler* self, bool steps_) {
bool main_ = Scheduler_on_main_thread(self);
if ((((self->commands_) != 0)) && ((main_))) {
self->answering_in_wait_ = true;
({ Spite_Function* spite_temp_118 = self->commands_; ((void (*)(void*))spite_temp_118->spite_typed_call)(spite_temp_118->spite_owner); });
self->answering_in_wait_ = false;
}
if ((((self->reloads_) != 0)) && ((main_))) {
({ Spite_Function* spite_temp_119 = self->reloads_; ((void (*)(void*))spite_temp_119->spite_typed_call)(spite_temp_119->spite_owner); });
}
int32_t finished_ = 0;
if ((steps_)) {
finished_ = Scheduler_step_ready(self);
}
if (((finished_ == 0))) {
SchedulerLoop* here_ = Scheduler_loop(self);
int32_t wait_limit_ = Scheduler_timeout(self);
bool spite_crash_reached_1 = false;
if (!(((((((wait_limit_ >= 0)) || (((here_)->offloads_running_ > 0)))) || (((self->serving_) && ((spite_crash_reached_1 = true, (main_))))))))) {
spite_failed_2(wait_limit_, here_, self, spite_crash_reached_1, main_, steps_, finished_);
}
Scheduler_wait_event(self, SchedulerLoop___retain(here_), wait_limit_);
SchedulerLoop___release(here_);
}
}
static SPITE_CRASH_REPORT void spite_failed_2(int32_t wait_limit_, SchedulerLoop* here_, Scheduler* self, bool spite_crash_reached_1, bool main_, bool steps_, int32_t finished_) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_17(), stderr);
fputs("\twait_limit=", stderr);
{ SpiteString spite_temp_120 = SpiteInteger_to_string(wait_limit_); fwrite(spite_string_bytes(&spite_temp_120), 1, (size_t)spite_string_length(spite_temp_120), stderr); SpiteString___release(spite_temp_120); }
fputs("\there.offloads_running=", stderr);
{ SpiteString spite_temp_121 = SpiteInteger_to_string((here_)->offloads_running_); fwrite(spite_string_bytes(&spite_temp_121), 1, (size_t)spite_string_length(spite_temp_121), stderr); SpiteString___release(spite_temp_121); }
fputs("\tserving=", stderr);
{ SpiteString spite_temp_122 = SpiteBoolean_to_string(self->serving_); fwrite(spite_string_bytes(&spite_temp_122), 1, (size_t)spite_string_length(spite_temp_122), stderr); SpiteString___release(spite_temp_122); }
if (spite_crash_reached_1) {
fputs("\tmain=", stderr);
{ SpiteString spite_temp_123 = SpiteBoolean_to_string(main_); fwrite(spite_string_bytes(&spite_temp_123), 1, (size_t)spite_string_length(spite_temp_123), stderr); SpiteString___release(spite_temp_123); }
}
fputs("\tsteps=", stderr);
{ SpiteString spite_temp_124 = SpiteBoolean_to_string(steps_); spite_crash_text(spite_string_bytes(&spite_temp_124), spite_string_length(spite_temp_124)); SpiteString___release(spite_temp_124); }
fputs("\tfinished=", stderr);
{ SpiteString spite_temp_125 = SpiteInteger_to_string(finished_); spite_crash_text(spite_string_bytes(&spite_temp_125), spite_string_length(spite_temp_125)); SpiteString___release(spite_temp_125); }
fputs("\tstarted=", stderr);
{ SpiteString spite_temp_126 = SpiteBoolean_to_string(self->started_); spite_crash_text(spite_string_bytes(&spite_temp_126), spite_string_length(spite_temp_126)); SpiteString___release(spite_temp_126); }
fputs("\tscheduler_thread=", stderr);
{ SpiteString spite_temp_127 = SpiteLong_to_string(self->scheduler_thread_); spite_crash_text(spite_string_bytes(&spite_temp_127), spite_string_length(spite_temp_127)); SpiteString___release(spite_temp_127); }
fputs("\twaits_begun=", stderr);
{ SpiteString spite_temp_128 = SpiteInteger_to_string(self->waits_begun_); spite_crash_text(spite_string_bytes(&spite_temp_128), spite_string_length(spite_temp_128)); SpiteString___release(spite_temp_128); }
fputs("\tcheck_points_passed=", stderr);
{ SpiteString spite_temp_129 = SpiteInteger_to_string(self->check_points_passed_); spite_crash_text(spite_string_bytes(&spite_temp_129), spite_string_length(spite_temp_129)); SpiteString___release(spite_temp_129); }
fputs("\tanswering_in_wait=", stderr);
{ SpiteString spite_temp_130 = SpiteBoolean_to_string(self->answering_in_wait_); spite_crash_text(spite_string_bytes(&spite_temp_130), spite_string_length(spite_temp_130)); SpiteString___release(spite_temp_130); }
fputs("\tpaused=", stderr);
{ SpiteString spite_temp_131 = SpiteBoolean_to_string(self->paused_); spite_crash_text(spite_string_bytes(&spite_temp_131), spite_string_length(spite_temp_131)); SpiteString___release(spite_temp_131); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
int32_t Scheduler_timeout(Scheduler* self) {
SchedulerLoop* here_ = Scheduler_loop(self);
if ((List_Long_is_empty((here_)->deadlines_))) {
int32_t spite_temp_132 = (-(1));
SchedulerLoop___release(here_);
return spite_temp_132;
}
int64_t now_ = Scheduler_clock(self);
int64_t earliest_ = ({ Nullable_Long spite_temp_133 = List_Long_get_at((here_)->deadlines_, 0); if (__builtin_expect(!spite_temp_133.has_value, 0)) spite_outside_list("here.deadlines[0]", spite_site_18()); spite_temp_133.value; });
int32_t index_ = 1;
while (((index_ < spite_folded_List_Long_count((here_)->deadlines_)))) {
int64_t deadline_ = ({ Nullable_Long spite_temp_134 = List_Long_get_at((here_)->deadlines_, index_); if (__builtin_expect(!spite_temp_134.has_value, 0)) spite_outside_list("here.deadlines[index]", spite_site_19()); spite_temp_134.value; });
if (((deadline_ < earliest_))) {
earliest_ = deadline_;
}
index_ = (index_ + 1);
}
if (((earliest_ <= now_))) {
int32_t spite_temp_135 = 0;
SchedulerLoop___release(here_);
return spite_temp_135;
}
int32_t spite_temp_136 = ({ int64_t spite_temp_137 = ({ int64_t spite_temp_138 = earliest_; int64_t spite_temp_139 = now_; int64_t spite_temp_140; if (__builtin_expect(__builtin_sub_overflow(spite_temp_138, spite_temp_139, &spite_temp_140), 0)) spite_overflowed("earliest - now", "a Long", "-", (int64_t)spite_temp_138, (int64_t)spite_temp_139, spite_site_20()); spite_temp_140; }); if (__builtin_expect(spite_temp_137 < INT32_MIN || spite_temp_137 > INT32_MAX, 0)) spite_narrowed((int64_t)spite_temp_137, "a Long", "an Integer", spite_site_20()); (int32_t)spite_temp_137; });
SchedulerLoop___release(here_);
return spite_temp_136;
}
int64_t Scheduler_current_thread(Scheduler* self) {
int64_t spite_temp_141 = ({ spite_last_foreign_call = "GetCurrentThreadId\tlibrary=kernel32.dll\tfrom=library/windows/scheduler.spite:4"; int64_t spite_temp_142 = ((int64_t (*)(void))spite_foreign_1_47)();  int64_t spite_foreign_result = spite_temp_142;  (void)spite_foreign_result; spite_temp_142; });
return spite_temp_141;
}
void Scheduler_create_event(Scheduler* self, SchedulerLoop* event_loop_) {
int64_t no_value_ = SpiteInteger_to_long(0);
(event_loop_)->wake_ = ({ spite_last_foreign_call = "CreateEventA\tlibrary=kernel32.dll\tfrom=library/windows/scheduler.spite:9"; int64_t spite_temp_143 = ((int64_t (*)(int64_t, int64_t, int64_t, int64_t))spite_foreign_1_24)((int64_t)(no_value_), (int64_t)(0), (int64_t)(0), (int64_t)(no_value_));  int64_t spite_foreign_result = spite_temp_143;  (void)spite_foreign_result; spite_temp_143; });
(event_loop_)->wake_signal_ = (event_loop_)->wake_;
SchedulerLoop___release(event_loop_);
}
void Scheduler_signal_event(Scheduler* self, SchedulerLoop* event_loop_) {
(void)(({ spite_last_foreign_call = "SetEvent\tlibrary=kernel32.dll\tfrom=library/windows/scheduler.spite:14"; int32_t spite_temp_144 = ((int32_t (*)(int64_t))spite_foreign_1_48)((int64_t)((event_loop_)->wake_signal_));  int32_t spite_foreign_result = spite_temp_144;  (void)spite_foreign_result; spite_temp_144; }));
SchedulerLoop___release(event_loop_);
}
void Scheduler_wait_event(Scheduler* self, SchedulerLoop* event_loop_, int32_t milliseconds_) {
(void)(({ spite_last_foreign_call = "WaitForSingleObject\tlibrary=kernel32.dll\tfrom=library/windows/scheduler.spite:18"; int32_t spite_temp_145 = ((int32_t (*)(int64_t, int64_t))spite_foreign_1_46)((int64_t)((event_loop_)->wake_), (int64_t)(milliseconds_));  int32_t spite_foreign_result = spite_temp_145;  (void)spite_foreign_result; spite_temp_145; }));
SchedulerLoop___release(event_loop_);
}
int64_t Scheduler_clock(Scheduler* self) {
int64_t spite_temp_146 = ({ spite_last_foreign_call = "GetTickCount64\tlibrary=kernel32.dll\tfrom=library/windows/scheduler.spite:22"; int64_t spite_temp_147 = ((int64_t (*)(void))spite_foreign_1_49)();  int64_t spite_foreign_result = spite_temp_147;  (void)spite_foreign_result; spite_temp_147; });
return spite_temp_146;
}
int64_t Scheduler_start_thread(Scheduler* self, int64_t entry_, int64_t argument_) {
int64_t no_value_ = SpiteInteger_to_long(0);
int64_t spite_temp_148 = ({ spite_last_foreign_call = "CreateThread\tlibrary=kernel32.dll\tfrom=library/windows/scheduler.spite:27"; int64_t spite_temp_149 = ((int64_t (*)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t))spite_foreign_1_45)((int64_t)(no_value_), (int64_t)(no_value_), (int64_t)(entry_), (int64_t)(argument_), (int64_t)(0), (int64_t)(no_value_));  int64_t spite_foreign_result = spite_temp_149;  (void)spite_foreign_result; spite_temp_149; });
return spite_temp_148;
}
void Scheduler_end_thread(Scheduler* self, int64_t thread_) {
(void)(({ spite_last_foreign_call = "WaitForSingleObject\tlibrary=kernel32.dll\tfrom=library/windows/scheduler.spite:31"; int32_t spite_temp_150 = ((int32_t (*)(int64_t, int64_t))spite_foreign_1_46)((int64_t)(thread_), (int64_t)((-(1))));  int32_t spite_foreign_result = spite_temp_150;  (void)spite_foreign_result; spite_temp_150; }));
(void)(({ spite_last_foreign_call = "CloseHandle\tlibrary=kernel32.dll\tfrom=library/windows/scheduler.spite:32"; int32_t spite_temp_151 = ((int32_t (*)(int64_t))spite_foreign_1_22)((int64_t)(thread_));  int32_t spite_foreign_result = spite_temp_151;  (void)spite_foreign_result; spite_temp_151; }));
}
bool Scheduler_step_frame(Scheduler* self, int64_t frame_) {
return (*(bool (**)(void*))(intptr_t)frame_)((void*)(intptr_t)frame_);
}
void Scheduler_release_work(Scheduler* self, int64_t frame_) {
void** spite_slot = (void**)((char*)(intptr_t)frame_ + 16); Spite_Function* spite_work = (Spite_Function*)*spite_slot; *spite_slot = 0; Spite_Function___release(spite_work);
}
int32_t SpiteString_length(SpiteString self) {
int32_t spite_temp_152 = ({ int64_t spite_temp_153 = spite_string_length(self); if (__builtin_expect(spite_temp_153 < INT32_MIN || spite_temp_153 > INT32_MAX, 0)) spite_narrowed((int64_t)spite_temp_153, "a Long", "an Integer", spite_site_21()); (int32_t)spite_temp_153; });
return spite_temp_152;
}
SpiteString SpiteString_to_string(SpiteString self) {
SpiteString spite_temp_154 = SpiteString___retain(self);
return spite_temp_154;
}
void ThreadSlot_ThreadSlot(ThreadSlot* self) {
self->_key_ = ThreadSlot_create_key(self);
}
int64_t ThreadSlot_read(ThreadSlot* self) {
int64_t spite_temp_155 = ThreadSlot_read_key(self, self->_key_);
return spite_temp_155;
}
void ThreadSlot_write(ThreadSlot* self, int64_t value_) {
ThreadSlot_write_key(self, self->_key_, value_);
}
void ThreadSlot_drop(ThreadSlot* self) {
if (!(((self->_key_ != SpiteInteger_to_long((-(1))))))) {
return;
}
ThreadSlot_delete_key(self, self->_key_);
}
int64_t ThreadSlot_create_key(ThreadSlot* self) {
int64_t created_ = ({ spite_last_foreign_call = "TlsAlloc\tlibrary=kernel32.dll\tfrom=library/windows/thread_slot.spite:4"; int64_t spite_temp_156 = ((int64_t (*)(void))spite_foreign_1_72)();  int64_t spite_foreign_result = spite_temp_156;  (void)spite_foreign_result; spite_temp_156; });
if (!(((created_ != 4294967295)))) {
spite_failed_3(created_, self);
}
int64_t spite_temp_157 = created_;
return spite_temp_157;
}
static SPITE_CRASH_REPORT void spite_failed_3(int64_t created_, ThreadSlot* self) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_22(), stderr);
fputs("\tcreated=", stderr);
{ SpiteString spite_temp_158 = SpiteLong_to_string(created_); fwrite(spite_string_bytes(&spite_temp_158), 1, (size_t)spite_string_length(spite_temp_158), stderr); SpiteString___release(spite_temp_158); }
fputs("\t_key=", stderr);
{ SpiteString spite_temp_159 = SpiteLong_to_string(self->_key_); spite_crash_text(spite_string_bytes(&spite_temp_159), spite_string_length(spite_temp_159)); SpiteString___release(spite_temp_159); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
int64_t ThreadSlot_read_key(ThreadSlot* self, int64_t key_) {
int64_t spite_temp_160 = ({ spite_last_foreign_call = "TlsGetValue\tlibrary=kernel32.dll\tfrom=library/windows/thread_slot.spite:10"; int64_t spite_temp_161 = ((int64_t (*)(int64_t))spite_foreign_1_73)((int64_t)(key_));  int64_t spite_foreign_result = spite_temp_161;  (void)spite_foreign_result; spite_temp_161; });
return spite_temp_160;
}
void ThreadSlot_write_key(ThreadSlot* self, int64_t key_, int64_t value_) {
(void)(({ spite_last_foreign_call = "TlsSetValue\tlibrary=kernel32.dll\tfrom=library/windows/thread_slot.spite:14"; int32_t spite_temp_162 = ((int32_t (*)(int64_t, int64_t))spite_foreign_1_74)((int64_t)(key_), (int64_t)(value_));  int32_t spite_foreign_result = spite_temp_162;  (void)spite_foreign_result; spite_temp_162; }));
}
void ThreadSlot_delete_key(ThreadSlot* self, int64_t key_) {
(void)(({ spite_last_foreign_call = "TlsFree\tlibrary=kernel32.dll\tfrom=library/windows/thread_slot.spite:18"; int32_t spite_temp_163 = ((int32_t (*)(int64_t))spite_foreign_1_75)((int64_t)(key_));  int32_t spite_foreign_result = spite_temp_163;  (void)spite_foreign_result; spite_temp_163; }));
}
SpiteString SpiteMemory_Address_text(int64_t self, int64_t length_) {
return spite_string_from_bytes((const char*)(intptr_t)self, length_);
}
SpiteString SpiteMemory_Address_to_string(int64_t self) {
int64_t number_ = SpiteMemory_Address_to_long(self);
SpiteString spite_temp_164 = SpiteLong_to_string(number_);
return spite_temp_164;
}
int64_t Memory_Arena_allocate(Memory_Arena* self, int64_t bytes_) {
int64_t rounded_ = ({ int64_t spite_temp_165 = (({ int64_t spite_temp_166 = bytes_; int64_t spite_temp_167 = SpiteInteger_to_long(15); int64_t spite_temp_168; if (__builtin_expect(__builtin_add_overflow(spite_temp_166, spite_temp_167, &spite_temp_168), 0)) spite_overflowed("bytes + 15", "a Long", "+", (int64_t)spite_temp_166, (int64_t)spite_temp_167, spite_site_23()); spite_temp_168; }) / SpiteInteger_to_long(16)); int64_t spite_temp_169 = SpiteInteger_to_long(16); int64_t spite_temp_170; if (__builtin_expect(__builtin_mul_overflow(spite_temp_165, spite_temp_169, &spite_temp_170), 0)) spite_overflowed("(bytes + 15) / 16 * 16", "a Long", "*", (int64_t)spite_temp_165, (int64_t)spite_temp_169, spite_site_23()); spite_temp_170; });
if (((((self->_block_ == ((int64_t)(0)))) || ((({ int64_t spite_temp_171 = self->_used_; int64_t spite_temp_172 = rounded_; int64_t spite_temp_173; if (__builtin_expect(__builtin_add_overflow(spite_temp_171, spite_temp_172, &spite_temp_173), 0)) spite_overflowed("_used + rounded", "a Long", "+", (int64_t)spite_temp_171, (int64_t)spite_temp_172, spite_site_24()); spite_temp_173; }) > self->_end_))))) {
Memory_Arena_start_block(self, rounded_);
}
int64_t address_ = (self->_block_ + ((int64_t)(self->_used_)));
self->_used_ = ({ int64_t spite_temp_174 = self->_used_; int64_t spite_temp_175 = rounded_; int64_t spite_temp_176; if (__builtin_expect(__builtin_add_overflow(spite_temp_174, spite_temp_175, &spite_temp_176), 0)) spite_overflowed("_used + rounded", "a Long", "+", (int64_t)spite_temp_174, (int64_t)spite_temp_175, spite_site_25()); spite_temp_176; });
int64_t spite_temp_177 = address_;
return spite_temp_177;
}
void Memory_Arena_free(Memory_Arena* self, int64_t _address_) {
}
void Memory_Arena_start_block(Memory_Arena* self, int64_t at_least_) {
int64_t size_ = self->_block_bytes_;
if (((({ int64_t spite_temp_178 = at_least_; int64_t spite_temp_179 = SpiteInteger_to_long(16); int64_t spite_temp_180; if (__builtin_expect(__builtin_add_overflow(spite_temp_178, spite_temp_179, &spite_temp_180), 0)) spite_overflowed("at_least + 16", "a Long", "+", (int64_t)spite_temp_178, (int64_t)spite_temp_179, spite_site_26()); spite_temp_180; }) > size_))) {
size_ = ({ int64_t spite_temp_181 = at_least_; int64_t spite_temp_182 = SpiteInteger_to_long(16); int64_t spite_temp_183; if (__builtin_expect(__builtin_add_overflow(spite_temp_181, spite_temp_182, &spite_temp_183), 0)) spite_overflowed("at_least + 16", "a Long", "+", (int64_t)spite_temp_181, (int64_t)spite_temp_182, spite_site_27()); spite_temp_183; });
}
int64_t block_ = Memory_Heap_allocate(self->heap_, size_);
SpiteMemory_Address_write_long(block_, SpiteInteger_to_long(0), SpiteMemory_Address_to_long(self->_block_));
self->_block_ = block_;
self->_end_ = size_;
self->_used_ = SpiteInteger_to_long(16);
}
int64_t Memory_Heap_allocate(Memory_Heap* self, int64_t bytes_) {
return (int64_t)(intptr_t)SPITE_MALLOC((size_t)bytes_);
}
int64_t Memory_Heap_resize(Memory_Heap* self, int64_t address_, int64_t bytes_) {
return (int64_t)(intptr_t)SPITE_REALLOC((void*)(intptr_t)address_, (size_t)bytes_);
}
void Memory_Heap_free(Memory_Heap* self, int64_t address_) {
SPITE_FREE((void*)(intptr_t)address_);
}
void Spite_Class_Class(Spite_Class* self, SpiteString starting_name_) {
SpiteString spite_temp_184 = SpiteString___retain(starting_name_);
SpiteString___release(self->_name_);
self->_name_ = spite_temp_184;
SpiteString___release(starting_name_);
}
void Spite_Function_Function(Spite_Function* self, SpiteString starting_name_, Spite_Class* starting_returns_) {
SpiteString spite_temp_185 = SpiteString___retain(starting_name_);
SpiteString___release(self->_name_);
self->_name_ = spite_temp_185;
Spite_Class* spite_temp_186 = Spite_Class___retain(starting_returns_);
Spite_Class___release(self->_returns_);
self->_returns_ = spite_temp_186;
Spite_Class___release(starting_returns_);
SpiteString___release(starting_name_);
}
void Naive_Naive(Naive* self) {
File spite_slot_1;
File* first_file_ = File___make_into(&spite_slot_1, spite_lit_14);
File spite_slot_2;
File* second_file_ = File___make_into(&spite_slot_2, spite_lit_15);
SpiteString first_text_ = Naive_make_text(self, spite_lit_16, 4000);
SpiteString second_text_ = Naive_make_text(self, spite_lit_17, 4000);
(void)(File_write(first_file_, SpiteString___retain(first_text_)));
(void)(File_write(second_file_, SpiteString___retain(second_text_)));
int64_t start_ = Clock_elapsed_nanoseconds(self->clock_);
int64_t characters_ = Naive_read_rounds___held_0_1(self, first_file_, second_file_, 40);
int64_t microseconds_ = (({ int64_t spite_temp_187 = Clock_elapsed_nanoseconds(self->clock_); int64_t spite_temp_188 = start_; int64_t spite_temp_189; if (__builtin_expect(__builtin_sub_overflow(spite_temp_187, spite_temp_188, &spite_temp_189), 0)) spite_overflowed("clock.elapsed_nanoseconds() - start", "a Long", "-", (int64_t)spite_temp_187, (int64_t)spite_temp_188, spite_site_28()); spite_temp_189; }) / SpiteInteger_to_long(1000));
bool first_removed_ = File_remove(first_file_);
bool second_removed_ = File_remove(second_file_);
List_Console_Printable spite_framed_1; Console_Printable spite_framed_1_items[5]; int32_t spite_framed_1_count = 0;
Console_print(self->console_, ({ spite_framed_1_items[0] = spite_tagged_object(0, ((void*)&spite_lit_18_box)); spite_framed_1_items[1] = spite_tagged_SpiteLong(characters_); spite_framed_1_items[2] = spite_tagged_object(0, ((void*)&spite_lit_19_box)); spite_framed_1_items[3] = spite_tagged_SpiteBoolean(first_removed_); spite_framed_1_items[4] = spite_tagged_SpiteBoolean(second_removed_); spite_framed_1_count = 5; List_Console_Printable___framed(&spite_framed_1, (int64_t)(intptr_t)spite_framed_1_items, 5); }));
for (int32_t spite_index = 0; spite_index < spite_framed_1_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_1_items[spite_index]); }
List_Console_Printable spite_framed_2; Console_Printable spite_framed_2_items[1]; int32_t spite_framed_2_count = 0;
Console_error(self->console_, ({ spite_framed_2_items[0] = spite_tagged_object(0, spite_box_SpiteString(({ char spite_temp_190_digits[24]; SpiteString spite_temp_190 = SPITE_STATIC_STRING(spite_temp_190_digits, spite_long_digits(spite_temp_190_digits, (int64_t)(microseconds_))); SpiteString spite_temp_191[] = {spite_lit_20, spite_temp_190}; SpiteString spite_temp_192 = spite_string_join(2, spite_temp_191); spite_temp_192; }))); spite_framed_2_count = 1; List_Console_Printable___framed(&spite_framed_2, (int64_t)(intptr_t)spite_framed_2_items, 1); }));
for (int32_t spite_index = 0; spite_index < spite_framed_2_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_2_items[spite_index]); }
SpiteString___release(second_text_);
SpiteString___release(first_text_);
File___unframe(second_file_);
File___unframe(first_file_);
}
SpiteString Naive_make_text(Naive* self, SpiteString word_, int32_t count_) {
List_String* lines_ = List_String___make();
int32_t index_ = 0;
while (((index_ < count_))) {
List_String_append(lines_, ({ SpiteString spite_temp_193 = word_; char spite_temp_194_digits[24]; SpiteString spite_temp_194 = SPITE_STATIC_STRING(spite_temp_194_digits, spite_long_digits(spite_temp_194_digits, (int64_t)(index_))); SpiteString spite_temp_195[] = {spite_temp_193, spite_lit_21, spite_temp_194}; SpiteString spite_temp_196 = spite_string_join(3, spite_temp_195); spite_temp_196; }));
index_ = (index_ + 1);
}
SpiteString spite_temp_197 = List_String_join(lines_, spite_lit_22);
List_String___release(lines_);
SpiteString___release(word_);
return spite_temp_197;
}
int64_t Naive_read_rounds___held_0_1(Naive* self, File* first_file_, File* second_file_, int32_t rounds_) {
int64_t characters_ = SpiteInteger_to_long(0);
int32_t round_ = 0;
while (((round_ < rounds_))) {
int32_t read_ = Naive_read_both___held_0_1(self, first_file_, second_file_);
characters_ = (characters_ + SpiteInteger_to_long(read_));
round_ = (round_ + 1);
}
int64_t spite_temp_198 = characters_;
return spite_temp_198;
}
int32_t Naive_read_both___held_0_1(Naive* self, File* first_file_, File* second_file_) {
Concurrent__Nullable_String* first_ = Concurrent__Nullable_String___make(spite_function_value_File_read(first_file_));
SpiteString second_ = File_read(second_file_);
Concurrent__Nullable_String__join(first_);
SpiteString spite_temp_3_ = Concurrent__Nullable_String__result(first_);
if (!((!SPITE_STRING_IS_NULL(spite_temp_3_)))) {
spite_failed_4(second_);
}
if (!((!SPITE_STRING_IS_NULL(second_)))) {
spite_failed_5(spite_temp_3_);
}
int32_t spite_temp_199 = ({ int32_t spite_temp_200 = SpiteString_length(spite_temp_3_); int32_t spite_temp_201 = SpiteString_length(second_); int32_t spite_temp_202; if (__builtin_expect(__builtin_add_overflow(spite_temp_200, spite_temp_201, &spite_temp_202), 0)) spite_overflowed("first.length() + second.length()", "an Integer", "+", (int64_t)spite_temp_200, (int64_t)spite_temp_201, spite_site_31()); spite_temp_202; });
SpiteString___release(spite_temp_3_);
SpiteString___release(second_);
Concurrent__Nullable_String___release(first_);
return spite_temp_199;
}
static SPITE_CRASH_REPORT void spite_failed_4(SpiteString second_) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_29(), stderr);
fputs("\tfirst is null", stderr);
if (!((!SPITE_STRING_IS_NULL(second_)))) {
fputs("\tsecond is null", stderr);
} else {
fputs("\tsecond=", stderr);
{ SpiteString spite_temp_203 = second_; spite_crash_text(spite_string_bytes(&spite_temp_203), spite_string_length(spite_temp_203)); }
}
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
static SPITE_CRASH_REPORT void spite_failed_5(SpiteString spite_temp_3_) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_30(), stderr);
fputs("\tsecond is null", stderr);
fputs("\tfirst=", stderr);
{ SpiteString spite_temp_204 = spite_temp_3_; spite_crash_text(spite_string_bytes(&spite_temp_204), spite_string_length(spite_temp_204)); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
int32_t List_String_count(List_String* self) {
int32_t spite_temp_205 = self->item_count_;
return spite_temp_205;
}
void List_String_append(List_String* self, SpiteString value_) {
List_String_make_room(self);
TypedMemory__String_write_value(self->values_, self->items_, self->item_count_, SpiteString___retain(value_));
self->item_count_ = ({ int32_t spite_temp_206 = self->item_count_; int32_t spite_temp_207 = 1; int32_t spite_temp_208; if (__builtin_expect(__builtin_add_overflow(spite_temp_206, spite_temp_207, &spite_temp_208), 0)) spite_overflowed("item_count + 1", "an Integer", "+", (int64_t)spite_temp_206, (int64_t)spite_temp_207, spite_site_32()); spite_temp_208; });
SpiteString___release(value_);
}
SpiteString List_String_get_at(List_String* self, int32_t index_) {
if ((((index_ >= 0))) && (((index_ < self->item_count_)))) {
SpiteString spite_temp_209 = TypedMemory__String_read_value(self->values_, self->items_, index_);
return spite_temp_209;
}
SpiteString spite_temp_210 = SPITE_STRING_NULL;
return spite_temp_210;
}
void List_String_clear(List_String* self) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
TypedMemory__String_release_value(self->values_, self->items_, index_);
index_ = (index_ + 1);
}
self->item_count_ = 0;
}
SpiteString List_String_join(List_String* self, SpiteString separator_) {
List_String* pieces_ = List_String___make();
int64_t total_ = SpiteInteger_to_long(0);
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
SpiteString piece_ = TypedMemory__String_read_value(self->values_, self->items_, index_);
total_ = (total_ + SpiteInteger_to_long(SpiteString_length(piece_)));
List_String_append(pieces_, SpiteString___retain(piece_));
index_ = (index_ + 1);
SpiteString___release(piece_);
}
if (((self->item_count_ > 1))) {
total_ = ({ int64_t spite_temp_211 = total_; int64_t spite_temp_212 = SpiteInteger_to_long(({ int32_t spite_temp_213 = SpiteString_length(separator_); int32_t spite_temp_214 = ({ int32_t spite_temp_215 = self->item_count_; int32_t spite_temp_216 = 1; int32_t spite_temp_217; if (__builtin_expect(__builtin_sub_overflow(spite_temp_215, spite_temp_216, &spite_temp_217), 0)) spite_overflowed("item_count - 1", "an Integer", "-", (int64_t)spite_temp_215, (int64_t)spite_temp_216, spite_site_37()); spite_temp_217; }); int32_t spite_temp_218; if (__builtin_expect(__builtin_mul_overflow(spite_temp_213, spite_temp_214, &spite_temp_218), 0)) spite_overflowed("separator.length() * (item_count - 1)", "an Integer", "*", (int64_t)spite_temp_213, (int64_t)spite_temp_214, spite_site_37()); spite_temp_218; })); int64_t spite_temp_219; if (__builtin_expect(__builtin_add_overflow(spite_temp_211, spite_temp_212, &spite_temp_219), 0)) spite_overflowed("total + separator.length() * (item_count - 1)", "a Long", "+", (int64_t)spite_temp_211, (int64_t)spite_temp_212, spite_site_37()); spite_temp_219; });
}
int64_t spite_temp_220[32];
int64_t spite_temp_221 = ({ int64_t spite_temp_222 = total_; int64_t spite_temp_223 = SpiteInteger_to_long(1); int64_t spite_temp_224; if (__builtin_expect(__builtin_add_overflow(spite_temp_222, spite_temp_223, &spite_temp_224), 0)) spite_overflowed("total + 1", "a Long", "+", (int64_t)spite_temp_222, (int64_t)spite_temp_223, spite_site_38()); spite_temp_224; });
int64_t address_ = spite_temp_221 <= 256 ? (int64_t)(intptr_t)spite_temp_220 : Memory_Heap_allocate(self->heap_, spite_temp_221);
int64_t position_ = SpiteInteger_to_long(0);
index_ = 0;
while (((index_ < List_String_count(pieces_)))) {
if (((index_ > 0))) {
position_ = List_String_write_text(self, SpiteString___retain(separator_), address_, position_);
}
SpiteString next_piece_ = ({ SpiteString spite_temp_225 = List_String_get_at(pieces_, index_); if (__builtin_expect(!((!SPITE_STRING_IS_NULL(spite_temp_225))), 0)) spite_outside_list("pieces[index]", spite_site_39()); spite_temp_225; });
position_ = List_String_write_text(self, SpiteString___retain(next_piece_), address_, position_);
index_ = (index_ + 1);
SpiteString___release(next_piece_);
}
SpiteString joined_ = SpiteMemory_Address_text(address_, total_);
if (address_ != (int64_t)(intptr_t)spite_temp_220) Memory_Heap_free(self->heap_, address_);
SpiteString spite_temp_226 = SpiteString___retain(joined_);
SpiteString___release(joined_);
List_String___release(pieces_);
SpiteString___release(separator_);
return spite_temp_226;
}
int64_t List_String_write_text(List_String* self, SpiteString text_, int64_t address_, int64_t position_) {
int32_t index_ = 0;
while (((index_ < SpiteString_length(text_)))) {
int32_t code_ = SpiteString_code_at(text_, index_);
SpiteMemory_Address_write_byte(address_, ({ int64_t spite_temp_227 = position_; int64_t spite_temp_228 = SpiteInteger_to_long(index_); int64_t spite_temp_229; if (__builtin_expect(__builtin_add_overflow(spite_temp_227, spite_temp_228, &spite_temp_229), 0)) spite_overflowed("position + index", "a Long", "+", (int64_t)spite_temp_227, (int64_t)spite_temp_228, spite_site_40()); spite_temp_229; }), ({ int32_t spite_temp_230 = code_; if (__builtin_expect(spite_temp_230 < 0 || spite_temp_230 > UINT8_MAX, 0)) spite_narrowed((int64_t)spite_temp_230, "an Integer", "a Byte", spite_site_40()); (uint8_t)spite_temp_230; }));
index_ = (index_ + 1);
}
int64_t spite_temp_231 = ({ int64_t spite_temp_232 = position_; int64_t spite_temp_233 = SpiteInteger_to_long(SpiteString_length(text_)); int64_t spite_temp_234; if (__builtin_expect(__builtin_add_overflow(spite_temp_232, spite_temp_233, &spite_temp_234), 0)) spite_overflowed("position + text.length()", "a Long", "+", (int64_t)spite_temp_232, (int64_t)spite_temp_233, spite_site_41()); spite_temp_234; });
SpiteString___release(text_);
return spite_temp_231;
}
void List_String_drop(List_String* self) {
List_String_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_235 = SPITE_ALLOCATOR_List_String(self, spite_singleton_Memory_Heap); int64_t spite_temp_236 = self->items_; if (((SpiteHeader*)(spite_temp_235))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_235), spite_temp_236); } else if (((SpiteHeader*)(spite_temp_235))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_235), spite_temp_236); } });
}
}
void List_String_make_room(List_String* self) {
if (((self->item_count_ == self->capacity_))) {
List_String__grow(self);
}
}
void List_String__grow(List_String* self) {
int32_t grown_ = ({ int32_t spite_temp_237 = self->capacity_; int32_t spite_temp_238 = 2; int32_t spite_temp_239; if (__builtin_expect(__builtin_mul_overflow(spite_temp_237, spite_temp_238, &spite_temp_239), 0)) spite_overflowed("capacity * 2", "an Integer", "*", (int64_t)spite_temp_237, (int64_t)spite_temp_238, spite_site_42()); spite_temp_239; });
if (((self->capacity_ == 0))) {
grown_ = 4;
}
int64_t bytes_ = TypedMemory__String_value_bytes(self->values_);
self->items_ = List_String__resized(self, ({ int64_t spite_temp_240 = bytes_; int64_t spite_temp_241 = SpiteInteger_to_long(self->capacity_); int64_t spite_temp_242; if (__builtin_expect(__builtin_mul_overflow(spite_temp_240, spite_temp_241, &spite_temp_242), 0)) spite_overflowed("bytes * capacity", "a Long", "*", (int64_t)spite_temp_240, (int64_t)spite_temp_241, spite_site_43()); spite_temp_242; }), ({ int64_t spite_temp_243 = bytes_; int64_t spite_temp_244 = SpiteInteger_to_long(grown_); int64_t spite_temp_245; if (__builtin_expect(__builtin_mul_overflow(spite_temp_243, spite_temp_244, &spite_temp_245), 0)) spite_overflowed("bytes * grown", "a Long", "*", (int64_t)spite_temp_243, (int64_t)spite_temp_244, spite_site_43()); spite_temp_245; }));
self->capacity_ = grown_;
}
int64_t List_String__resized(List_String* self, int64_t old_bytes_, int64_t new_bytes_) {
if ((({ Spite_Allocator spite_temp_246 = SPITE_ALLOCATOR_List_String(self, spite_singleton_Memory_Heap); bool spite_temp_247 = (((SpiteHeader*)(spite_temp_246))->class_id == 94); spite_temp_247; }))) {
int64_t spite_temp_248 = Memory_Heap_resize(self->heap_, self->items_, new_bytes_);
return spite_temp_248;
}
int64_t moved_ = ({ Spite_Allocator spite_temp_249 = SPITE_ALLOCATOR_List_String(self, spite_singleton_Memory_Heap); int64_t spite_temp_250 = new_bytes_; int64_t spite_temp_251 = 0; if (((SpiteHeader*)(spite_temp_249))->class_id == 93) { spite_temp_251 = Memory_Arena_allocate(((Memory_Arena*)spite_temp_249), spite_temp_250); } else if (((SpiteHeader*)(spite_temp_249))->class_id == 94) { spite_temp_251 = Memory_Heap_allocate(((Memory_Heap*)spite_temp_249), spite_temp_250); } spite_temp_251; });
if (((self->items_ != ((int64_t)(0))))) {
SpiteMemory_Address_copy_to(self->items_, moved_, old_bytes_);
({ Spite_Allocator spite_temp_252 = SPITE_ALLOCATOR_List_String(self, spite_singleton_Memory_Heap); int64_t spite_temp_253 = self->items_; if (((SpiteHeader*)(spite_temp_252))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_252), spite_temp_253); } else if (((SpiteHeader*)(spite_temp_252))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_252), spite_temp_253); } });
}
int64_t spite_temp_254 = moved_;
return spite_temp_254;
}
SpiteString TypedMemory__String_read_value(TypedMemory__String* self, int64_t address_, int32_t index_) {
return SpiteString___retain(((SpiteString*)(intptr_t)address_)[index_]);
}
void TypedMemory__String_write_value(TypedMemory__String* self, int64_t address_, int32_t index_, SpiteString value_) {
((SpiteString*)(intptr_t)address_)[index_] = value_;
}
void TypedMemory__String_release_value(TypedMemory__String* self, int64_t address_, int32_t index_) {
SpiteString___release(((SpiteString*)(intptr_t)address_)[index_]);
}
int64_t TypedMemory__String_value_bytes(TypedMemory__String* self) {
return (int64_t)sizeof(SpiteString);
}
bool List_Long_is_empty(List_Long* self) {
bool spite_temp_255 = (self->item_count_ == 0);
return spite_temp_255;
}
void List_Long_append(List_Long* self, int64_t value_) {
List_Long_make_room(self);
TypedMemory__Long_write_value(self->values_, self->items_, self->item_count_, value_);
self->item_count_ = ({ int32_t spite_temp_256 = self->item_count_; int32_t spite_temp_257 = 1; int32_t spite_temp_258; if (__builtin_expect(__builtin_add_overflow(spite_temp_256, spite_temp_257, &spite_temp_258), 0)) spite_overflowed("item_count + 1", "an Integer", "+", (int64_t)spite_temp_256, (int64_t)spite_temp_257, spite_site_32()); spite_temp_258; });
}
Nullable_Long List_Long_get_at(List_Long* self, int32_t index_) {
if ((((index_ >= 0))) && (((index_ < self->item_count_)))) {
Nullable_Long spite_temp_259 = ((Nullable_Long){ .has_value = true, .value = TypedMemory__Long_read_value(self->values_, self->items_, index_) });
return spite_temp_259;
}
Nullable_Long spite_temp_260 = ((Nullable_Long){ .has_value = false, .value = 0 });
return spite_temp_260;
}
void List_Long_remove_at(List_Long* self, int32_t index_) {
if (!(((index_ >= 0)))) {
spite_failed_6(index_, self);
}
if (!(((index_ < self->item_count_)))) {
spite_failed_7(index_, self);
}
TypedMemory__Long_release_value(self->values_, self->items_, index_);
List_Long_move_items(self, (index_ + 1), index_, ({ int32_t spite_temp_261 = ({ int32_t spite_temp_262 = self->item_count_; int32_t spite_temp_263 = index_; int32_t spite_temp_264; if (__builtin_expect(__builtin_sub_overflow(spite_temp_262, spite_temp_263, &spite_temp_264), 0)) spite_overflowed("item_count - index", "an Integer", "-", (int64_t)spite_temp_262, (int64_t)spite_temp_263, spite_site_35()); spite_temp_264; }); int32_t spite_temp_265 = 1; int32_t spite_temp_266; if (__builtin_expect(__builtin_sub_overflow(spite_temp_261, spite_temp_265, &spite_temp_266), 0)) spite_overflowed("item_count - index - 1", "an Integer", "-", (int64_t)spite_temp_261, (int64_t)spite_temp_265, spite_site_35()); spite_temp_266; }));
self->item_count_ = ({ int32_t spite_temp_267 = self->item_count_; int32_t spite_temp_268 = 1; int32_t spite_temp_269; if (__builtin_expect(__builtin_sub_overflow(spite_temp_267, spite_temp_268, &spite_temp_269), 0)) spite_overflowed("item_count - 1", "an Integer", "-", (int64_t)spite_temp_267, (int64_t)spite_temp_268, spite_site_36()); spite_temp_269; });
}
static SPITE_CRASH_REPORT void spite_failed_6(int32_t index_, List_Long* self) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_33(), stderr);
fputs("\tindex=", stderr);
{ SpiteString spite_temp_270 = SpiteInteger_to_string(index_); fwrite(spite_string_bytes(&spite_temp_270), 1, (size_t)spite_string_length(spite_temp_270), stderr); SpiteString___release(spite_temp_270); }
fputs("\titems=", stderr);
{ SpiteString spite_temp_271 = SpiteMemory_Address_to_string(self->items_); spite_crash_text(spite_string_bytes(&spite_temp_271), spite_string_length(spite_temp_271)); SpiteString___release(spite_temp_271); }
fputs("\titem_count=", stderr);
{ SpiteString spite_temp_272 = SpiteInteger_to_string(self->item_count_); spite_crash_text(spite_string_bytes(&spite_temp_272), spite_string_length(spite_temp_272)); SpiteString___release(spite_temp_272); }
fputs("\tcapacity=", stderr);
{ SpiteString spite_temp_273 = SpiteInteger_to_string(self->capacity_); spite_crash_text(spite_string_bytes(&spite_temp_273), spite_string_length(spite_temp_273)); SpiteString___release(spite_temp_273); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
static SPITE_CRASH_REPORT void spite_failed_7(int32_t index_, List_Long* self) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_34(), stderr);
fputs("\tindex=", stderr);
{ SpiteString spite_temp_274 = SpiteInteger_to_string(index_); fwrite(spite_string_bytes(&spite_temp_274), 1, (size_t)spite_string_length(spite_temp_274), stderr); SpiteString___release(spite_temp_274); }
fputs("\titem_count=", stderr);
{ SpiteString spite_temp_275 = SpiteInteger_to_string(self->item_count_); fwrite(spite_string_bytes(&spite_temp_275), 1, (size_t)spite_string_length(spite_temp_275), stderr); SpiteString___release(spite_temp_275); }
fputs("\titems=", stderr);
{ SpiteString spite_temp_276 = SpiteMemory_Address_to_string(self->items_); spite_crash_text(spite_string_bytes(&spite_temp_276), spite_string_length(spite_temp_276)); SpiteString___release(spite_temp_276); }
fputs("\tcapacity=", stderr);
{ SpiteString spite_temp_277 = SpiteInteger_to_string(self->capacity_); spite_crash_text(spite_string_bytes(&spite_temp_277), spite_string_length(spite_temp_277)); SpiteString___release(spite_temp_277); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
void List_Long_clear(List_Long* self) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
TypedMemory__Long_release_value(self->values_, self->items_, index_);
index_ = (index_ + 1);
}
self->item_count_ = 0;
}
void List_Long_drop(List_Long* self) {
List_Long_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_278 = SPITE_ALLOCATOR_List_Long(self, spite_singleton_Memory_Heap); int64_t spite_temp_279 = self->items_; if (((SpiteHeader*)(spite_temp_278))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_278), spite_temp_279); } else if (((SpiteHeader*)(spite_temp_278))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_278), spite_temp_279); } });
}
}
void List_Long_make_room(List_Long* self) {
if (((self->item_count_ == self->capacity_))) {
List_Long__grow(self);
}
}
void List_Long__grow(List_Long* self) {
int32_t grown_ = ({ int32_t spite_temp_280 = self->capacity_; int32_t spite_temp_281 = 2; int32_t spite_temp_282; if (__builtin_expect(__builtin_mul_overflow(spite_temp_280, spite_temp_281, &spite_temp_282), 0)) spite_overflowed("capacity * 2", "an Integer", "*", (int64_t)spite_temp_280, (int64_t)spite_temp_281, spite_site_42()); spite_temp_282; });
if (((self->capacity_ == 0))) {
grown_ = 4;
}
int64_t bytes_ = TypedMemory__Long_value_bytes(self->values_);
self->items_ = List_Long__resized(self, ({ int64_t spite_temp_283 = bytes_; int64_t spite_temp_284 = SpiteInteger_to_long(self->capacity_); int64_t spite_temp_285; if (__builtin_expect(__builtin_mul_overflow(spite_temp_283, spite_temp_284, &spite_temp_285), 0)) spite_overflowed("bytes * capacity", "a Long", "*", (int64_t)spite_temp_283, (int64_t)spite_temp_284, spite_site_43()); spite_temp_285; }), ({ int64_t spite_temp_286 = bytes_; int64_t spite_temp_287 = SpiteInteger_to_long(grown_); int64_t spite_temp_288; if (__builtin_expect(__builtin_mul_overflow(spite_temp_286, spite_temp_287, &spite_temp_288), 0)) spite_overflowed("bytes * grown", "a Long", "*", (int64_t)spite_temp_286, (int64_t)spite_temp_287, spite_site_43()); spite_temp_288; }));
self->capacity_ = grown_;
}
int64_t List_Long__resized(List_Long* self, int64_t old_bytes_, int64_t new_bytes_) {
if ((({ Spite_Allocator spite_temp_289 = SPITE_ALLOCATOR_List_Long(self, spite_singleton_Memory_Heap); bool spite_temp_290 = (((SpiteHeader*)(spite_temp_289))->class_id == 94); spite_temp_290; }))) {
int64_t spite_temp_291 = Memory_Heap_resize(self->heap_, self->items_, new_bytes_);
return spite_temp_291;
}
int64_t moved_ = ({ Spite_Allocator spite_temp_292 = SPITE_ALLOCATOR_List_Long(self, spite_singleton_Memory_Heap); int64_t spite_temp_293 = new_bytes_; int64_t spite_temp_294 = 0; if (((SpiteHeader*)(spite_temp_292))->class_id == 93) { spite_temp_294 = Memory_Arena_allocate(((Memory_Arena*)spite_temp_292), spite_temp_293); } else if (((SpiteHeader*)(spite_temp_292))->class_id == 94) { spite_temp_294 = Memory_Heap_allocate(((Memory_Heap*)spite_temp_292), spite_temp_293); } spite_temp_294; });
if (((self->items_ != ((int64_t)(0))))) {
SpiteMemory_Address_copy_to(self->items_, moved_, old_bytes_);
({ Spite_Allocator spite_temp_295 = SPITE_ALLOCATOR_List_Long(self, spite_singleton_Memory_Heap); int64_t spite_temp_296 = self->items_; if (((SpiteHeader*)(spite_temp_295))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_295), spite_temp_296); } else if (((SpiteHeader*)(spite_temp_295))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_295), spite_temp_296); } });
}
int64_t spite_temp_297 = moved_;
return spite_temp_297;
}
void List_Long_move_items(List_Long* self, int32_t from_, int32_t to_, int32_t moved_count_) {
int64_t bytes_ = TypedMemory__Long_value_bytes(self->values_);
int64_t moved_items_ = (self->items_ + ((int64_t)(({ int64_t spite_temp_298 = bytes_; int64_t spite_temp_299 = SpiteInteger_to_long(from_); int64_t spite_temp_300; if (__builtin_expect(__builtin_mul_overflow(spite_temp_298, spite_temp_299, &spite_temp_300), 0)) spite_overflowed("bytes * from", "a Long", "*", (int64_t)spite_temp_298, (int64_t)spite_temp_299, spite_site_44()); spite_temp_300; }))));
SpiteMemory_Address_copy_to(moved_items_, (self->items_ + ((int64_t)(({ int64_t spite_temp_301 = bytes_; int64_t spite_temp_302 = SpiteInteger_to_long(to_); int64_t spite_temp_303; if (__builtin_expect(__builtin_mul_overflow(spite_temp_301, spite_temp_302, &spite_temp_303), 0)) spite_overflowed("bytes * to", "a Long", "*", (int64_t)spite_temp_301, (int64_t)spite_temp_302, spite_site_45()); spite_temp_303; })))), ({ int64_t spite_temp_304 = bytes_; int64_t spite_temp_305 = SpiteInteger_to_long(moved_count_); int64_t spite_temp_306; if (__builtin_expect(__builtin_mul_overflow(spite_temp_304, spite_temp_305, &spite_temp_306), 0)) spite_overflowed("bytes * moved_count", "a Long", "*", (int64_t)spite_temp_304, (int64_t)spite_temp_305, spite_site_45()); spite_temp_306; }));
}
int64_t TypedMemory__Long_read_value(TypedMemory__Long* self, int64_t address_, int32_t index_) {
return ((int64_t*)(intptr_t)address_)[index_];
}
void TypedMemory__Long_write_value(TypedMemory__Long* self, int64_t address_, int32_t index_, int64_t value_) {
((int64_t*)(intptr_t)address_)[index_] = value_;
}
void TypedMemory__Long_release_value(TypedMemory__Long* self, int64_t address_, int32_t index_) {

}
int64_t TypedMemory__Long_value_bytes(TypedMemory__Long* self) {
return (int64_t)sizeof(int64_t);
}
void List_Memory_Address_append(List_Memory_Address* self, int64_t value_) {
List_Memory_Address_make_room(self);
spite_folded_TypedMemory__Memory_Address_write_value(self->values_, self->items_, self->item_count_, value_);
self->item_count_ = ({ int32_t spite_temp_307 = self->item_count_; int32_t spite_temp_308 = 1; int32_t spite_temp_309; if (__builtin_expect(__builtin_add_overflow(spite_temp_307, spite_temp_308, &spite_temp_309), 0)) spite_overflowed("item_count + 1", "an Integer", "+", (int64_t)spite_temp_307, (int64_t)spite_temp_308, spite_site_32()); spite_temp_309; });
}
void List_Memory_Address_drop(List_Memory_Address* self) {
spite_folded_List_Memory_Address_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_310 = SPITE_ALLOCATOR_List_Memory_Address(self, spite_singleton_Memory_Heap); int64_t spite_temp_311 = self->items_; if (((SpiteHeader*)(spite_temp_310))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_310), spite_temp_311); } else if (((SpiteHeader*)(spite_temp_310))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_310), spite_temp_311); } });
}
}
void List_Memory_Address_make_room(List_Memory_Address* self) {
if (((self->item_count_ == self->capacity_))) {
List_Memory_Address__grow(self);
}
}
void List_Memory_Address__grow(List_Memory_Address* self) {
int32_t grown_ = ({ int32_t spite_temp_312 = self->capacity_; int32_t spite_temp_313 = 2; int32_t spite_temp_314; if (__builtin_expect(__builtin_mul_overflow(spite_temp_312, spite_temp_313, &spite_temp_314), 0)) spite_overflowed("capacity * 2", "an Integer", "*", (int64_t)spite_temp_312, (int64_t)spite_temp_313, spite_site_42()); spite_temp_314; });
if (((self->capacity_ == 0))) {
grown_ = 4;
}
int64_t bytes_ = spite_folded_TypedMemory__Memory_Address_value_bytes(self->values_);
self->items_ = List_Memory_Address__resized(self, ({ int64_t spite_temp_315 = bytes_; int64_t spite_temp_316 = SpiteInteger_to_long(self->capacity_); int64_t spite_temp_317; if (__builtin_expect(__builtin_mul_overflow(spite_temp_315, spite_temp_316, &spite_temp_317), 0)) spite_overflowed("bytes * capacity", "a Long", "*", (int64_t)spite_temp_315, (int64_t)spite_temp_316, spite_site_43()); spite_temp_317; }), ({ int64_t spite_temp_318 = bytes_; int64_t spite_temp_319 = SpiteInteger_to_long(grown_); int64_t spite_temp_320; if (__builtin_expect(__builtin_mul_overflow(spite_temp_318, spite_temp_319, &spite_temp_320), 0)) spite_overflowed("bytes * grown", "a Long", "*", (int64_t)spite_temp_318, (int64_t)spite_temp_319, spite_site_43()); spite_temp_320; }));
self->capacity_ = grown_;
}
int64_t List_Memory_Address__resized(List_Memory_Address* self, int64_t old_bytes_, int64_t new_bytes_) {
if ((({ Spite_Allocator spite_temp_321 = SPITE_ALLOCATOR_List_Memory_Address(self, spite_singleton_Memory_Heap); bool spite_temp_322 = (((SpiteHeader*)(spite_temp_321))->class_id == 94); spite_temp_322; }))) {
int64_t spite_temp_323 = Memory_Heap_resize(self->heap_, self->items_, new_bytes_);
return spite_temp_323;
}
int64_t moved_ = ({ Spite_Allocator spite_temp_324 = SPITE_ALLOCATOR_List_Memory_Address(self, spite_singleton_Memory_Heap); int64_t spite_temp_325 = new_bytes_; int64_t spite_temp_326 = 0; if (((SpiteHeader*)(spite_temp_324))->class_id == 93) { spite_temp_326 = Memory_Arena_allocate(((Memory_Arena*)spite_temp_324), spite_temp_325); } else if (((SpiteHeader*)(spite_temp_324))->class_id == 94) { spite_temp_326 = Memory_Heap_allocate(((Memory_Heap*)spite_temp_324), spite_temp_325); } spite_temp_326; });
if (((self->items_ != ((int64_t)(0))))) {
SpiteMemory_Address_copy_to(self->items_, moved_, old_bytes_);
({ Spite_Allocator spite_temp_327 = SPITE_ALLOCATOR_List_Memory_Address(self, spite_singleton_Memory_Heap); int64_t spite_temp_328 = self->items_; if (((SpiteHeader*)(spite_temp_327))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_327), spite_temp_328); } else if (((SpiteHeader*)(spite_temp_327))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_327), spite_temp_328); } });
}
int64_t spite_temp_329 = moved_;
return spite_temp_329;
}
void ThreadLocal__SchedulerLoop_ThreadLocal(ThreadLocal__SchedulerLoop* self) {
SpiteMemory_Address_write_long_atomically(self->_published_, SpiteInteger_to_long(0), SpiteInteger_to_long(0));
}
SchedulerLoop* ThreadLocal__SchedulerLoop_get(ThreadLocal__SchedulerLoop* self) {
int64_t position_ = ThreadSlot_read(self->_slot_);
if (!(((position_ != SpiteInteger_to_long(0))))) {
return 0;
}
int64_t items_ = ((int64_t)(SpiteMemory_Address_read_long_atomically(self->_published_, SpiteInteger_to_long(0))));
SchedulerLoop* spite_temp_330 = TypedMemory__SchedulerLoop_read_value(self->_values_, items_, ({ int64_t spite_temp_331 = ({ int64_t spite_temp_332 = position_; int64_t spite_temp_333 = SpiteInteger_to_long(1); int64_t spite_temp_334; if (__builtin_expect(__builtin_sub_overflow(spite_temp_332, spite_temp_333, &spite_temp_334), 0)) spite_overflowed("position - 1", "a Long", "-", (int64_t)spite_temp_332, (int64_t)spite_temp_333, spite_site_46()); spite_temp_334; }); if (__builtin_expect(spite_temp_331 < INT32_MIN || spite_temp_331 > INT32_MAX, 0)) spite_narrowed((int64_t)spite_temp_331, "a Long", "an Integer", spite_site_46()); (int32_t)spite_temp_331; }));
return spite_temp_330;
}
void ThreadLocal__SchedulerLoop_set(ThreadLocal__SchedulerLoop* self, SchedulerLoop* value_) {
int64_t position_ = ThreadSlot_read(self->_slot_);
Lock_lock(self->_lock_);
if (((position_ == SpiteInteger_to_long(0)))) {
int64_t items_ = ThreadLocal__SchedulerLoop__make_room(self);
TypedMemory__SchedulerLoop_write_value(self->_values_, items_, self->_count_, SchedulerLoop___retain(value_));
self->_count_ = ({ int32_t spite_temp_335 = self->_count_; int32_t spite_temp_336 = 1; int32_t spite_temp_337; if (__builtin_expect(__builtin_add_overflow(spite_temp_335, spite_temp_336, &spite_temp_337), 0)) spite_overflowed("_count + 1", "an Integer", "+", (int64_t)spite_temp_335, (int64_t)spite_temp_336, spite_site_47()); spite_temp_337; });
ThreadSlot_write(self->_slot_, SpiteInteger_to_long(self->_count_));
}
else {
int64_t items_ = ((int64_t)(SpiteMemory_Address_read_long_atomically(self->_published_, SpiteInteger_to_long(0))));
TypedMemory__SchedulerLoop_release_value(self->_values_, items_, ({ int64_t spite_temp_338 = ({ int64_t spite_temp_339 = position_; int64_t spite_temp_340 = SpiteInteger_to_long(1); int64_t spite_temp_341; if (__builtin_expect(__builtin_sub_overflow(spite_temp_339, spite_temp_340, &spite_temp_341), 0)) spite_overflowed("position - 1", "a Long", "-", (int64_t)spite_temp_339, (int64_t)spite_temp_340, spite_site_48()); spite_temp_341; }); if (__builtin_expect(spite_temp_338 < INT32_MIN || spite_temp_338 > INT32_MAX, 0)) spite_narrowed((int64_t)spite_temp_338, "a Long", "an Integer", spite_site_48()); (int32_t)spite_temp_338; }));
TypedMemory__SchedulerLoop_write_value(self->_values_, items_, ({ int64_t spite_temp_342 = ({ int64_t spite_temp_343 = position_; int64_t spite_temp_344 = SpiteInteger_to_long(1); int64_t spite_temp_345; if (__builtin_expect(__builtin_sub_overflow(spite_temp_343, spite_temp_344, &spite_temp_345), 0)) spite_overflowed("position - 1", "a Long", "-", (int64_t)spite_temp_343, (int64_t)spite_temp_344, spite_site_49()); spite_temp_345; }); if (__builtin_expect(spite_temp_342 < INT32_MIN || spite_temp_342 > INT32_MAX, 0)) spite_narrowed((int64_t)spite_temp_342, "a Long", "an Integer", spite_site_49()); (int32_t)spite_temp_342; }), SchedulerLoop___retain(value_));
}
Lock_unlock(self->_lock_);
SchedulerLoop___release(value_);
}
int64_t ThreadLocal__SchedulerLoop__make_room(ThreadLocal__SchedulerLoop* self) {
int64_t items_ = ((int64_t)(SpiteMemory_Address_read_long_atomically(self->_published_, SpiteInteger_to_long(0))));
if (((self->_count_ == self->_capacity_))) {
int32_t grown_ = ({ int32_t spite_temp_346 = self->_capacity_; int32_t spite_temp_347 = 2; int32_t spite_temp_348; if (__builtin_expect(__builtin_mul_overflow(spite_temp_346, spite_temp_347, &spite_temp_348), 0)) spite_overflowed("_capacity * 2", "an Integer", "*", (int64_t)spite_temp_346, (int64_t)spite_temp_347, spite_site_50()); spite_temp_348; });
if (((self->_capacity_ == 0))) {
grown_ = 4;
}
int64_t bytes_ = TypedMemory__SchedulerLoop_value_bytes(self->_values_);
int64_t larger_ = Memory_Heap_allocate(self->_heap_, ({ int64_t spite_temp_349 = bytes_; int64_t spite_temp_350 = SpiteInteger_to_long(grown_); int64_t spite_temp_351; if (__builtin_expect(__builtin_mul_overflow(spite_temp_349, spite_temp_350, &spite_temp_351), 0)) spite_overflowed("bytes * grown", "a Long", "*", (int64_t)spite_temp_349, (int64_t)spite_temp_350, spite_site_51()); spite_temp_351; }));
if (((items_ != ((int64_t)(0))))) {
SpiteMemory_Address_copy_to(items_, larger_, ({ int64_t spite_temp_352 = bytes_; int64_t spite_temp_353 = SpiteInteger_to_long(self->_count_); int64_t spite_temp_354; if (__builtin_expect(__builtin_mul_overflow(spite_temp_352, spite_temp_353, &spite_temp_354), 0)) spite_overflowed("bytes * _count", "a Long", "*", (int64_t)spite_temp_352, (int64_t)spite_temp_353, spite_site_52()); spite_temp_354; }));
List_Memory_Address_append(self->_retired_, items_);
}
SpiteMemory_Address_write_long_atomically(self->_published_, SpiteInteger_to_long(0), SpiteMemory_Address_to_long(larger_));
self->_capacity_ = grown_;
items_ = larger_;
}
int64_t spite_temp_355 = items_;
return spite_temp_355;
}
void ThreadLocal__SchedulerLoop_drop(ThreadLocal__SchedulerLoop* self) {
int64_t items_ = ((int64_t)(SpiteMemory_Address_read_long_atomically(self->_published_, SpiteInteger_to_long(0))));
if (((items_ != ((int64_t)(0))))) {
int32_t index_ = 0;
while (((index_ < self->_count_))) {
TypedMemory__SchedulerLoop_release_value(self->_values_, items_, index_);
index_ = (index_ + 1);
}
Memory_Heap_free(self->_heap_, items_);
}
while (((!(spite_folded_List_Memory_Address_is_empty(self->_retired_))))) {
Nullable_Memory_Address old_items_ = List_Memory_Address_remove_last(self->_retired_);
if (!((old_items_).has_value)) {
spite_failed_8(items_, self);
}
Memory_Heap_free(self->_heap_, (old_items_).value);
}
Memory_Heap_free(self->_heap_, self->_published_);
}
static SPITE_CRASH_REPORT void spite_failed_8(int64_t items_, ThreadLocal__SchedulerLoop* self) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_53(), stderr);
fputs("\told_items is null", stderr);
fputs("\titems=", stderr);
{ SpiteString spite_temp_356 = SpiteMemory_Address_to_string(items_); spite_crash_text(spite_string_bytes(&spite_temp_356), spite_string_length(spite_temp_356)); SpiteString___release(spite_temp_356); }
fputs("\t_published=", stderr);
{ SpiteString spite_temp_357 = SpiteMemory_Address_to_string(self->_published_); spite_crash_text(spite_string_bytes(&spite_temp_357), spite_string_length(spite_temp_357)); SpiteString___release(spite_temp_357); }
fputs("\t_count=", stderr);
{ SpiteString spite_temp_358 = SpiteInteger_to_string(self->_count_); spite_crash_text(spite_string_bytes(&spite_temp_358), spite_string_length(spite_temp_358)); SpiteString___release(spite_temp_358); }
fputs("\t_capacity=", stderr);
{ SpiteString spite_temp_359 = SpiteInteger_to_string(self->_capacity_); spite_crash_text(spite_string_bytes(&spite_temp_359), spite_string_length(spite_temp_359)); SpiteString___release(spite_temp_359); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
SchedulerLoop* TypedMemory__SchedulerLoop_read_value(TypedMemory__SchedulerLoop* self, int64_t address_, int32_t index_) {
return SchedulerLoop___retain(((SchedulerLoop**)(intptr_t)address_)[index_]);
}
void TypedMemory__SchedulerLoop_write_value(TypedMemory__SchedulerLoop* self, int64_t address_, int32_t index_, SchedulerLoop* value_) {
((SchedulerLoop**)(intptr_t)address_)[index_] = value_;
}
void TypedMemory__SchedulerLoop_release_value(TypedMemory__SchedulerLoop* self, int64_t address_, int32_t index_) {
SchedulerLoop___release(((SchedulerLoop**)(intptr_t)address_)[index_]);
}
int64_t TypedMemory__SchedulerLoop_value_bytes(TypedMemory__SchedulerLoop* self) {
return (int64_t)sizeof(SchedulerLoop*);
}
void List_SchedulerLoop_append(List_SchedulerLoop* self, SchedulerLoop* value_) {
List_SchedulerLoop_make_room(self);
TypedMemory__SchedulerLoop_write_value(self->values_, self->items_, self->item_count_, SchedulerLoop___retain(value_));
self->item_count_ = ({ int32_t spite_temp_360 = self->item_count_; int32_t spite_temp_361 = 1; int32_t spite_temp_362; if (__builtin_expect(__builtin_add_overflow(spite_temp_360, spite_temp_361, &spite_temp_362), 0)) spite_overflowed("item_count + 1", "an Integer", "+", (int64_t)spite_temp_360, (int64_t)spite_temp_361, spite_site_32()); spite_temp_362; });
SchedulerLoop___release(value_);
}
SchedulerLoop* List_SchedulerLoop_get_at(List_SchedulerLoop* self, int32_t index_) {
if ((((index_ >= 0))) && (((index_ < self->item_count_)))) {
SchedulerLoop* spite_temp_363 = TypedMemory__SchedulerLoop_read_value(self->values_, self->items_, index_);
return spite_temp_363;
}
SchedulerLoop* spite_temp_364 = 0;
return spite_temp_364;
}
void List_SchedulerLoop_drop(List_SchedulerLoop* self) {
List_SchedulerLoop_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_365 = SPITE_ALLOCATOR_List_SchedulerLoop(self, spite_singleton_Memory_Heap); int64_t spite_temp_366 = self->items_; if (((SpiteHeader*)(spite_temp_365))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_365), spite_temp_366); } else if (((SpiteHeader*)(spite_temp_365))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_365), spite_temp_366); } });
}
}
void List_SchedulerLoop_make_room(List_SchedulerLoop* self) {
if (((self->item_count_ == self->capacity_))) {
List_SchedulerLoop__grow(self);
}
}
void List_SchedulerLoop__grow(List_SchedulerLoop* self) {
int32_t grown_ = ({ int32_t spite_temp_367 = self->capacity_; int32_t spite_temp_368 = 2; int32_t spite_temp_369; if (__builtin_expect(__builtin_mul_overflow(spite_temp_367, spite_temp_368, &spite_temp_369), 0)) spite_overflowed("capacity * 2", "an Integer", "*", (int64_t)spite_temp_367, (int64_t)spite_temp_368, spite_site_42()); spite_temp_369; });
if (((self->capacity_ == 0))) {
grown_ = 4;
}
int64_t bytes_ = TypedMemory__SchedulerLoop_value_bytes(self->values_);
self->items_ = List_SchedulerLoop__resized(self, ({ int64_t spite_temp_370 = bytes_; int64_t spite_temp_371 = SpiteInteger_to_long(self->capacity_); int64_t spite_temp_372; if (__builtin_expect(__builtin_mul_overflow(spite_temp_370, spite_temp_371, &spite_temp_372), 0)) spite_overflowed("bytes * capacity", "a Long", "*", (int64_t)spite_temp_370, (int64_t)spite_temp_371, spite_site_43()); spite_temp_372; }), ({ int64_t spite_temp_373 = bytes_; int64_t spite_temp_374 = SpiteInteger_to_long(grown_); int64_t spite_temp_375; if (__builtin_expect(__builtin_mul_overflow(spite_temp_373, spite_temp_374, &spite_temp_375), 0)) spite_overflowed("bytes * grown", "a Long", "*", (int64_t)spite_temp_373, (int64_t)spite_temp_374, spite_site_43()); spite_temp_375; }));
self->capacity_ = grown_;
}
int64_t List_SchedulerLoop__resized(List_SchedulerLoop* self, int64_t old_bytes_, int64_t new_bytes_) {
if ((({ Spite_Allocator spite_temp_376 = SPITE_ALLOCATOR_List_SchedulerLoop(self, spite_singleton_Memory_Heap); bool spite_temp_377 = (((SpiteHeader*)(spite_temp_376))->class_id == 94); spite_temp_377; }))) {
int64_t spite_temp_378 = Memory_Heap_resize(self->heap_, self->items_, new_bytes_);
return spite_temp_378;
}
int64_t moved_ = ({ Spite_Allocator spite_temp_379 = SPITE_ALLOCATOR_List_SchedulerLoop(self, spite_singleton_Memory_Heap); int64_t spite_temp_380 = new_bytes_; int64_t spite_temp_381 = 0; if (((SpiteHeader*)(spite_temp_379))->class_id == 93) { spite_temp_381 = Memory_Arena_allocate(((Memory_Arena*)spite_temp_379), spite_temp_380); } else if (((SpiteHeader*)(spite_temp_379))->class_id == 94) { spite_temp_381 = Memory_Heap_allocate(((Memory_Heap*)spite_temp_379), spite_temp_380); } spite_temp_381; });
if (((self->items_ != ((int64_t)(0))))) {
SpiteMemory_Address_copy_to(self->items_, moved_, old_bytes_);
({ Spite_Allocator spite_temp_382 = SPITE_ALLOCATOR_List_SchedulerLoop(self, spite_singleton_Memory_Heap); int64_t spite_temp_383 = self->items_; if (((SpiteHeader*)(spite_temp_382))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_382), spite_temp_383); } else if (((SpiteHeader*)(spite_temp_382))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_382), spite_temp_383); } });
}
int64_t spite_temp_384 = moved_;
return spite_temp_384;
}
void List_Spite_AttributeDeclaration_drop(List_Spite_AttributeDeclaration* self) {
List_Spite_AttributeDeclaration_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_385 = SPITE_ALLOCATOR_List_Spite_AttributeDeclaration(self, spite_singleton_Memory_Heap); int64_t spite_temp_386 = self->items_; if (((SpiteHeader*)(spite_temp_385))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_385), spite_temp_386); } else if (((SpiteHeader*)(spite_temp_385))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_385), spite_temp_386); } });
}
}
void List_Spite_Function_drop(List_Spite_Function* self) {
List_Spite_Function_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_387 = SPITE_ALLOCATOR_List_Spite_Function(self, spite_singleton_Memory_Heap); int64_t spite_temp_388 = self->items_; if (((SpiteHeader*)(spite_temp_387))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_387), spite_temp_388); } else if (((SpiteHeader*)(spite_temp_387))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_387), spite_temp_388); } });
}
}
void List_Spite_Argument_drop(List_Spite_Argument* self) {
List_Spite_Argument_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_389 = SPITE_ALLOCATOR_List_Spite_Argument(self, spite_singleton_Memory_Heap); int64_t spite_temp_390 = self->items_; if (((SpiteHeader*)(spite_temp_389))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_389), spite_temp_390); } else if (((SpiteHeader*)(spite_temp_389))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_389), spite_temp_390); } });
}
}
void List_Spite_Class_drop(List_Spite_Class* self) {
List_Spite_Class_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_391 = SPITE_ALLOCATOR_List_Spite_Class(self, spite_singleton_Memory_Heap); int64_t spite_temp_392 = self->items_; if (((SpiteHeader*)(spite_temp_391))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_391), spite_temp_392); } else if (((SpiteHeader*)(spite_temp_391))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_391), spite_temp_392); } });
}
}
void List_Spite_Namespace_drop(List_Spite_Namespace* self) {
List_Spite_Namespace_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_393 = SPITE_ALLOCATOR_List_Spite_Namespace(self, spite_singleton_Memory_Heap); int64_t spite_temp_394 = self->items_; if (((SpiteHeader*)(spite_temp_393))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_393), spite_temp_394); } else if (((SpiteHeader*)(spite_temp_393))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_393), spite_temp_394); } });
}
}
Console_Printable List_Console_Printable_get_at(List_Console_Printable* self, int32_t index_) {
if ((((index_ >= 0))) && (((index_ < self->item_count_)))) {
Console_Printable spite_temp_395 = TypedMemory__Console_Printable_read_value(self->values_, self->items_, index_);
return spite_temp_395;
}
Console_Printable spite_temp_396 = SPITE_TAGGED_NULL;
return spite_temp_396;
}
void List_Console_Printable_drop(List_Console_Printable* self) {
List_Console_Printable_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_397 = SPITE_ALLOCATOR_List_Console_Printable(self, spite_singleton_Memory_Heap); int64_t spite_temp_398 = self->items_; if (((SpiteHeader*)(spite_temp_397))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_397), spite_temp_398); } else if (((SpiteHeader*)(spite_temp_397))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_397), spite_temp_398); } });
}
}
Console_Printable TypedMemory__Console_Printable_read_value(TypedMemory__Console_Printable* self, int64_t address_, int32_t index_) {
return Console_Printable___retain(((Console_Printable*)(intptr_t)address_)[index_]);
}
void List_Symbol_drop(List_Symbol* self) {
spite_folded_List_Symbol_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_399 = SPITE_ALLOCATOR_List_Symbol(self, spite_singleton_Memory_Heap); int64_t spite_temp_400 = self->items_; if (((SpiteHeader*)(spite_temp_399))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_399), spite_temp_400); } else if (((SpiteHeader*)(spite_temp_399))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_399), spite_temp_400); } });
}
}
void Concurrent__Nullable_String_Concurrent(Concurrent__Nullable_String* self, Spite_Function* starting_work_) {
Spite_Function* spite_temp_401 = Spite_Function___retain(starting_work_);
Spite_Function___release(self->_work_);
self->_work_ = spite_temp_401;
self->_frame_ = Concurrent__Nullable_String__start_frame(self);
if (((self->_frame_ == SpiteInteger_to_long(0)))) {
Concurrent__Nullable_String__run(self);
}
else {
self->_finished_ = false;
Scheduler_begin(self->_scheduler_, self->_frame_);
Concurrent__Nullable_String__collect(self);
}
Spite_Function* spite_temp_402 = 0;
Spite_Function___release(self->_work_);
self->_work_ = spite_temp_402;
Spite_Function___release(starting_work_);
}
void Concurrent__Nullable_String__run(Concurrent__Nullable_String* self) {
if (!(((self->_work_) != 0))) {
return;
}
SpiteString value_ = ({ Spite_Function* spite_temp_403 = self->_work_; SpiteString spite_temp_404 = ((SpiteString (*)(void*))spite_temp_403->spite_typed_call)(spite_temp_403->spite_owner); spite_temp_404; });
List_Nullable_String_append(self->_results_, SpiteString___retain(value_));
SpiteString___release(value_);
}
void Concurrent__Nullable_String__collect(Concurrent__Nullable_String* self) {
if (!(((!(self->_finished_))))) {
return;
}
if (!(((self->_frame_ != SpiteInteger_to_long(0))))) {
return;
}
if (!((Scheduler_frame_done(self->_scheduler_, ((int64_t)(self->_frame_)))))) {
return;
}
SpiteString value_ = Concurrent__Nullable_String__frame_result(self);
List_Nullable_String_append(self->_results_, SpiteString___retain(value_));
self->_finished_ = true;
SpiteString___release(value_);
}
SpiteString Concurrent__Nullable_String__result(Concurrent__Nullable_String* self) {
Concurrent__Nullable_String__join(self);
int32_t spite_crash_value_1; bool spite_crash_set_1 = false;
if (!((((spite_crash_set_1 = true, spite_crash_value_1 = (spite_folded_List_Nullable_String_count(self->_results_))) > 0)))) {
spite_failed_9(spite_crash_set_1, spite_crash_value_1, self);
}
SpiteString spite_temp_405 = spite_folded_List_Nullable_String_get_at(self->_results_, 0);
return spite_temp_405;
}
static SPITE_CRASH_REPORT void spite_failed_9(bool spite_crash_set_1, int32_t spite_crash_value_1, Concurrent__Nullable_String* self) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_54(), stderr);
if (spite_crash_set_1) {
fputs("\t_results.count()=", stderr);
{ SpiteString spite_temp_406 = SpiteInteger_to_string(spite_crash_value_1); fwrite(spite_string_bytes(&spite_temp_406), 1, (size_t)spite_string_length(spite_temp_406), stderr); SpiteString___release(spite_temp_406); }
}
fputs("\t_frame=", stderr);
{ SpiteString spite_temp_407 = SpiteLong_to_string(self->_frame_); spite_crash_text(spite_string_bytes(&spite_temp_407), spite_string_length(spite_temp_407)); SpiteString___release(spite_temp_407); }
fputs("\t_finished=", stderr);
{ SpiteString spite_temp_408 = SpiteBoolean_to_string(self->_finished_); spite_crash_text(spite_string_bytes(&spite_temp_408), spite_string_length(spite_temp_408)); SpiteString___release(spite_temp_408); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
void Concurrent__Nullable_String__join(Concurrent__Nullable_String* self) {
if (((!(self->_finished_)))) {
bool waits_for_its_own_caller_ = (!(Scheduler_wait_for(self->_scheduler_, self->_frame_)));
if (!(((!(waits_for_its_own_caller_))))) {
spite_failed_10(waits_for_its_own_caller_, self);
}
Concurrent__Nullable_String__collect(self);
}
}
static SPITE_CRASH_REPORT void spite_failed_10(bool waits_for_its_own_caller_, Concurrent__Nullable_String* self) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_55(), stderr);
fputs("\twaits_for_its_own_caller=", stderr);
{ SpiteString spite_temp_409 = SpiteBoolean_to_string(waits_for_its_own_caller_); fwrite(spite_string_bytes(&spite_temp_409), 1, (size_t)spite_string_length(spite_temp_409), stderr); SpiteString___release(spite_temp_409); }
fputs("\t_frame=", stderr);
{ SpiteString spite_temp_410 = SpiteLong_to_string(self->_frame_); spite_crash_text(spite_string_bytes(&spite_temp_410), spite_string_length(spite_temp_410)); SpiteString___release(spite_temp_410); }
fputs("\t_finished=", stderr);
{ SpiteString spite_temp_411 = SpiteBoolean_to_string(self->_finished_); spite_crash_text(spite_string_bytes(&spite_temp_411), spite_string_length(spite_temp_411)); SpiteString___release(spite_temp_411); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
void Concurrent__Nullable_String_drop(Concurrent__Nullable_String* self) {
Concurrent__Nullable_String__join(self);
Concurrent__Nullable_String__free_frame(self);
}
int64_t Concurrent__Nullable_String__start_frame(Concurrent__Nullable_String* self) {
return (int64_t)(intptr_t)spite_frame_start(self->_work_);
}
SpiteString Concurrent__Nullable_String__frame_result(Concurrent__Nullable_String* self) {
return ((struct { SpiteFrame spite_head; SpiteString spite_result; }*)(void*)(intptr_t)self->_frame_)->spite_result;
}
void Concurrent__Nullable_String__free_frame(Concurrent__Nullable_String* self) {
if (self->_frame_ != 0) SPITE_FREE((void*)(intptr_t)self->_frame_);
self->_frame_ = 0;
}
void List_Nullable_String_append(List_Nullable_String* self, SpiteString value_) {
List_Nullable_String_make_room(self);
spite_folded_TypedMemory__Nullable_String_write_value(self->values_, self->items_, self->item_count_, SpiteString___retain(value_));
self->item_count_ = ({ int32_t spite_temp_412 = self->item_count_; int32_t spite_temp_413 = 1; int32_t spite_temp_414; if (__builtin_expect(__builtin_add_overflow(spite_temp_412, spite_temp_413, &spite_temp_414), 0)) spite_overflowed("item_count + 1", "an Integer", "+", (int64_t)spite_temp_412, (int64_t)spite_temp_413, spite_site_32()); spite_temp_414; });
SpiteString___release(value_);
}
void List_Nullable_String_drop(List_Nullable_String* self) {
spite_folded_List_Nullable_String_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_415 = SPITE_ALLOCATOR_List_Nullable_String(self, spite_singleton_Memory_Heap); int64_t spite_temp_416 = self->items_; if (((SpiteHeader*)(spite_temp_415))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_415), spite_temp_416); } else if (((SpiteHeader*)(spite_temp_415))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_415), spite_temp_416); } });
}
}
void List_Nullable_String_make_room(List_Nullable_String* self) {
if (((self->item_count_ == self->capacity_))) {
List_Nullable_String__grow(self);
}
}
void List_Nullable_String__grow(List_Nullable_String* self) {
int32_t grown_ = ({ int32_t spite_temp_417 = self->capacity_; int32_t spite_temp_418 = 2; int32_t spite_temp_419; if (__builtin_expect(__builtin_mul_overflow(spite_temp_417, spite_temp_418, &spite_temp_419), 0)) spite_overflowed("capacity * 2", "an Integer", "*", (int64_t)spite_temp_417, (int64_t)spite_temp_418, spite_site_42()); spite_temp_419; });
if (((self->capacity_ == 0))) {
grown_ = 4;
}
int64_t bytes_ = spite_folded_TypedMemory__Nullable_String_value_bytes(self->values_);
self->items_ = List_Nullable_String__resized(self, ({ int64_t spite_temp_420 = bytes_; int64_t spite_temp_421 = SpiteInteger_to_long(self->capacity_); int64_t spite_temp_422; if (__builtin_expect(__builtin_mul_overflow(spite_temp_420, spite_temp_421, &spite_temp_422), 0)) spite_overflowed("bytes * capacity", "a Long", "*", (int64_t)spite_temp_420, (int64_t)spite_temp_421, spite_site_43()); spite_temp_422; }), ({ int64_t spite_temp_423 = bytes_; int64_t spite_temp_424 = SpiteInteger_to_long(grown_); int64_t spite_temp_425; if (__builtin_expect(__builtin_mul_overflow(spite_temp_423, spite_temp_424, &spite_temp_425), 0)) spite_overflowed("bytes * grown", "a Long", "*", (int64_t)spite_temp_423, (int64_t)spite_temp_424, spite_site_43()); spite_temp_425; }));
self->capacity_ = grown_;
}
int64_t List_Nullable_String__resized(List_Nullable_String* self, int64_t old_bytes_, int64_t new_bytes_) {
if ((({ Spite_Allocator spite_temp_426 = SPITE_ALLOCATOR_List_Nullable_String(self, spite_singleton_Memory_Heap); bool spite_temp_427 = (((SpiteHeader*)(spite_temp_426))->class_id == 94); spite_temp_427; }))) {
int64_t spite_temp_428 = Memory_Heap_resize(self->heap_, self->items_, new_bytes_);
return spite_temp_428;
}
int64_t moved_ = ({ Spite_Allocator spite_temp_429 = SPITE_ALLOCATOR_List_Nullable_String(self, spite_singleton_Memory_Heap); int64_t spite_temp_430 = new_bytes_; int64_t spite_temp_431 = 0; if (((SpiteHeader*)(spite_temp_429))->class_id == 93) { spite_temp_431 = Memory_Arena_allocate(((Memory_Arena*)spite_temp_429), spite_temp_430); } else if (((SpiteHeader*)(spite_temp_429))->class_id == 94) { spite_temp_431 = Memory_Heap_allocate(((Memory_Heap*)spite_temp_429), spite_temp_430); } spite_temp_431; });
if (((self->items_ != ((int64_t)(0))))) {
SpiteMemory_Address_copy_to(self->items_, moved_, old_bytes_);
({ Spite_Allocator spite_temp_432 = SPITE_ALLOCATOR_List_Nullable_String(self, spite_singleton_Memory_Heap); int64_t spite_temp_433 = self->items_; if (((SpiteHeader*)(spite_temp_432))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_432), spite_temp_433); } else if (((SpiteHeader*)(spite_temp_432))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_432), spite_temp_433); } });
}
int64_t spite_temp_434 = moved_;
return spite_temp_434;
}
Nullable_Memory_Address List_Memory_Address_remove_last(List_Memory_Address* self) {
if (((self->item_count_ > 0))) {
int64_t last_item_ = spite_folded_TypedMemory__Memory_Address_read_value(self->values_, self->items_, ({ int32_t spite_temp_435 = self->item_count_; int32_t spite_temp_436 = 1; int32_t spite_temp_437; if (__builtin_expect(__builtin_sub_overflow(spite_temp_435, spite_temp_436, &spite_temp_437), 0)) spite_overflowed("item_count - 1", "an Integer", "-", (int64_t)spite_temp_435, (int64_t)spite_temp_436, spite_site_56()); spite_temp_437; }));
spite_folded_TypedMemory__Memory_Address_release_value(self->values_, self->items_, ({ int32_t spite_temp_438 = self->item_count_; int32_t spite_temp_439 = 1; int32_t spite_temp_440; if (__builtin_expect(__builtin_sub_overflow(spite_temp_438, spite_temp_439, &spite_temp_440), 0)) spite_overflowed("item_count - 1", "an Integer", "-", (int64_t)spite_temp_438, (int64_t)spite_temp_439, spite_site_57()); spite_temp_440; }));
self->item_count_ = ({ int32_t spite_temp_441 = self->item_count_; int32_t spite_temp_442 = 1; int32_t spite_temp_443; if (__builtin_expect(__builtin_sub_overflow(spite_temp_441, spite_temp_442, &spite_temp_443), 0)) spite_overflowed("item_count - 1", "an Integer", "-", (int64_t)spite_temp_441, (int64_t)spite_temp_442, spite_site_58()); spite_temp_443; });
Nullable_Memory_Address spite_temp_444 = ((Nullable_Memory_Address){ .has_value = true, .value = last_item_ });
return spite_temp_444;
}
Nullable_Memory_Address spite_temp_445 = ((Nullable_Memory_Address){ .has_value = false, .value = 0 });
return spite_temp_445;
}
void List_SchedulerLoop_clear(List_SchedulerLoop* self) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
TypedMemory__SchedulerLoop_release_value(self->values_, self->items_, index_);
index_ = (index_ + 1);
}
self->item_count_ = 0;
}
void List_Spite_AttributeDeclaration_clear(List_Spite_AttributeDeclaration* self) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
TypedMemory__Spite_AttributeDeclaration_release_value(self->values_, self->items_, index_);
index_ = (index_ + 1);
}
self->item_count_ = 0;
}
void TypedMemory__Spite_AttributeDeclaration_release_value(TypedMemory__Spite_AttributeDeclaration* self, int64_t address_, int32_t index_) {
Spite_AttributeDeclaration___release(((Spite_AttributeDeclaration**)(intptr_t)address_)[index_]);
}
void List_Spite_Function_clear(List_Spite_Function* self) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
TypedMemory__Spite_Function_release_value(self->values_, self->items_, index_);
index_ = (index_ + 1);
}
self->item_count_ = 0;
}
void TypedMemory__Spite_Function_release_value(TypedMemory__Spite_Function* self, int64_t address_, int32_t index_) {
Spite_Function___release(((Spite_Function**)(intptr_t)address_)[index_]);
}
void List_Spite_Argument_clear(List_Spite_Argument* self) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
TypedMemory__Spite_Argument_release_value(self->values_, self->items_, index_);
index_ = (index_ + 1);
}
self->item_count_ = 0;
}
void TypedMemory__Spite_Argument_release_value(TypedMemory__Spite_Argument* self, int64_t address_, int32_t index_) {
Spite_Argument___release(((Spite_Argument**)(intptr_t)address_)[index_]);
}
void List_Spite_Class_clear(List_Spite_Class* self) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
TypedMemory__Spite_Class_release_value(self->values_, self->items_, index_);
index_ = (index_ + 1);
}
self->item_count_ = 0;
}
void TypedMemory__Spite_Class_release_value(TypedMemory__Spite_Class* self, int64_t address_, int32_t index_) {
Spite_Class___release(((Spite_Class**)(intptr_t)address_)[index_]);
}
void List_Spite_Namespace_clear(List_Spite_Namespace* self) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
TypedMemory__Spite_Namespace_release_value(self->values_, self->items_, index_);
index_ = (index_ + 1);
}
self->item_count_ = 0;
}
void TypedMemory__Spite_Namespace_release_value(TypedMemory__Spite_Namespace* self, int64_t address_, int32_t index_) {
Spite_Namespace___release(((Spite_Namespace**)(intptr_t)address_)[index_]);
}
void List_Console_Printable_clear(List_Console_Printable* self) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
TypedMemory__Console_Printable_release_value(self->values_, self->items_, index_);
index_ = (index_ + 1);
}
self->item_count_ = 0;
}
void TypedMemory__Console_Printable_release_value(TypedMemory__Console_Printable* self, int64_t address_, int32_t index_) {
Console_Printable___release(((Console_Printable*)(intptr_t)address_)[index_]);
}
static void* File_read___begin(File* self) {
File_read___frame* spite_frame = (File_read___frame*)SPITE_MALLOC(sizeof(File_read___frame));
memset(spite_frame, 0, sizeof(File_read___frame));
spite_frame->spite_head.step = File_read___step;
spite_frame->self = self;
return spite_frame;
}
static bool File_read___step(void* spite_raw) {
File_read___frame* spite_frame = (File_read___frame*)spite_raw;
File* self = spite_frame->self;
switch (spite_frame->spite_head.state) { case 2: goto spite_resume_1; default: break; }
spite_frame->handle_ = File_open_file(self, spite_lit_23);
if (!(((spite_frame->handle_ != SpiteInteger_to_long(0))))) {
{ spite_frame->spite_result = SPITE_STRING_NULL; return true; }
}
File_seek(self, spite_frame->handle_, SpiteInteger_to_long(0), 2);
spite_frame->length_ = File_tell(self, spite_frame->handle_);
File_seek(self, spite_frame->handle_, SpiteInteger_to_long(0), 0);
spite_frame->buffer_ = Memory_Heap_allocate(self->heap_, ({ int64_t spite_temp_446 = spite_frame->length_; int64_t spite_temp_447 = SpiteInteger_to_long(1); int64_t spite_temp_448; if (__builtin_expect(__builtin_add_overflow(spite_temp_446, spite_temp_447, &spite_temp_448), 0)) spite_overflowed("length + 1", "a Long", "+", (int64_t)spite_temp_446, (int64_t)spite_temp_447, spite_site_3()); spite_temp_448; }));
spite_frame->spite_wait_2 = File_read_into___begin(self, SpiteMemory_Address_to_long(spite_frame->buffer_), spite_frame->length_, spite_frame->handle_);
spite_resume_1: if (!File_read_into___step(spite_frame->spite_wait_2)) SPITE_SUSPEND(2);
spite_frame->spite_temp_1 = ((File_read_into___frame*)spite_frame->spite_wait_2)->spite_result;
SPITE_FREE(spite_frame->spite_wait_2);
spite_frame->got_ = spite_frame->spite_temp_1;
File_close_file(self, spite_frame->handle_);
spite_frame->content_ = SpiteMemory_Address_text(spite_frame->buffer_, spite_frame->got_);
Memory_Heap_free(self->heap_, spite_frame->buffer_);
spite_frame->spite_temp_2 = SpiteString___retain(spite_frame->content_);
SpiteString___release(spite_frame->content_);
{ spite_frame->spite_result = spite_frame->spite_temp_2; return true; }
return true;
}
static void* spite_frame_start(Spite_Function* spite_work) {
if (spite_work == 0) return 0;
if (spite_work->spite_typed_call == (void*)File_read) { SpiteFrame* spite_started = (SpiteFrame*)File_read___begin(spite_work->spite_owner); spite_started->work = spite_work; Spite_Function___retain(spite_work); return spite_started; }
return 0;
}
int64_t File_read_into(File* self, int64_t buffer_, int64_t bytes_, int64_t handle_) {
Scheduler* spite_scheduler = spite_singleton_Scheduler();
if (SPITE_GUARDS_HELD() != 0 || !Scheduler_waits_here(spite_scheduler)) { Scheduler___release(spite_scheduler); return File_read_into___waiting(self, buffer_, bytes_, handle_); }
File_read_into___call spite_call = { { 0, &File_read_into___perform, spite_scheduler }, self, buffer_, bytes_, handle_ };
if (!Scheduler_offload(spite_scheduler, (int64_t)(intptr_t)&spite_call, (int64_t)(intptr_t)&spite_offload_thread)) { Scheduler___release(spite_scheduler); return File_read_into___waiting(self, buffer_, bytes_, handle_); }
Scheduler___release(spite_scheduler);
return spite_call.result;
}
int64_t File_write_text(File* self, SpiteString text_, int64_t handle_) {
Scheduler* spite_scheduler = spite_singleton_Scheduler();
if (SPITE_GUARDS_HELD() != 0 || !Scheduler_waits_here(spite_scheduler)) { Scheduler___release(spite_scheduler); return File_write_text___waiting(self, text_, handle_); }
File_write_text___call spite_call = { { 0, &File_write_text___perform, spite_scheduler }, self, text_, handle_ };
if (!Scheduler_offload(spite_scheduler, (int64_t)(intptr_t)&spite_call, (int64_t)(intptr_t)&spite_offload_thread)) { Scheduler___release(spite_scheduler); return File_write_text___waiting(self, text_, handle_); }
Scheduler___release(spite_scheduler);
return spite_call.result;
}
static void* File_read_into___begin(File* self, int64_t buffer_, int64_t bytes_, int64_t handle_) {
File_read_into___frame* spite_frame = (File_read_into___frame*)SPITE_MALLOC(sizeof(File_read_into___frame));
memset(spite_frame, 0, sizeof(File_read_into___frame));
spite_frame->spite_head.step = File_read_into___step;
spite_frame->self = self;
spite_frame->buffer_ = buffer_;
spite_frame->bytes_ = bytes_;
spite_frame->handle_ = handle_;
return spite_frame;
}
static bool File_read_into___step(void* spite_raw) {
File_read_into___frame* spite_frame = (File_read_into___frame*)spite_raw;
Scheduler* spite_scheduler = spite_singleton_Scheduler();
if (spite_frame->spite_head.state == 0) {
spite_frame->spite_call = (File_read_into___call){ { 0, &File_read_into___perform, spite_scheduler }, spite_frame->self, spite_frame->buffer_, spite_frame->bytes_, spite_frame->handle_ };
spite_frame->spite_thread = Scheduler_offload_start(spite_scheduler, (int64_t)(intptr_t)&spite_frame->spite_call, (int64_t)(intptr_t)&spite_offload_thread);
spite_frame->spite_head.state = 1;
}
bool spite_over = Scheduler_offload_over(spite_scheduler, (int64_t)(intptr_t)&spite_frame->spite_call, spite_frame->spite_thread);
if (spite_over) spite_frame->spite_result = spite_frame->spite_call.result;
Scheduler___release(spite_scheduler);
return spite_over;
}
typedef struct SpiteFunctionPlace { const void* start; const char* owner; const char* name; int32_t line; } SpiteFunctionPlace;
int main(int argument_count, char** argument_values);
static const SpiteFunctionPlace spite_function_places[] = {
{(const void*)&spite_crash_text, "-\t-", "spite_crash_text", 0},
{(const void*)&spite_crash_begin, "-\t-", "spite_crash_begin", 0},
{(const void*)&spite_report_assert_trace, "-\t-", "spite_report_assert_trace", 0},
{(const void*)&spite_string_length, "-\t-", "spite_string_length", 0},
{(const void*)&spite_string_bytes, "-\t-", "spite_string_bytes", 0},
{(const void*)&spite_string_code_at, "-\t-", "spite_string_code_at", 0},
{(const void*)&spite_described_enter, "-\t-", "spite_described_enter", 0},
{(const void*)&spite_described_leave, "-\t-", "spite_described_leave", 0},
{(const void*)&spite_singleton_Memory_Heap, "-\t-", "spite_singleton_Memory_Heap", 0},
{(const void*)&spite_singleton_TypedMemory__String, "-\t-", "spite_singleton_TypedMemory__String", 0},
{(const void*)&Console_Printable___retain, "-\t-", "Console_Printable___retain", 0},
{(const void*)&spite_singleton_Build, "-\t-", "spite_singleton_Build", 0},
{(const void*)&spite_singleton_Console_teardown, "-\t-", "spite_singleton_Console_teardown", 0},
{(const void*)&spite_singleton_Console, "-\t-", "spite_singleton_Console", 0},
{(const void*)&spite_singleton_TypedMemory__Long, "-\t-", "spite_singleton_TypedMemory__Long", 0},
{(const void*)&spite_singleton_Clock_teardown, "-\t-", "spite_singleton_Clock_teardown", 0},
{(const void*)&spite_singleton_Clock, "-\t-", "spite_singleton_Clock", 0},
{(const void*)&spite_singleton_TypedMemory__Memory_Address, "-\t-", "spite_singleton_TypedMemory__Memory_Address", 0},
{(const void*)&spite_singleton_Scheduler_teardown, "-\t-", "spite_singleton_Scheduler_teardown", 0},
{(const void*)&spite_singleton_Scheduler, "-\t-", "spite_singleton_Scheduler", 0},
{(const void*)&spite_singleton_TypedMemory__SchedulerLoop, "-\t-", "spite_singleton_TypedMemory__SchedulerLoop", 0},
{(const void*)&spite_singleton_TypedMemory__Spite_AttributeDeclaration, "-\t-", "spite_singleton_TypedMemory__Spite_AttributeDeclaration", 0},
{(const void*)&spite_singleton_TypedMemory__Spite_Function, "-\t-", "spite_singleton_TypedMemory__Spite_Function", 0},
{(const void*)&spite_singleton_TypedMemory__Spite_Argument, "-\t-", "spite_singleton_TypedMemory__Spite_Argument", 0},
{(const void*)&Launcher___init, "-\t-", "Launcher___init", 0},
{(const void*)&Launcher___allocate, "-\t-", "Launcher___allocate", 0},
{(const void*)&Launcher___release, "-\t-", "Launcher___release", 0},
{(const void*)&Launcher___free, "-\t-", "Launcher___free", 0},
{(const void*)&spite_overflowed, "-\t-", "spite_overflowed", 0},
{(const void*)&spite_singleton_TypedMemory__Console_Printable, "-\t-", "spite_singleton_TypedMemory__Console_Printable", 0},
{(const void*)&List_Console_Printable___framed, "-\t-", "List_Console_Printable___framed", 0},
{(const void*)&spite_box_SpiteString, "-\t-", "spite_box_SpiteString", 0},
{(const void*)&spite_string_box_release, "-\t-", "spite_string_box_release", 0},
{(const void*)&spite_tagged_SpiteLong, "-\t-", "spite_tagged_SpiteLong", 0},
{(const void*)&spite_narrowed, "-\t-", "spite_narrowed", 0},
{(const void*)&spite_divided_by_zero, "-\t-", "spite_divided_by_zero", 0},
{(const void*)&spite_outside_list, "-\t-", "spite_outside_list", 0},
{(const void*)&Clock___init, "-\t-", "Clock___init", 0},
{(const void*)&Clock___allocate, "-\t-", "Clock___allocate", 0},
{(const void*)&Clock___make, "-\t-", "Clock___make", 0},
{(const void*)&Clock___destroy, "-\t-", "Clock___destroy", 0},
{(const void*)&Clock___discard, "-\t-", "Clock___discard", 0},
{(const void*)&Console___init, "-\t-", "Console___init", 0},
{(const void*)&Console___allocate, "-\t-", "Console___allocate", 0},
{(const void*)&Console___make, "-\t-", "Console___make", 0},
{(const void*)&Console___destroy, "-\t-", "Console___destroy", 0},
{(const void*)&Console___discard, "-\t-", "Console___discard", 0},
{(const void*)&File___framed, "-\t-", "File___framed", 0},
{(const void*)&File___unframe, "-\t-", "File___unframe", 0},
{(const void*)&File___make_into, "-\t-", "File___make_into", 0},
{(const void*)&DynamicLibrary___init, "-\t-", "DynamicLibrary___init", 0},
{(const void*)&DynamicLibrary___allocate, "-\t-", "DynamicLibrary___allocate", 0},
{(const void*)&DynamicLibrary___make, "-\t-", "DynamicLibrary___make", 0},
{(const void*)&DynamicLibrary___destroy, "-\t-", "DynamicLibrary___destroy", 0},
{(const void*)&DynamicLibrary___discard, "-\t-", "DynamicLibrary___discard", 0},
{(const void*)&File___init, "-\t-", "File___init", 0},
{(const void*)&File___retain, "-\t-", "File___retain", 0},
{(const void*)&File___release, "-\t-", "File___release", 0},
{(const void*)&File___free, "-\t-", "File___free", 0},
{(const void*)&Lock___init, "-\t-", "Lock___init", 0},
{(const void*)&Lock___allocate, "-\t-", "Lock___allocate", 0},
{(const void*)&Lock___make, "-\t-", "Lock___make", 0},
{(const void*)&Lock___release, "-\t-", "Lock___release", 0},
{(const void*)&Lock___free, "-\t-", "Lock___free", 0},
{(const void*)&Scheduler___init, "-\t-", "Scheduler___init", 0},
{(const void*)&Scheduler___allocate, "-\t-", "Scheduler___allocate", 0},
{(const void*)&Scheduler___make, "-\t-", "Scheduler___make", 0},
{(const void*)&Scheduler___destroy, "-\t-", "Scheduler___destroy", 0},
{(const void*)&Scheduler___discard, "-\t-", "Scheduler___discard", 0},
{(const void*)&SchedulerLoop___init, "-\t-", "SchedulerLoop___init", 0},
{(const void*)&SchedulerLoop___allocate, "-\t-", "SchedulerLoop___allocate", 0},
{(const void*)&SchedulerLoop___make, "-\t-", "SchedulerLoop___make", 0},
{(const void*)&SchedulerLoop___retain, "-\t-", "SchedulerLoop___retain", 0},
{(const void*)&SchedulerLoop___release, "-\t-", "SchedulerLoop___release", 0},
{(const void*)&SchedulerLoop___free, "-\t-", "SchedulerLoop___free", 0},
{(const void*)&spite_long_digits, "-\t-", "spite_long_digits", 0},
{(const void*)&spite_string_block, "-\t-", "spite_string_block", 0},
{(const void*)&spite_string_held, "-\t-", "spite_string_held", 0},
{(const void*)&SpiteString___retain, "-\t-", "SpiteString___retain", 0},
{(const void*)&SpiteString___release, "-\t-", "SpiteString___release", 0},
{(const void*)&spite_string_from_bytes, "-\t-", "spite_string_from_bytes", 0},
{(const void*)&spite_string_join, "-\t-", "spite_string_join", 0},
{(const void*)&ThreadSlot___init, "-\t-", "ThreadSlot___init", 0},
{(const void*)&ThreadSlot___allocate, "-\t-", "ThreadSlot___allocate", 0},
{(const void*)&ThreadSlot___make, "-\t-", "ThreadSlot___make", 0},
{(const void*)&ThreadSlot___release, "-\t-", "ThreadSlot___release", 0},
{(const void*)&ThreadSlot___free, "-\t-", "ThreadSlot___free", 0},
{(const void*)&Spite_Argument___release, "-\t-", "Spite_Argument___release", 0},
{(const void*)&Spite_Argument___free, "-\t-", "Spite_Argument___free", 0},
{(const void*)&Spite_AttributeDeclaration___release, "-\t-", "Spite_AttributeDeclaration___release", 0},
{(const void*)&Spite_AttributeDeclaration___free, "-\t-", "Spite_AttributeDeclaration___free", 0},
{(const void*)&Spite_Class___init, "-\t-", "Spite_Class___init", 0},
{(const void*)&Spite_Class___allocate, "-\t-", "Spite_Class___allocate", 0},
{(const void*)&Spite_Class___make, "-\t-", "Spite_Class___make", 0},
{(const void*)&Spite_Class___retain, "-\t-", "Spite_Class___retain", 0},
{(const void*)&Spite_Class___release, "-\t-", "Spite_Class___release", 0},
{(const void*)&Spite_Class___free, "-\t-", "Spite_Class___free", 0},
{(const void*)&Spite_Function___init_constructed, "-\t-", "Spite_Function___init_constructed", 0},
{(const void*)&Spite_Function___allocate_constructed, "-\t-", "Spite_Function___allocate_constructed", 0},
{(const void*)&Spite_Function___make, "-\t-", "Spite_Function___make", 0},
{(const void*)&Spite_Function___retain, "-\t-", "Spite_Function___retain", 0},
{(const void*)&Spite_Function___release, "-\t-", "Spite_Function___release", 0},
{(const void*)&Spite_Function___free, "-\t-", "Spite_Function___free", 0},
{(const void*)&Spite_Namespace___release, "-\t-", "Spite_Namespace___release", 0},
{(const void*)&Spite_Namespace___free, "-\t-", "Spite_Namespace___free", 0},
{(const void*)&Naive___init, "-\t-", "Naive___init", 0},
{(const void*)&Naive___allocate, "-\t-", "Naive___allocate", 0},
{(const void*)&Naive___release, "-\t-", "Naive___release", 0},
{(const void*)&Naive___free, "-\t-", "Naive___free", 0},
{(const void*)&spite_tagged_SpiteBoolean, "-\t-", "spite_tagged_SpiteBoolean", 0},
{(const void*)&File_read___dropping_call, "-\t-", "File_read___dropping_call", 0},
{(const void*)&spite_function_value_File_read, "-\t-", "spite_function_value_File_read", 0},
{(const void*)&spite_singleton_TypedMemory__Nullable_String, "-\t-", "spite_singleton_TypedMemory__Nullable_String", 0},
{(const void*)&List_String___init, "-\t-", "List_String___init", 0},
{(const void*)&List_String___allocate, "-\t-", "List_String___allocate", 0},
{(const void*)&List_String___make, "-\t-", "List_String___make", 0},
{(const void*)&List_String___release, "-\t-", "List_String___release", 0},
{(const void*)&List_String___free, "-\t-", "List_String___free", 0},
{(const void*)&List_Long___init, "-\t-", "List_Long___init", 0},
{(const void*)&List_Long___allocate, "-\t-", "List_Long___allocate", 0},
{(const void*)&List_Long___make, "-\t-", "List_Long___make", 0},
{(const void*)&List_Long___retain, "-\t-", "List_Long___retain", 0},
{(const void*)&List_Long___release, "-\t-", "List_Long___release", 0},
{(const void*)&List_Long___free, "-\t-", "List_Long___free", 0},
{(const void*)&List_Memory_Address___init, "-\t-", "List_Memory_Address___init", 0},
{(const void*)&List_Memory_Address___allocate, "-\t-", "List_Memory_Address___allocate", 0},
{(const void*)&List_Memory_Address___make, "-\t-", "List_Memory_Address___make", 0},
{(const void*)&List_Memory_Address___release, "-\t-", "List_Memory_Address___release", 0},
{(const void*)&List_Memory_Address___free, "-\t-", "List_Memory_Address___free", 0},
{(const void*)&ThreadLocal__SchedulerLoop___init, "-\t-", "ThreadLocal__SchedulerLoop___init", 0},
{(const void*)&ThreadLocal__SchedulerLoop___allocate, "-\t-", "ThreadLocal__SchedulerLoop___allocate", 0},
{(const void*)&ThreadLocal__SchedulerLoop___make, "-\t-", "ThreadLocal__SchedulerLoop___make", 0},
{(const void*)&ThreadLocal__SchedulerLoop___release, "-\t-", "ThreadLocal__SchedulerLoop___release", 0},
{(const void*)&ThreadLocal__SchedulerLoop___free, "-\t-", "ThreadLocal__SchedulerLoop___free", 0},
{(const void*)&List_SchedulerLoop___init, "-\t-", "List_SchedulerLoop___init", 0},
{(const void*)&List_SchedulerLoop___allocate, "-\t-", "List_SchedulerLoop___allocate", 0},
{(const void*)&List_SchedulerLoop___make, "-\t-", "List_SchedulerLoop___make", 0},
{(const void*)&List_SchedulerLoop___release, "-\t-", "List_SchedulerLoop___release", 0},
{(const void*)&List_SchedulerLoop___free, "-\t-", "List_SchedulerLoop___free", 0},
{(const void*)&List_Spite_AttributeDeclaration___init, "-\t-", "List_Spite_AttributeDeclaration___init", 0},
{(const void*)&List_Spite_AttributeDeclaration___allocate, "-\t-", "List_Spite_AttributeDeclaration___allocate", 0},
{(const void*)&List_Spite_AttributeDeclaration___make, "-\t-", "List_Spite_AttributeDeclaration___make", 0},
{(const void*)&List_Spite_AttributeDeclaration___release, "-\t-", "List_Spite_AttributeDeclaration___release", 0},
{(const void*)&List_Spite_AttributeDeclaration___free, "-\t-", "List_Spite_AttributeDeclaration___free", 0},
{(const void*)&List_Spite_Function___init, "-\t-", "List_Spite_Function___init", 0},
{(const void*)&List_Spite_Function___allocate, "-\t-", "List_Spite_Function___allocate", 0},
{(const void*)&List_Spite_Function___make, "-\t-", "List_Spite_Function___make", 0},
{(const void*)&List_Spite_Function___release, "-\t-", "List_Spite_Function___release", 0},
{(const void*)&List_Spite_Function___free, "-\t-", "List_Spite_Function___free", 0},
{(const void*)&List_Spite_Argument___init, "-\t-", "List_Spite_Argument___init", 0},
{(const void*)&List_Spite_Argument___allocate, "-\t-", "List_Spite_Argument___allocate", 0},
{(const void*)&List_Spite_Argument___make, "-\t-", "List_Spite_Argument___make", 0},
{(const void*)&List_Spite_Argument___release, "-\t-", "List_Spite_Argument___release", 0},
{(const void*)&List_Spite_Argument___free, "-\t-", "List_Spite_Argument___free", 0},
{(const void*)&List_Spite_Class___release, "-\t-", "List_Spite_Class___release", 0},
{(const void*)&List_Spite_Class___free, "-\t-", "List_Spite_Class___free", 0},
{(const void*)&List_Spite_Namespace___release, "-\t-", "List_Spite_Namespace___release", 0},
{(const void*)&List_Spite_Namespace___free, "-\t-", "List_Spite_Namespace___free", 0},
{(const void*)&List_Console_Printable___init, "-\t-", "List_Console_Printable___init", 0},
{(const void*)&List_Console_Printable___retain, "-\t-", "List_Console_Printable___retain", 0},
{(const void*)&List_Console_Printable___release, "-\t-", "List_Console_Printable___release", 0},
{(const void*)&List_Console_Printable___free, "-\t-", "List_Console_Printable___free", 0},
{(const void*)&List_Symbol___release, "-\t-", "List_Symbol___release", 0},
{(const void*)&List_Symbol___free, "-\t-", "List_Symbol___free", 0},
{(const void*)&Concurrent__Nullable_String___init, "-\t-", "Concurrent__Nullable_String___init", 0},
{(const void*)&Concurrent__Nullable_String___allocate, "-\t-", "Concurrent__Nullable_String___allocate", 0},
{(const void*)&Concurrent__Nullable_String___make, "-\t-", "Concurrent__Nullable_String___make", 0},
{(const void*)&Concurrent__Nullable_String___release, "-\t-", "Concurrent__Nullable_String___release", 0},
{(const void*)&Concurrent__Nullable_String___free, "-\t-", "Concurrent__Nullable_String___free", 0},
{(const void*)&List_Nullable_String___init, "-\t-", "List_Nullable_String___init", 0},
{(const void*)&List_Nullable_String___allocate, "-\t-", "List_Nullable_String___allocate", 0},
{(const void*)&List_Nullable_String___make, "-\t-", "List_Nullable_String___make", 0},
{(const void*)&List_Nullable_String___release, "-\t-", "List_Nullable_String___release", 0},
{(const void*)&List_Nullable_String___free, "-\t-", "List_Nullable_String___free", 0},
{(const void*)&spite_class_object_Nullable_String, "-\t-", "spite_class_object_Nullable_String", 0},
{(const void*)&Console_Printable___release, "-\t-", "Console_Printable___release", 0},
{(const void*)&Console_Printable___call_to_string, "-\t-", "Console_Printable___call_to_string", 0},
{(const void*)&spite_foreign_library_1, "-\t-", "spite_foreign_library_1", 0},
{(const void*)&spite_foreign_library_2, "-\t-", "spite_foreign_library_2", 0},
{(const void*)&spite_offload_thread, "-\t-", "spite_offload_thread", 0},
{(const void*)&File_read_into___perform, "-\t-", "File_read_into___perform", 0},
{(const void*)&File_write_text___perform, "-\t-", "File_write_text___perform", 0},
{(const void*)&spite_singleton_check_circle, "-\t-", "spite_singleton_check_circle", 0},
{(const void*)&spite_singleton_making, "-\t-", "spite_singleton_making", 0},
{(const void*)&spite_singleton_made, "-\t-", "spite_singleton_made", 0},
{(const void*)&spite_singleton_tracked, "-\t-", "spite_singleton_tracked", 0},
{(const void*)&spite_singleton_created, "-\t-", "spite_singleton_created", 0},
{(const void*)&spite_singleton_used_after_exit, "-\t-", "spite_singleton_used_after_exit", 0},
{(const void*)&spite_singleton_free_later, "-\t-", "spite_singleton_free_later", 0},
{(const void*)&spite_singletons_destroy, "-\t-", "spite_singletons_destroy", 0},
{(const void*)&Launcher_Launcher, "launcher/launcher.spite\tLauncher", "Launcher", 3},
{(const void*)&SpiteBoolean_to_string, "library/boolean.spite\tBoolean", "to_string", 3},
{(const void*)&Clock_Clock, "library/windows/clock.spite\tClock", "Clock", 4},
{(const void*)&Clock_elapsed_nanoseconds, "library/windows/clock.spite\tClock", "elapsed_nanoseconds", 11},
{(const void*)&Console_print, "library/console.spite\tConsole", "print", 18},
{(const void*)&Console_error, "library/console.spite\tConsole", "error", 29},
{(const void*)&Console__write_values, "library/console.spite\tConsole", "_write_values", 50},
{(const void*)&Console__write_to, "library/console.spite\tConsole", "_write_to", 62},
{(const void*)&Console__write_output, "bootstrap/source/generation/prelude.spite\tConsole", "_write_output", 1},
{(const void*)&Console__write_error, "bootstrap/source/generation/prelude.spite\tConsole", "_write_error", 2},
{(const void*)&Console__flush, "bootstrap/source/generation/prelude.spite\tConsole", "_flush", 4},
{(const void*)&DynamicLibrary_DynamicLibrary, "library/dynamic_library.spite\tDynamicLibrary", "DynamicLibrary", 6},
{(const void*)&DynamicLibrary_drop, "library/dynamic_library.spite\tDynamicLibrary", "drop", 11},
{(const void*)&DynamicLibrary_open_library, "bootstrap/source/generation/prelude.spite\tDynamicLibrary", "open_library", 1},
{(const void*)&DynamicLibrary_find_symbol, "bootstrap/source/generation/prelude.spite\tDynamicLibrary", "find_symbol", 2},
{(const void*)&DynamicLibrary_close_library, "bootstrap/source/generation/prelude.spite\tDynamicLibrary", "close_library", 3},
{(const void*)&File_File, "library/file.spite\tFile", "File", 4},
{(const void*)&File_read, "library/file.spite\tFile", "read", 33},
{(const void*)&File_write, "library/file.spite\tFile", "write", 47},
{(const void*)&File_put, "library/file.spite\tFile", "put", 152},
{(const void*)&File_open_file, "library/windows/file.spite\tFile", "open_file", 4},
{(const void*)&File_close_file, "library/windows/file.spite\tFile", "close_file", 8},
{(const void*)&File_seek, "library/windows/file.spite\tFile", "seek", 12},
{(const void*)&File_tell, "library/windows/file.spite\tFile", "tell", 16},
{(const void*)&File_read_into___waiting, "library/windows/file.spite\tFile", "read_into", 20},
{(const void*)&File_write_text___waiting, "library/windows/file.spite\tFile", "write_text", 28},
{(const void*)&File_remove, "library/windows/file.spite\tFile", "remove", 43},
{(const void*)&SpiteInteger_to_string, "library/integer.spite\tInteger", "to_string", 3},
{(const void*)&Lock_Lock, "library/lock.spite\tLock", "Lock", 4},
{(const void*)&Lock_lock, "library/lock.spite\tLock", "lock", 8},
{(const void*)&Lock_unlock, "library/lock.spite\tLock", "unlock", 12},
{(const void*)&Lock_drop, "library/lock.spite\tLock", "drop", 22},
{(const void*)&Lock_create_lock, "library/windows/lock.spite\tLock", "create_lock", 3},
{(const void*)&Lock_acquire, "library/windows/lock.spite\tLock", "acquire", 9},
{(const void*)&Lock_release_lock, "library/windows/lock.spite\tLock", "release_lock", 13},
{(const void*)&Lock_destroy_lock, "library/windows/lock.spite\tLock", "destroy_lock", 17},
{(const void*)&SpiteLong_to_string, "library/long.spite\tLong", "to_string", 3},
{(const void*)&Scheduler_loop, "library/scheduler.spite\tScheduler", "loop", 16},
{(const void*)&Scheduler_start, "library/scheduler.spite\tScheduler", "start", 30},
{(const void*)&Scheduler_on_main_thread, "library/scheduler.spite\tScheduler", "on_main_thread", 47},
{(const void*)&Scheduler_waits_here, "library/scheduler.spite\tScheduler", "waits_here", 51},
{(const void*)&Scheduler_begin, "library/scheduler.spite\tScheduler", "begin", 60},
{(const void*)&Scheduler_run_frame, "library/scheduler.spite\tScheduler", "run_frame", 68},
{(const void*)&Scheduler_frame_done, "library/scheduler.spite\tScheduler", "frame_done", 86},
{(const void*)&Scheduler_forget_wait, "library/scheduler.spite\tScheduler", "forget_wait", 123},
{(const void*)&Scheduler_step_ready, "library/scheduler.spite\tScheduler", "step_ready", 150},
{(const void*)&Scheduler_wait_for, "library/scheduler.spite\tScheduler", "wait_for", 166},
{(const void*)&Scheduler_runs_below, "library/scheduler.spite\tScheduler", "runs_below", 177},
{(const void*)&Scheduler_remove_one, "library/scheduler.spite\tScheduler", "remove_one", 198},
{(const void*)&Scheduler_offload, "library/scheduler.spite\tScheduler", "offload", 246},
{(const void*)&Scheduler_offload_start, "library/scheduler.spite\tScheduler", "offload_start", 255},
{(const void*)&spite_failed_1, "-\t-", "spite_failed_1", 0},
{(const void*)&Scheduler_offload_over, "library/scheduler.spite\tScheduler", "offload_over", 263},
{(const void*)&Scheduler_offload_done, "library/scheduler.spite\tScheduler", "offload_done", 273},
{(const void*)&Scheduler_begin_wait, "library/scheduler.spite\tScheduler", "begin_wait", 278},
{(const void*)&Scheduler_signal, "library/scheduler.spite\tScheduler", "signal", 293},
{(const void*)&Scheduler_idle, "library/scheduler.spite\tScheduler", "idle", 307},
{(const void*)&Scheduler_idle_stepping, "library/scheduler.spite\tScheduler", "idle_stepping", 312},
{(const void*)&spite_failed_2, "-\t-", "spite_failed_2", 0},
{(const void*)&Scheduler_timeout, "library/scheduler.spite\tScheduler", "timeout", 334},
{(const void*)&Scheduler_current_thread, "library/windows/scheduler.spite\tScheduler", "current_thread", 3},
{(const void*)&Scheduler_create_event, "library/windows/scheduler.spite\tScheduler", "create_event", 7},
{(const void*)&Scheduler_signal_event, "library/windows/scheduler.spite\tScheduler", "signal_event", 13},
{(const void*)&Scheduler_wait_event, "library/windows/scheduler.spite\tScheduler", "wait_event", 17},
{(const void*)&Scheduler_clock, "library/windows/scheduler.spite\tScheduler", "clock", 21},
{(const void*)&Scheduler_start_thread, "library/windows/scheduler.spite\tScheduler", "start_thread", 25},
{(const void*)&Scheduler_end_thread, "library/windows/scheduler.spite\tScheduler", "end_thread", 30},
{(const void*)&Scheduler_step_frame, "bootstrap/source/generation/prelude.spite\tScheduler", "step_frame", 1},
{(const void*)&Scheduler_release_work, "bootstrap/source/generation/prelude.spite\tScheduler", "release_work", 2},
{(const void*)&SpiteString_length, "library/string.spite\tString", "length", 4},
{(const void*)&SpiteString_to_string, "library/string.spite\tString", "to_string", 184},
{(const void*)&ThreadSlot_ThreadSlot, "library/thread_slot.spite\tThreadSlot", "ThreadSlot", 3},
{(const void*)&ThreadSlot_read, "library/thread_slot.spite\tThreadSlot", "read", 7},
{(const void*)&ThreadSlot_write, "library/thread_slot.spite\tThreadSlot", "write", 11},
{(const void*)&ThreadSlot_drop, "library/thread_slot.spite\tThreadSlot", "drop", 15},
{(const void*)&ThreadSlot_create_key, "library/windows/thread_slot.spite\tThreadSlot", "create_key", 3},
{(const void*)&spite_failed_3, "-\t-", "spite_failed_3", 0},
{(const void*)&ThreadSlot_read_key, "library/windows/thread_slot.spite\tThreadSlot", "read_key", 9},
{(const void*)&ThreadSlot_write_key, "library/windows/thread_slot.spite\tThreadSlot", "write_key", 13},
{(const void*)&ThreadSlot_delete_key, "library/windows/thread_slot.spite\tThreadSlot", "delete_key", 17},
{(const void*)&SpiteMemory_Address_text, "library/memory/address.spite\tMemory.Address", "text", 3},
{(const void*)&SpiteMemory_Address_to_string, "library/memory/address.spite\tMemory.Address", "to_string", 13},
{(const void*)&Memory_Arena_allocate, "library/memory/arena.spite\tMemory.Arena", "allocate", 11},
{(const void*)&Memory_Arena_free, "library/memory/arena.spite\tMemory.Arena", "free", 21},
{(const void*)&Memory_Arena_start_block, "library/memory/arena.spite\tMemory.Arena", "start_block", 23},
{(const void*)&Memory_Heap_allocate, "bootstrap/source/generation/prelude.spite\tMemory.Heap", "allocate", 1},
{(const void*)&Memory_Heap_resize, "bootstrap/source/generation/prelude.spite\tMemory.Heap", "resize", 2},
{(const void*)&Memory_Heap_free, "bootstrap/source/generation/prelude.spite\tMemory.Heap", "free", 3},
{(const void*)&Spite_Class_Class, "library/spite/class.spite\tSpite.Class", "Class", 16},
{(const void*)&Spite_Function_Function, "library/spite/function.spite\tSpite.Function", "Function", 11},
{(const void*)&Naive_Naive, "benchmarks/reads_in_a_row_overlap/naive/naive.spite\tNaive", "Naive", 4},
{(const void*)&Naive_make_text, "benchmarks/reads_in_a_row_overlap/naive/naive.spite\tNaive", "make_text", 20},
{(const void*)&Naive_read_rounds___held_0_1, "benchmarks/reads_in_a_row_overlap/naive/naive.spite\tNaive", "read_rounds", 30},
{(const void*)&Naive_read_both___held_0_1, "benchmarks/reads_in_a_row_overlap/naive/naive.spite\tNaive", "read_both", 41},
{(const void*)&spite_failed_4, "-\t-", "spite_failed_4", 0},
{(const void*)&spite_failed_5, "-\t-", "spite_failed_5", 0},
{(const void*)&List_String_count, "library/list.spite\tList", "count", 9},
{(const void*)&List_String_append, "library/list.spite\tList", "append", 17},
{(const void*)&List_String_get_at, "library/list.spite\tList", "get_at", 41},
{(const void*)&List_String_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&List_String_join, "library/list.spite\tList", "join", 174},
{(const void*)&List_String_write_text, "library/list.spite\tList", "write_text", 611},
{(const void*)&List_String_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_String_make_room, "library/list.spite\tList", "make_room", 849},
{(const void*)&List_String__grow, "library/list.spite\tList", "_grow", 855},
{(const void*)&List_String__resized, "library/list.spite\tList", "_resized", 865},
{(const void*)&TypedMemory__String_read_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "read_value", 1},
{(const void*)&TypedMemory__String_write_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "write_value", 2},
{(const void*)&TypedMemory__String_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&TypedMemory__String_value_bytes, "bootstrap/source/generation/prelude.spite\tTypedMemory", "value_bytes", 4},
{(const void*)&List_Long_is_empty, "library/list.spite\tList", "is_empty", 13},
{(const void*)&List_Long_append, "library/list.spite\tList", "append", 17},
{(const void*)&List_Long_get_at, "library/list.spite\tList", "get_at", 41},
{(const void*)&List_Long_remove_at, "library/list.spite\tList", "remove_at", 54},
{(const void*)&spite_failed_6, "-\t-", "spite_failed_6", 0},
{(const void*)&spite_failed_7, "-\t-", "spite_failed_7", 0},
{(const void*)&List_Long_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&List_Long_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_Long_make_room, "library/list.spite\tList", "make_room", 849},
{(const void*)&List_Long__grow, "library/list.spite\tList", "_grow", 855},
{(const void*)&List_Long__resized, "library/list.spite\tList", "_resized", 865},
{(const void*)&List_Long_move_items, "library/list.spite\tList", "move_items", 877},
{(const void*)&TypedMemory__Long_read_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "read_value", 1},
{(const void*)&TypedMemory__Long_write_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "write_value", 2},
{(const void*)&TypedMemory__Long_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&TypedMemory__Long_value_bytes, "bootstrap/source/generation/prelude.spite\tTypedMemory", "value_bytes", 4},
{(const void*)&List_Memory_Address_append, "library/list.spite\tList", "append", 17},
{(const void*)&List_Memory_Address_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_Memory_Address_make_room, "library/list.spite\tList", "make_room", 849},
{(const void*)&List_Memory_Address__grow, "library/list.spite\tList", "_grow", 855},
{(const void*)&List_Memory_Address__resized, "library/list.spite\tList", "_resized", 865},
{(const void*)&ThreadLocal__SchedulerLoop_ThreadLocal, "library/thread_local.spite\tThreadLocal", "ThreadLocal", 12},
{(const void*)&ThreadLocal__SchedulerLoop_get, "library/thread_local.spite\tThreadLocal", "get", 16},
{(const void*)&ThreadLocal__SchedulerLoop_set, "library/thread_local.spite\tThreadLocal", "set", 23},
{(const void*)&ThreadLocal__SchedulerLoop__make_room, "library/thread_local.spite\tThreadLocal", "_make_room", 39},
{(const void*)&ThreadLocal__SchedulerLoop_drop, "library/thread_local.spite\tThreadLocal", "drop", 59},
{(const void*)&spite_failed_8, "-\t-", "spite_failed_8", 0},
{(const void*)&TypedMemory__SchedulerLoop_read_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "read_value", 1},
{(const void*)&TypedMemory__SchedulerLoop_write_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "write_value", 2},
{(const void*)&TypedMemory__SchedulerLoop_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&TypedMemory__SchedulerLoop_value_bytes, "bootstrap/source/generation/prelude.spite\tTypedMemory", "value_bytes", 4},
{(const void*)&List_SchedulerLoop_append, "library/list.spite\tList", "append", 17},
{(const void*)&List_SchedulerLoop_get_at, "library/list.spite\tList", "get_at", 41},
{(const void*)&List_SchedulerLoop_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_SchedulerLoop_make_room, "library/list.spite\tList", "make_room", 849},
{(const void*)&List_SchedulerLoop__grow, "library/list.spite\tList", "_grow", 855},
{(const void*)&List_SchedulerLoop__resized, "library/list.spite\tList", "_resized", 865},
{(const void*)&List_Spite_AttributeDeclaration_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_Spite_Function_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_Spite_Argument_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_Spite_Class_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_Spite_Namespace_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_Console_Printable_get_at, "library/list.spite\tList", "get_at", 41},
{(const void*)&List_Console_Printable_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&TypedMemory__Console_Printable_read_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "read_value", 1},
{(const void*)&List_Symbol_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&Concurrent__Nullable_String_Concurrent, "library/concurrent.spite\tConcurrent", "Concurrent", 9},
{(const void*)&Concurrent__Nullable_String__run, "library/concurrent.spite\tConcurrent", "_run", 47},
{(const void*)&Concurrent__Nullable_String__collect, "library/concurrent.spite\tConcurrent", "_collect", 53},
{(const void*)&Concurrent__Nullable_String__result, "library/concurrent.spite\tConcurrent", "_result", 60},
{(const void*)&spite_failed_9, "-\t-", "spite_failed_9", 0},
{(const void*)&Concurrent__Nullable_String__join, "library/concurrent.spite\tConcurrent", "_join", 66},
{(const void*)&spite_failed_10, "-\t-", "spite_failed_10", 0},
{(const void*)&Concurrent__Nullable_String_drop, "library/concurrent.spite\tConcurrent", "drop", 74},
{(const void*)&Concurrent__Nullable_String__start_frame, "bootstrap/source/generation/prelude.spite\tConcurrent", "_start_frame", 1},
{(const void*)&Concurrent__Nullable_String__frame_result, "bootstrap/source/generation/prelude.spite\tConcurrent", "_frame_result", 2},
{(const void*)&Concurrent__Nullable_String__free_frame, "bootstrap/source/generation/prelude.spite\tConcurrent", "_free_frame", 3},
{(const void*)&List_Nullable_String_append, "library/list.spite\tList", "append", 17},
{(const void*)&List_Nullable_String_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_Nullable_String_make_room, "library/list.spite\tList", "make_room", 849},
{(const void*)&List_Nullable_String__grow, "library/list.spite\tList", "_grow", 855},
{(const void*)&List_Nullable_String__resized, "library/list.spite\tList", "_resized", 865},
{(const void*)&List_Memory_Address_remove_last, "library/list.spite\tList", "remove_last", 73},
{(const void*)&List_SchedulerLoop_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&List_Spite_AttributeDeclaration_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&TypedMemory__Spite_AttributeDeclaration_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&List_Spite_Function_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&TypedMemory__Spite_Function_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&List_Spite_Argument_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&TypedMemory__Spite_Argument_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&List_Spite_Class_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&TypedMemory__Spite_Class_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&List_Spite_Namespace_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&TypedMemory__Spite_Namespace_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&List_Console_Printable_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&TypedMemory__Console_Printable_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&File_read___begin, "-\t-", "File_read___begin", 0},
{(const void*)&File_read___step, "-\t-", "File_read___step", 0},
{(const void*)&spite_frame_start, "-\t-", "spite_frame_start", 0},
{(const void*)&File_read_into, "library/windows/file.spite\tFile", "read_into", 20},
{(const void*)&File_write_text, "library/windows/file.spite\tFile", "write_text", 28},
{(const void*)&File_read_into___begin, "-\t-", "File_read_into___begin", 0},
{(const void*)&File_read_into___step, "-\t-", "File_read_into___step", 0},
{(const void*)&main, "-\t-", "main", 0},
{0, 0, 0, 0}
};
#define SPITE_FAULT_FOREIGN 1
#define SPITE_FAULT_TRACE 1
/* ---- native faults (D244): a fault the program cannot survive is reported, never silent (docs/failure.md) ---- */
typedef struct SpiteFaultLine { char bytes[1024]; int length; } SpiteFaultLine;
typedef struct SpiteFaultFrames { const SpiteFunctionPlace* last; uint64_t repeated; int shown; int more; int skips_runtime; } SpiteFaultFrames;
static volatile int32_t spite_fault_busy = 0;
static void spite_fault_add(SpiteFaultLine* line, const char* text) {
while (text != 0 && *text != 0 && line->length < (int)sizeof(line->bytes) - 1) { line->bytes[line->length] = *text; line->length = line->length + 1; text = text + 1; }
}
static void spite_fault_add_number(SpiteFaultLine* line, uint64_t value, uint64_t base) {
char digits[24];
int count = 0;
if (base == 16) spite_fault_add(line, "0x");
do { digits[count] = "0123456789abcdef"[value % base]; count = count + 1; value = value / base; } while (value != 0);
while (count > 0 && line->length < (int)sizeof(line->bytes) - 1) { count = count - 1; line->bytes[line->length] = digits[count]; line->length = line->length + 1; }
}
static void spite_fault_write(SpiteFaultLine* line) {
line->bytes[line->length] = '\n';
#ifdef _WIN32
DWORD written = 0;
WriteFile(GetStdHandle(STD_ERROR_HANDLE), line->bytes, (DWORD)(line->length + 1), &written, 0);
#else
ssize_t written = write(2, line->bytes, (size_t)(line->length + 1));
(void)written;
#endif
line->length = 0;
}
static const SpiteFunctionPlace* spite_fault_place_starting(uintptr_t start) {
for (const SpiteFunctionPlace* place = spite_function_places; place->start != 0; place = place + 1) if ((uintptr_t)place->start == start) return place;
return 0;
}
static const SpiteFunctionPlace* spite_fault_place_nearest(uintptr_t address) {
const SpiteFunctionPlace* found = 0;
for (const SpiteFunctionPlace* place = spite_function_places; place->start != 0; place = place + 1) {
if ((uintptr_t)place->start <= address && (found == 0 || (uintptr_t)place->start > (uintptr_t)found->start)) found = place;
}
return found;
}
static void spite_fault_add_place(SpiteFaultLine* line, const SpiteFunctionPlace* place) {
if (place == 0) { spite_fault_add(line, "-\t-\t-"); return; }
const char* owner = place->owner;
while (*owner != 0 && *owner != '\t' && line->length < (int)sizeof(line->bytes) - 1) { line->bytes[line->length] = *owner; line->length = line->length + 1; owner = owner + 1; }
if (place->line > 0) { spite_fault_add(line, ":"); spite_fault_add_number(line, (uint64_t)place->line, 10); }
spite_fault_add(line, owner);
spite_fault_add(line, "\t");
spite_fault_add(line, place->name);
}
static void spite_fault_frame_flush(SpiteFaultFrames* frames) {
if (frames->last == 0) return;
if (frames->shown >= 16) { frames->more = 1; return; }
SpiteFaultLine line;
line.length = 0;
spite_fault_add(&line, "spite.frame\t");
spite_fault_add_place(&line, frames->last);
if (frames->repeated > 1) { spite_fault_add(&line, "\trepeated="); spite_fault_add_number(&line, frames->repeated, 10); }
spite_fault_write(&line);
frames->shown = frames->shown + 1;
}
static void spite_fault_frame(SpiteFaultFrames* frames, const SpiteFunctionPlace* place) {
if (place == 0) return;
if (frames->skips_runtime != 0) { if (place->line == 0) return; frames->skips_runtime = 0; }
if (place == frames->last) { frames->repeated = frames->repeated + 1; return; }
spite_fault_frame_flush(frames);
frames->last = place;
frames->repeated = 1;
}
static void spite_fault_frames_end(SpiteFaultFrames* frames, int walked_all) {
spite_fault_frame_flush(frames);
if (frames->more == 0 && walked_all != 0) return;
SpiteFaultLine line;
line.length = 0;
spite_fault_add(&line, "spite.frame\tmore");
spite_fault_write(&line);
}
static void spite_fault_report_start(SpiteFaultLine* line, const char* kind, const SpiteFunctionPlace* place) {
spite_fault_add(line, "spite.fault\t");
spite_fault_add(line, kind);
spite_fault_add(line, "\t");
spite_fault_add_place(line, place);
}
static void spite_fault_report_end(SpiteFaultLine* line) {
#ifdef SPITE_FAULT_FOREIGN
if (spite_last_foreign_call != 0) { spite_fault_add(line, "\tforeign="); spite_fault_add(line, spite_last_foreign_call); }
#endif
spite_fault_write(line);
#ifdef SPITE_FAULT_TRACE
int64_t total = __atomic_load_n(&spite_assert_total, __ATOMIC_ACQUIRE);
int64_t kept = total < 32 ? total : 32;
for (int64_t index = total - kept; index < total; index = index + 1) {
if (spite_assert_trace[index % 32] == 0) continue;
const char* site = spite_assert_trace[index % 32];
uint64_t repeats = spite_assert_count(index);
while (index + 1 < total && spite_assert_trace[(index + 1) % 32] == site) { index = index + 1; repeats = repeats + spite_assert_count(index); }
spite_fault_add(line, site);
if (line->length > 0 && line->bytes[line->length - 1] == '\n') line->length = line->length - 1;
if (repeats > 1) { spite_fault_add(line, "\trepeated="); spite_fault_add_number(line, repeats, 10); }
spite_fault_write(line);
}
if (total > 32) { spite_fault_add(line, "spite.assert\tearlier="); spite_fault_add_number(line, (uint64_t)(total - 32), 10); spite_fault_write(line); }
#endif
}
#ifdef _WIN32
static DWORD spite_fault_owner = 0;
static DWORD spite_fault_code = 0;
static const char* spite_fault_kind(DWORD code, ULONG_PTR access) {
switch (code) {
case EXCEPTION_ACCESS_VIOLATION: return access == 8 ? "execute-violation" : access == 1 ? "write-violation" : "read-violation";
case EXCEPTION_IN_PAGE_ERROR: return "in-page-error";
case EXCEPTION_STACK_OVERFLOW: return "stack-overflow";
case EXCEPTION_ILLEGAL_INSTRUCTION: return "illegal-instruction";
case EXCEPTION_PRIV_INSTRUCTION: return "privileged-instruction";
case EXCEPTION_INT_DIVIDE_BY_ZERO: return "integer-division-by-zero";
case EXCEPTION_INT_OVERFLOW: return "integer-overflow";
case EXCEPTION_DATATYPE_MISALIGNMENT: return "misaligned-access";
case EXCEPTION_BREAKPOINT: return "breakpoint";
case 0xC0000374: return "heap-corruption";
case EXCEPTION_FLT_DIVIDE_BY_ZERO: case EXCEPTION_FLT_INVALID_OPERATION: case EXCEPTION_FLT_OVERFLOW: case EXCEPTION_FLT_UNDERFLOW:
case EXCEPTION_FLT_INEXACT_RESULT: case EXCEPTION_FLT_DENORMAL_OPERAND: case EXCEPTION_FLT_STACK_CHECK: return "floating-point-exception";
default: return "exception";
}
}
static HMODULE spite_fault_module(uintptr_t address) {
HMODULE module = 0;
if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)address, &module)) return 0;
return module;
}
static void spite_fault_add_module(SpiteFaultLine* line, uintptr_t address) {
HMODULE module = spite_fault_module(address);
char path[MAX_PATH];
if (module == 0 || GetModuleFileNameA(module, path, MAX_PATH) == 0) { spite_fault_add_number(line, address, 16); return; }
const char* name = path;
for (const char* scan = path; *scan != 0; scan = scan + 1) if (*scan == '\\' || *scan == '/') name = scan + 1;
spite_fault_add(line, name);
spite_fault_add(line, "+");
spite_fault_add_number(line, address - (uintptr_t)module, 16);
}
#if defined(_WIN64)
#if defined(_M_ARM64) || defined(__aarch64__)
#define SPITE_FAULT_PC(frame) ((frame).Pc)
#else
#define SPITE_FAULT_PC(frame) ((frame).Rip)
#endif
static const SpiteFunctionPlace* spite_fault_place_of(uintptr_t address) {
DWORD64 image = 0;
PRUNTIME_FUNCTION entry = RtlLookupFunctionEntry((DWORD64)address, &image, 0);
if (entry != 0) return spite_fault_place_starting((uintptr_t)(image + entry->BeginAddress));
if (spite_fault_module(address) != GetModuleHandleA(0)) return 0;
return spite_fault_place_nearest(address);
}
static int spite_fault_walk(const CONTEXT* start, SpiteFaultFrames* frames) {
CONTEXT frame = *start;
for (int depth = 0; depth < 256; depth = depth + 1) {
DWORD64 pc = SPITE_FAULT_PC(frame);
if (pc == 0) return 1;
DWORD64 image = 0;
PRUNTIME_FUNCTION entry = RtlLookupFunctionEntry(pc, &image, 0);
spite_fault_frame(frames, depth == 0 ? spite_fault_place_of((uintptr_t)pc) : entry != 0 ? spite_fault_place_starting((uintptr_t)(image + entry->BeginAddress)) : 0);
if (entry == 0) {
#if defined(_M_ARM64) || defined(__aarch64__)
if (depth != 0 || frame.Lr == pc) return 1;
frame.Pc = frame.Lr;
#else
frame.Rip = *(DWORD64*)frame.Rsp;
frame.Rsp = frame.Rsp + 8;
#endif
} else {
void* handler_data = 0;
DWORD64 establisher = 0;
RtlVirtualUnwind(UNW_FLAG_NHANDLER, image, pc, entry, &frame, &handler_data, &establisher, 0);
}
}
return 0;
}
#else
static const SpiteFunctionPlace* spite_fault_place_of(uintptr_t address) {
if (spite_fault_module(address) != GetModuleHandleA(0)) return 0;
return spite_fault_place_nearest(address);
}
#endif
static LONG WINAPI spite_fault_filter(EXCEPTION_POINTERS* information) {
EXCEPTION_RECORD* record = information->ExceptionRecord;
if (__atomic_exchange_n(&spite_fault_busy, 1, __ATOMIC_ACQ_REL) != 0) {
if (spite_fault_owner == GetCurrentThreadId()) TerminateProcess(GetCurrentProcess(), spite_fault_code);
for (;;) Sleep(1000);
}
spite_fault_owner = GetCurrentThreadId();
spite_fault_code = record->ExceptionCode;
uintptr_t at = (uintptr_t)record->ExceptionAddress;
SpiteFaultLine line;
line.length = 0;
spite_fault_report_start(&line, spite_fault_kind(record->ExceptionCode, record->NumberParameters > 0 ? record->ExceptionInformation[0] : 0), spite_fault_place_of(at));
if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters > 1) { spite_fault_add(&line, "\taddress="); spite_fault_add_number(&line, (uint64_t)record->ExceptionInformation[1], 16); }
spite_fault_add(&line, "\tcode=");
spite_fault_add_number(&line, (uint64_t)record->ExceptionCode, 16);
spite_fault_add(&line, "\tat=");
spite_fault_add_module(&line, at);
spite_fault_report_end(&line);
SpiteFaultFrames frames = { 0, 0, 0, 0 };
#if defined(_WIN64)
int walked_all = spite_fault_walk(information->ContextRecord, &frames);
#else
int walked_all = 1;
spite_fault_frame(&frames, spite_fault_place_of(at));
#endif
spite_fault_frames_end(&frames, walked_all);
fflush(stdout);
TerminateProcess(GetCurrentProcess(), record->ExceptionCode);
return EXCEPTION_CONTINUE_SEARCH;
}
static LONG CALLBACK spite_fault_vectored(EXCEPTION_POINTERS* information) {
if (information->ExceptionRecord->ExceptionCode != 0xC0000374) return EXCEPTION_CONTINUE_SEARCH;
return spite_fault_filter(information);
}
static void spite_crash_frames(void) {
SpiteFaultFrames frames = { 0, 0, 0, 0, 1 };
int walked_all = 1;
#if defined(_WIN64)
CONTEXT context;
RtlCaptureContext(&context);
walked_all = spite_fault_walk(&context, &frames);
#endif
spite_fault_frames_end(&frames, walked_all);
}
static void spite_fault_thread(void) {
ULONG reserve = 16384;
SetThreadStackGuarantee(&reserve);
}
static void spite_fault_install(void) {
spite_crash_chain = spite_crash_frames;
SetUnhandledExceptionFilter(spite_fault_filter);
AddVectoredExceptionHandler(0, spite_fault_vectored);
spite_fault_thread();
}
#else
#if defined(__GLIBC__) || defined(__APPLE__)
#include <execinfo.h>
#endif
static uintptr_t spite_fault_own_base = 0;
static SPITE_THREAD_LOCAL char spite_fault_stack[65536];
static uintptr_t spite_fault_base_of(uintptr_t address, const char** file) {
Dl_info found;
if (dladdr((void*)address, &found) == 0) return 0;
if (file != 0) *file = found.dli_fname;
return (uintptr_t)found.dli_fbase;
}
static const SpiteFunctionPlace* spite_fault_place_of(uintptr_t address) {
if (spite_fault_own_base == 0 || spite_fault_base_of(address, 0) != spite_fault_own_base) return 0;
return spite_fault_place_nearest(address);
}
static void spite_fault_add_module(SpiteFaultLine* line, uintptr_t address) {
const char* file = 0;
uintptr_t base = spite_fault_base_of(address, &file);
if (base == 0 || file == 0) { spite_fault_add_number(line, address, 16); return; }
const char* name = file;
for (const char* scan = file; *scan != 0; scan = scan + 1) if (*scan == '/') name = scan + 1;
spite_fault_add(line, name);
spite_fault_add(line, "+");
spite_fault_add_number(line, address - base, 16);
}
static void spite_fault_registers(void* context, uintptr_t* pc, uintptr_t* frame, uintptr_t* stack) {
ucontext_t* registers = (ucontext_t*)context;
#if defined(__APPLE__) && defined(__aarch64__)
*pc = (uintptr_t)__darwin_arm_thread_state64_get_pc(registers->uc_mcontext->__ss);
*frame = (uintptr_t)__darwin_arm_thread_state64_get_fp(registers->uc_mcontext->__ss);
*stack = (uintptr_t)__darwin_arm_thread_state64_get_sp(registers->uc_mcontext->__ss);
#elif defined(__APPLE__) && defined(__x86_64__)
*pc = (uintptr_t)registers->uc_mcontext->__ss.__rip;
*frame = (uintptr_t)registers->uc_mcontext->__ss.__rbp;
*stack = (uintptr_t)registers->uc_mcontext->__ss.__rsp;
#elif defined(__linux__) && defined(__x86_64__)
*pc = (uintptr_t)registers->uc_mcontext.gregs[16];
*frame = (uintptr_t)registers->uc_mcontext.gregs[10];
*stack = (uintptr_t)registers->uc_mcontext.gregs[15];
#elif defined(__linux__) && defined(__aarch64__)
*pc = (uintptr_t)registers->uc_mcontext.pc;
*frame = (uintptr_t)registers->uc_mcontext.regs[29];
*stack = (uintptr_t)registers->uc_mcontext.sp;
#else
(void)registers;
*pc = 0;
*frame = 0;
*stack = 0;
#endif
}
static int spite_fault_in_allocator(void) {
#if defined(__GLIBC__) || defined(__APPLE__)
void* returns[64];
int count = backtrace(returns, 64);
for (int index = 0; index < count; index = index + 1) {
Dl_info found;
if (dladdr(returns[index], &found) == 0 || found.dli_sname == 0) continue;
const char* name = found.dli_sname;
if (strstr(name, "malloc") != 0 || strstr(name, "free") != 0 || strstr(name, "realloc") != 0 || strstr(name, "calloc") != 0) return 1;
}
#endif
return 0;
}
static const char* spite_fault_kind(int number, int code, uintptr_t address, uintptr_t stack) {
if (number == SIGABRT) return spite_fault_in_allocator() ? "heap-corruption" : "abort";
uintptr_t distance = address > stack ? address - stack : stack - address;
if (number == SIGSEGV && stack != 0 && distance < 65536) return "stack-overflow";
if (number == SIGSEGV) return "access-violation";
if (number == SIGBUS) return "bus-error";
if (number == SIGILL) return "illegal-instruction";
if (number == SIGTRAP) return "breakpoint";
if (number == SIGFPE && code == FPE_INTDIV) return "integer-division-by-zero";
if (number == SIGFPE && code == FPE_INTOVF) return "integer-overflow";
return "floating-point-exception";
}
static void spite_fault_signal(int number, siginfo_t* information, void* context) {
if (__atomic_exchange_n(&spite_fault_busy, 1, __ATOMIC_ACQ_REL) != 0) { for (;;) pause(); }
uintptr_t pc = 0;
uintptr_t frame = 0;
uintptr_t stack = 0;
spite_fault_registers(context, &pc, &frame, &stack);
uintptr_t address = (uintptr_t)information->si_addr;
SpiteFaultLine line;
line.length = 0;
spite_fault_report_start(&line, spite_fault_kind(number, information->si_code, address, stack), spite_fault_place_of(pc));
if (number == SIGSEGV || number == SIGBUS) { spite_fault_add(&line, "\taddress="); spite_fault_add_number(&line, (uint64_t)address, 16); }
spite_fault_add(&line, "\tsignal=");
spite_fault_add_number(&line, (uint64_t)number, 10);
spite_fault_add(&line, "\tat=");
spite_fault_add_module(&line, pc);
spite_fault_report_end(&line);
SpiteFaultFrames frames = { 0, 0, 0, 0 };
spite_fault_frame(&frames, spite_fault_place_of(pc));
int walked_all = 1;
#if !defined(__OPTIMIZE__) || defined(SPITE_FRAME_POINTERS)
walked_all = 0;
for (int depth = 1; depth < 256; depth = depth + 1) {
if (frame == 0 || (frame & (sizeof(uintptr_t) - 1)) != 0) { walked_all = 1; break; }
uintptr_t next = ((uintptr_t*)frame)[0];
uintptr_t returned = ((uintptr_t*)frame)[1];
if (returned == 0) { walked_all = 1; break; }
spite_fault_frame(&frames, spite_fault_place_of(returned - 1));
if (next <= frame || next - frame > 16777216) { walked_all = 1; break; }
frame = next;
}
#else
(void)frame;
#endif
spite_fault_frames_end(&frames, walked_all);
signal(number, SIG_DFL);
raise(number);
}
static void spite_crash_frames(void) {
SpiteFaultFrames frames = { 0, 0, 0, 0, 1 };
int walked_all = 1;
#if !defined(__OPTIMIZE__) || defined(SPITE_FRAME_POINTERS)
walked_all = 0;
uintptr_t frame = (uintptr_t)__builtin_frame_address(0);
for (int depth = 0; depth < 256; depth = depth + 1) {
if (frame == 0 || (frame & (sizeof(uintptr_t) - 1)) != 0) { walked_all = 1; break; }
uintptr_t next = ((uintptr_t*)frame)[0];
uintptr_t returned = ((uintptr_t*)frame)[1];
if (returned == 0) { walked_all = 1; break; }
spite_fault_frame(&frames, spite_fault_place_of(returned - 1));
if (next <= frame || next - frame > 16777216) { walked_all = 1; break; }
frame = next;
}
#endif
spite_fault_frames_end(&frames, walked_all);
}
static void spite_fault_thread(void) {
stack_t alternate;
alternate.ss_sp = spite_fault_stack;
alternate.ss_size = sizeof(spite_fault_stack);
alternate.ss_flags = 0;
sigaltstack(&alternate, 0);
}
static void spite_fault_install(void) {
spite_crash_chain = spite_crash_frames;
spite_fault_thread();
#if defined(__GLIBC__) || defined(__APPLE__)
void* warmed[1];
backtrace(warmed, 1);
#endif
spite_fault_own_base = spite_fault_base_of((uintptr_t)(void*)&spite_fault_install, 0);
struct sigaction action;
memset(&action, 0, sizeof(action));
action.sa_sigaction = spite_fault_signal;
action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
sigemptyset(&action.sa_mask);
int numbers[6] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGTRAP, SIGABRT };
for (int index = 0; index < 6; index = index + 1) sigaddset(&action.sa_mask, numbers[index]);
for (int index = 0; index < 6; index = index + 1) sigaction(numbers[index], &action, 0);
}
#endif
int main(int argument_count, char** argument_values) {
spite_fault_install();
spite_program_arguments = (SpiteArguments){ .count = argument_count - 1, .items = (const char**)(argument_values + 1) };
#ifdef _WIN32
_setmode(_fileno(stdout), _O_BINARY);
#endif
Launcher* spite_launcher = Launcher___allocate();
Launcher_Launcher(spite_launcher);
Launcher___release(spite_launcher);
spite_singletons_destroy();



if (spite_foreign_library_2_tracked) DynamicLibrary___destroy(spite_foreign_library_2_cache);
if (spite_foreign_library_1_tracked) DynamicLibrary___destroy(spite_foreign_library_1_cache);


if (spite_class_object_Nullable_String_cache != 0 && spite_class_object_Nullable_String_cache->_namespace_ != 0) { Spite_Namespace___release(spite_class_object_Nullable_String_cache->_namespace_); spite_class_object_Nullable_String_cache->_namespace_ = 0; }
Spite_Class___release(spite_class_object_Nullable_String_cache);
return 0;
}
