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
static int64_t spite_tasks_in_flight = 0;
static SPITE_THREAD_LOCAL void* spite_skipped[16];
static SPITE_THREAD_LOCAL int32_t spite_skipped_taken[16];
static SPITE_THREAD_LOCAL int32_t spite_skipped_depth = 0;
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
typedef struct Duration Duration;
typedef struct DynamicLibrary DynamicLibrary;
typedef struct List List;
typedef struct Nothing Nothing;
typedef struct ThreadPool ThreadPool;
typedef struct ThreadPoolJob ThreadPoolJob;
typedef struct TimeText TimeText;
typedef struct Memory_Arena Memory_Arena;
typedef struct Memory_Heap Memory_Heap;
typedef struct Spite_Argument Spite_Argument;
typedef struct Spite_AttributeDeclaration Spite_AttributeDeclaration;
typedef struct Spite_Class Spite_Class;
typedef struct Spite_Function Spite_Function;
typedef struct Spite_Namespace Spite_Namespace;
typedef struct Naive Naive;
typedef struct Orbit Orbit;
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
static DynamicLibrary* spite_foreign_library_1_cache = 0;
static bool spite_foreign_library_1_tracked = false;
static DynamicLibrary* spite_foreign_library_2_cache = 0;
static bool spite_foreign_library_2_tracked = false;
static SpiteString spite_lit_1 = SPITE_STATIC_STRING("", 0);
static Clock* spite_singleton_Clock_cache = 0;
static bool spite_singleton_Clock_destroyed = false;
static int32_t spite_singleton_Clock_lock = 0;
typedef struct List_ThreadPoolJob List_ThreadPoolJob;
typedef struct TypedMemory__ThreadPoolJob TypedMemory__ThreadPoolJob;
static SpiteString spite_symbol_1 = { (int64_t)0x0064656d616e6e75ULL, (int64_t)0x0800000000000000ULL };
static SpiteString spite_symbol_2 = { (int64_t)0x00676e6968746f4eULL, (int64_t)0x0800000000000000ULL };
static SpiteString spite_lit_2 = SPITE_STATIC_STRING("", 0);
typedef struct List_Spite_AttributeDeclaration List_Spite_AttributeDeclaration;
typedef struct TypedMemory__Spite_AttributeDeclaration TypedMemory__Spite_AttributeDeclaration;
typedef struct List_Spite_Function List_Spite_Function;
typedef struct TypedMemory__Spite_Function TypedMemory__Spite_Function;
typedef struct List_Spite_Argument List_Spite_Argument;
typedef struct TypedMemory__Spite_Argument TypedMemory__Spite_Argument;
static SpiteString spite_lit_3 = SPITE_STATIC_STRING("", 0);
static SpiteString spite_lit_4 = SPITE_STATIC_STRING("", 0);
typedef struct List_Spite_Class List_Spite_Class;
typedef struct TypedMemory__Spite_Class TypedMemory__Spite_Class;
typedef struct List_Spite_Namespace List_Spite_Namespace;
typedef struct TypedMemory__Spite_Namespace TypedMemory__Spite_Namespace;
typedef struct List_Orbit List_Orbit;
typedef struct TypedMemory__Orbit TypedMemory__Orbit;
typedef void* Spite_Allocator;
struct Launcher {
SpiteHeader header;
Build* build_;
};
typedef struct List_Console_Printable List_Console_Printable;
typedef struct TypedMemory__Console_Printable TypedMemory__Console_Printable;
typedef struct SpiteBox_SpiteLong { SpiteHeader header; int64_t value; } SpiteBox_SpiteLong;
#define SPITE_FRAMED_COUNT 1073741824
static SpiteString spite_lit_5 = SPITE_STATIC_STRING("true", 4);
static SpiteString spite_lit_6 = SPITE_STATIC_STRING("false", 5);
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
static SpiteString spite_lit_7 = SPITE_STATIC_STRING("\n", 1);
static SpiteString spite_lit_8 = SPITE_STATIC_STRING("\n", 1);
static SpiteString spite_lit_9 = SPITE_STATIC_STRING(" ", 1);
#define spite_site_2() "library/console.spite:56 in Console._write_values"

struct Duration {
SpiteHeader header;
TimeText* _time_text_;
int64_t _seconds_;
int32_t _nanoseconds_;
};
#define spite_site_3() "library/duration.spite:17 in Duration.Duration"
#define spite_site_4() "library/duration.spite:19 in Duration.Duration"
#define spite_site_5() "library/duration.spite:26 in Duration.total"
struct DynamicLibrary {
SpiteHeader header;
SpiteString file_name_;
int64_t handle_;
};
static void* spite_foreign_1_22 = 0;
#define SpiteFloat_to_long(self) ((int64_t)(self))
#define SpiteFloat_to_double(self) ((double)(self))
static Spite_Class* spite_class_object_Nothing_cache = 0;
static bool spite_class_object_Nothing_ready = false;
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
static void* spite_foreign_1_34 = 0;
static void* spite_foreign_1_35 = 0;
static void* spite_foreign_1_36 = 0;
static SpiteString spite_lit_10 = SPITE_STATIC_STRING("0", 1);
#define spite_site_6() "library/long.spite:17 in Long.to_string"
#define spite_site_7() "library/long.spite:18 in Long.to_string"
#define spite_site_8() "library/long.spite:22 in Long.to_string"
#define spite_site_9() "library/long.spite:26 in Long.to_string"
#define SpiteLong_to_unsigned_integer(self) ((uint32_t)(self))
#define SpiteLong_to_unsigned_long(self) ((uint64_t)(self))
struct Nothing {
SpiteHeader header;
};
static void* spite_foreign_1_45 = 0;
static void* spite_foreign_1_46 = 0;
static void* spite_foreign_1_47 = 0;
#define SpiteShort_to_integer(self) ((int32_t)(self))
#define SpiteShort_to_long(self) ((int64_t)(self))
#define SpiteString_code_at(self, index) spite_string_code_at(&(self), (index))
struct ThreadPool {
SpiteHeader header;
Memory_Heap* heap_;
int64_t workers_;
int32_t worker_count_;
int64_t worker_threads_;
int32_t registered_;
List_ThreadPoolJob* jobs_;
int64_t queue_lock_;
int64_t work_ready_;
int64_t work_done_;
bool stopping_;
DynamicLibrary* kernel_;
};
#define spite_site_10() "library/thread_pool.spite:55 in ThreadPool.join"
#define spite_site_11() "library/thread_pool.spite:98 in ThreadPool.piece_start"
#define spite_site_12() "library/thread_pool.spite:100 in ThreadPool.piece_start"
#define spite_site_13() "library/thread_pool.spite:108 in ThreadPool.start"
#define spite_site_14() "library/thread_pool.spite:112 in ThreadPool.start"
#define spite_site_15() "library/thread_pool.spite:113 in ThreadPool.start"
#define spite_site_16() "spite.crash\t51fed911"
#define spite_site_17() "library/thread_pool.spite:120 in ThreadPool.start"
#define spite_site_18() "library/thread_pool.spite:130 in ThreadPool._serve"
#define spite_site_19() "library/thread_pool.spite:131 in ThreadPool._serve"
#define spite_site_20() "spite.crash\t4b3be9e7"
#define spite_site_21() "spite.crash\t22ff4541"
#define spite_site_22() "library/thread_pool.spite:166 in ThreadPool.drop"
static void* spite_foreign_1_67 = 0;
static void* spite_foreign_1_68 = 0;
static void* spite_foreign_1_69 = 0;
static void* spite_foreign_1_70 = 0;
static void* spite_foreign_1_71 = 0;
struct ThreadPoolJob {
SpiteHeader header;
Spite_Function* work_;
int32_t first_;
int32_t end_;
int64_t state_;
};
struct TimeText {
SpiteHeader header;
};
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
List_Orbit* orbits_;
};
static SpiteString spite_symbol_3 = { (int64_t)0x5f65636e61766461ULL, (int64_t)0x020000736b636974ULL };
typedef struct Benchmark__Nothing Benchmark__Nothing;
static SpiteBox_SpiteString spite_lit_11_box = { { 0, -1 }, SPITE_STATIC_STRING("turns", 5) };
typedef struct SpiteBox_SpiteInteger { SpiteHeader header; int32_t value; } SpiteBox_SpiteInteger;
static SpiteString spite_lit_12 = SPITE_STATIC_STRING("microseconds ", 13);
struct Orbit {
SpiteHeader header;
float angle_;
float speed_;
int32_t turns_;
};
#define spite_site_28() "benchmarks/a_loop_whose_passes_write_only_their_own_item_runs_in_bands/naive/orbit.spite:15 in Orbit.advance"
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
struct List_ThreadPoolJob {
SpiteHeader header;
Memory_Heap* heap_;
TypedMemory__ThreadPoolJob* values_;
int64_t items_;
int32_t item_count_;
int32_t capacity_;
};
struct TypedMemory__ThreadPoolJob {
SpiteHeader header;
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
struct List_Orbit {
SpiteHeader header;
Memory_Heap* heap_;
TypedMemory__Orbit* values_;
int64_t items_;
int32_t item_count_;
int32_t capacity_;
};
struct TypedMemory__Orbit {
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
struct Benchmark__Nothing {
SpiteHeader header;
Clock* _clock_;
Nothing* answer_;
Duration* duration_;
};
#define SpiteByte_to_unsigned_integer(self) ((uint32_t)(self))
#define SpiteDouble_to_float(self) ((float)(self))
#define SpiteInteger_to_short(self) ((int16_t)(self))
#define SpiteLong_to_double(self) ((double)(self))
#define spite_site_29() "library/list.spite:20 in List.append"
#define spite_site_30() "spite.crash\t4d4e4520"
#define spite_site_31() "spite.crash\t2a307341"
#define spite_site_32() "library/list.spite:57 in List.remove_at"
#define spite_site_33() "library/list.spite:58 in List.remove_at"
#define spite_site_34() "library/list.spite:856 in List._grow"
#define spite_site_35() "library/list.spite:861 in List._grow"
#define spite_site_36() "library/list.spite:879 in List.move_items"
#define spite_site_37() "library/list.spite:880 in List.move_items"
#define spite_site_38() "library/list.spite:707 in List.sum_turns"
static ThreadPool* spite_singleton_ThreadPool_cache = 0;
static bool spite_singleton_ThreadPool_destroyed = false;
static int32_t spite_singleton_ThreadPool_lock = 0;
static SpiteString spite_symbol_4 = { (int64_t)0x0000007473726966ULL, (int64_t)0x0a00000000000000ULL };
static Spite_Class* spite_class_object_Integer_cache = 0;
static bool spite_class_object_Integer_ready = false;
static SpiteString spite_symbol_5 = { (int64_t)0x0000000000646e65ULL, (int64_t)0x0c00000000000000ULL };
static SpiteString spite_symbol_6 = SPITE_STATIC_STRING("spite_band_advance_piece", 24);
#define spite_site_39() "library/benchmark.spite:11 in Benchmark.Benchmark"
#define spite_site_40() "library/thread_pool.spite:204 in ThreadPool.run_bands"
#define spite_site_41() "library/thread_pool.spite:212 in ThreadPool.run_bands"
#define spite_site_42() "library/thread_pool.spite:213 in ThreadPool.run_bands"
#define spite_site_43() "library/thread_pool.spite:214 in ThreadPool.run_bands"
#define spite_site_44() "library/thread_pool.spite:219 in ThreadPool.run_bands"
#define spite_site_45() "library/thread_pool.spite:226 in ThreadPool.run_bands"
static SpiteString spite_symbol_7 = { (int64_t)0x0072656765746e49ULL, (int64_t)0x0800000000000000ULL };
static SpiteString spite_symbol_8 = { (int64_t)0x797469746e656469ULL, (int64_t)0x0700000000000000ULL };
typedef struct SpiteGuard { _Alignas(64) int64_t owner; int64_t depth; } SpiteGuard;
#define SPITE_GUARDS_HELD() 0
#define SPITE_GUARDS_COUNT(change) ((void)0)
Memory_Heap* spite_singleton_Memory_Heap(void);
Console_Printable Console_Printable___retain(Console_Printable self);
void Console_Printable___release(Console_Printable self);
static void* ThreadPool___thread_entry(void* pool);
Build* spite_singleton_Build(void);
Console* spite_singleton_Console(void);
DynamicLibrary* spite_foreign_library_1(void);
DynamicLibrary* spite_foreign_library_2(void);
TimeText* spite_singleton_TimeText(void);
Clock* spite_singleton_Clock(void);
TypedMemory__ThreadPoolJob* spite_singleton_TypedMemory__ThreadPoolJob(void);
TypedMemory__Spite_AttributeDeclaration* spite_singleton_TypedMemory__Spite_AttributeDeclaration(void);
TypedMemory__Spite_Function* spite_singleton_TypedMemory__Spite_Function(void);
TypedMemory__Spite_Argument* spite_singleton_TypedMemory__Spite_Argument(void);
TypedMemory__Orbit* spite_singleton_TypedMemory__Orbit(void);
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
void Duration___init(Duration* self);
Duration* Duration___allocate(void);
Duration* Duration___default(void);
Duration* Duration___make(int64_t amount_, Duration_Unit unit_);
static inline void Duration___release(Duration* self);
void Duration___free(Duration* self);
void Duration_Duration(Duration* self, int64_t amount_, Duration_Unit unit_);
int64_t Duration_total(Duration* self, Duration_Unit unit_);
int64_t Duration__units_per_second(Duration* self, Duration_Unit unit_);
int32_t Duration__nanoseconds_per_unit(Duration* self, Duration_Unit unit_);
int64_t Duration__seconds_per_unit(Duration* self, Duration_Unit unit_);
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
Spite_Class* spite_class_object_Nothing(void);
SpiteString SpiteInteger_to_string(int32_t self);
SpiteString SpiteLong_to_string(int64_t self);
void Nothing___init(Nothing* self);
Nothing* Nothing___allocate(void);
Nothing* Nothing___default(void);
static inline void Nothing___release(Nothing* self);
void Nothing___free(Nothing* self);
SpiteString SpiteString_to_string(SpiteString self);
SpiteString SpiteString___retain(SpiteString self);
void SpiteString___release(SpiteString self);
SpiteString spite_string_from_bytes(const char* bytes, int64_t length);
SpiteString spite_string_join(int32_t count, const SpiteString* pieces);
static int64_t spite_long_digits(char* digits, int64_t value);
static SpiteStringBlock* spite_string_block(int64_t length);
static SpiteString spite_string_held(SpiteStringBlock* block, int64_t length);
void ThreadPool___init(ThreadPool* self);
ThreadPool* ThreadPool___allocate(void);
ThreadPool* ThreadPool___make(void);
void ThreadPool___release(ThreadPool* self);
void ThreadPool_drop(ThreadPool* self);
int32_t ThreadPool_size(ThreadPool* self);
void ThreadPool_submit(ThreadPool* self, Spite_Function* job_, int32_t first_, int32_t end_, int64_t state_);
bool ThreadPool_is_done(ThreadPool* self, int64_t state_);
void ThreadPool_join(ThreadPool* self, int64_t state_);
int32_t ThreadPool_piece_start(ThreadPool* self, int32_t index_, int32_t base_, int32_t extra_);
void ThreadPool_start(ThreadPool* self);
void ThreadPool__serve(ThreadPool* self);
void ThreadPool_run_queued(ThreadPool* self, int32_t position_);
void ThreadPool_drop(ThreadPool* self);
void ThreadPool_run_bands(ThreadPool* self, Spite_Function* piece_, int32_t count_);
int32_t ThreadPool_processor_count(ThreadPool* self);
int64_t ThreadPool_current_thread(ThreadPool* self);
int64_t ThreadPool_start_thread(ThreadPool* self, int64_t entry_, int64_t argument_);
void ThreadPool_join_thread(ThreadPool* self, int64_t thread_);
int64_t ThreadPool_create_lock(ThreadPool* self);
void ThreadPool_destroy_lock(ThreadPool* self, int64_t created_);
void ThreadPool_lock_queue(ThreadPool* self);
void ThreadPool_unlock_queue(ThreadPool* self);
int64_t ThreadPool_create_condition(ThreadPool* self);
void ThreadPool_destroy_condition(ThreadPool* self, int64_t created_);
void ThreadPool_wait_for_signal(ThreadPool* self, int64_t condition_);
void ThreadPool_signal_one(ThreadPool* self, int64_t condition_);
void ThreadPool_signal_all(ThreadPool* self, int64_t condition_);
int64_t ThreadPool_entry_address(ThreadPool* self);
int64_t ThreadPool_address(ThreadPool* self);
void ThreadPool__task_begun(ThreadPool* self);
void ThreadPool__task_ended(ThreadPool* self);
void ThreadPool__world_enter(ThreadPool* self);
void ThreadPool__world_leave(ThreadPool* self);
void ThreadPool___destroy(ThreadPool* self);
void ThreadPool___discard(ThreadPool* self);
static SPITE_CRASH_REPORT void spite_failed_1(int64_t thread_, int32_t wanted_, int64_t entry_, int64_t pool_, int32_t index_, ThreadPool* self);
static SPITE_CRASH_REPORT void spite_failed_2(int32_t position_, ThreadPool* self);
static SPITE_CRASH_REPORT void spite_failed_3(int32_t position_, ThreadPool* self);
void ThreadPoolJob___init(ThreadPoolJob* self);
ThreadPoolJob* ThreadPoolJob___allocate(void);
ThreadPoolJob* ThreadPoolJob___make(void);
static inline ThreadPoolJob* ThreadPoolJob___retain(ThreadPoolJob* self);
static inline void ThreadPoolJob___release(ThreadPoolJob* self);
void ThreadPoolJob___free(ThreadPoolJob* self);
void TimeText___release(TimeText* self);
SpiteString SpiteMemory_Address_text(int64_t self, int64_t length_);
SpiteString SpiteMemory_Address_to_string(int64_t self);
int64_t Memory_Arena_allocate(Memory_Arena* self, int64_t bytes_);
void Memory_Arena_free(Memory_Arena* self, int64_t _address_);
void Memory_Arena_start_block(Memory_Arena* self, int64_t at_least_);
void Memory_Heap___release(Memory_Heap* self);
int64_t Memory_Heap_allocate(Memory_Heap* self, int64_t bytes_);
int64_t Memory_Heap_resize(Memory_Heap* self, int64_t address_, int64_t bytes_);
void Memory_Heap_free(Memory_Heap* self, int64_t address_);
Spite_Argument* Spite_Argument___make(SpiteString starting_name_, Spite_Class* starting_class_);
static inline Spite_Argument* Spite_Argument___retain(Spite_Argument* self);
static inline void Spite_Argument___release(Spite_Argument* self);
void Spite_Argument___free(Spite_Argument* self);
void Spite_Argument_Argument(Spite_Argument* self, SpiteString starting_name_, Spite_Class* starting_class_);
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
static inline Naive* Naive___retain(Naive* self);
static inline void Naive___release(Naive* self);
void Naive___free(Naive* self);
void Naive_Naive(Naive* self);
void Naive_make_orbits(Naive* self, int32_t count_);
void Naive_advance_ticks(Naive* self);
Spite_Function* spite_function_value_Naive_advance_ticks(Naive* owner);
int32_t List_Orbit_sum_turns(List_Orbit* self);
static inline SpiteTagged spite_tagged_SpiteInteger(int32_t value);
void List_Orbit_spite_band_advance_piece(List_Orbit* self, int32_t first_, int32_t end_);
void List_Orbit_spite_band_advance(List_Orbit* self);
void List_Orbit_each_advance(List_Orbit* self);
void Orbit___init(Orbit* self);
Orbit* Orbit___allocate(void);
Orbit* Orbit___make(int32_t seed_);
static inline Orbit* Orbit___retain(Orbit* self);
static inline void Orbit___release(Orbit* self);
void Orbit___free(Orbit* self);
void Orbit_Orbit(Orbit* self, int32_t seed_);
void Orbit_advance(Orbit* self);
static inline void List_String___release(List_String* self);
void List_String___free(List_String* self);
void List_String_drop(List_String* self);
void List_String_clear(List_String* self);
void List_String_drop(List_String* self);
void TypedMemory__String___release(TypedMemory__String* self);
void TypedMemory__String_release_value(TypedMemory__String* self, int64_t address_, int32_t index_);
void List_ThreadPoolJob___init(List_ThreadPoolJob* self);
List_ThreadPoolJob* List_ThreadPoolJob___allocate(void);
List_ThreadPoolJob* List_ThreadPoolJob___make(void);
static inline void List_ThreadPoolJob___release(List_ThreadPoolJob* self);
void List_ThreadPoolJob___free(List_ThreadPoolJob* self);
void List_ThreadPoolJob_drop(List_ThreadPoolJob* self);
int32_t List_ThreadPoolJob_count(List_ThreadPoolJob* self);
bool List_ThreadPoolJob_is_empty(List_ThreadPoolJob* self);
void List_ThreadPoolJob_append(List_ThreadPoolJob* self, ThreadPoolJob* value_);
ThreadPoolJob* List_ThreadPoolJob_get_at(List_ThreadPoolJob* self, int32_t index_);
void List_ThreadPoolJob_remove_at(List_ThreadPoolJob* self, int32_t index_);
void List_ThreadPoolJob_clear(List_ThreadPoolJob* self);
void List_ThreadPoolJob_drop(List_ThreadPoolJob* self);
void List_ThreadPoolJob_make_room(List_ThreadPoolJob* self);
void List_ThreadPoolJob__grow(List_ThreadPoolJob* self);
int64_t List_ThreadPoolJob__resized(List_ThreadPoolJob* self, int64_t old_bytes_, int64_t new_bytes_);
void List_ThreadPoolJob_move_items(List_ThreadPoolJob* self, int32_t from_, int32_t to_, int32_t moved_count_);
void TypedMemory__ThreadPoolJob___release(TypedMemory__ThreadPoolJob* self);
ThreadPoolJob* TypedMemory__ThreadPoolJob_read_value(TypedMemory__ThreadPoolJob* self, int64_t address_, int32_t index_);
void TypedMemory__ThreadPoolJob_write_value(TypedMemory__ThreadPoolJob* self, int64_t address_, int32_t index_, ThreadPoolJob* value_);
void TypedMemory__ThreadPoolJob_release_value(TypedMemory__ThreadPoolJob* self, int64_t address_, int32_t index_);
int64_t TypedMemory__ThreadPoolJob_value_bytes(TypedMemory__ThreadPoolJob* self);
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
void List_Spite_Argument_append(List_Spite_Argument* self, Spite_Argument* value_);
void List_Spite_Argument_clear(List_Spite_Argument* self);
void List_Spite_Argument_drop(List_Spite_Argument* self);
void List_Spite_Argument_make_room(List_Spite_Argument* self);
void List_Spite_Argument__grow(List_Spite_Argument* self);
int64_t List_Spite_Argument__resized(List_Spite_Argument* self, int64_t old_bytes_, int64_t new_bytes_);
void TypedMemory__Spite_Argument___release(TypedMemory__Spite_Argument* self);
void TypedMemory__Spite_Argument_write_value(TypedMemory__Spite_Argument* self, int64_t address_, int32_t index_, Spite_Argument* value_);
void TypedMemory__Spite_Argument_release_value(TypedMemory__Spite_Argument* self, int64_t address_, int32_t index_);
int64_t TypedMemory__Spite_Argument_value_bytes(TypedMemory__Spite_Argument* self);
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
void List_Orbit___init(List_Orbit* self);
List_Orbit* List_Orbit___allocate(void);
List_Orbit* List_Orbit___make(void);
static inline void List_Orbit___release(List_Orbit* self);
void List_Orbit___free(List_Orbit* self);
void List_Orbit_drop(List_Orbit* self);
void List_Orbit_append(List_Orbit* self, Orbit* value_);
void List_Orbit_clear(List_Orbit* self);
void List_Orbit_drop(List_Orbit* self);
void List_Orbit_make_room(List_Orbit* self);
void List_Orbit__grow(List_Orbit* self);
int64_t List_Orbit__resized(List_Orbit* self, int64_t old_bytes_, int64_t new_bytes_);
int32_t List_Orbit_sum_turns(List_Orbit* self);
void List_Orbit_spite_band_advance_piece(List_Orbit* self, int32_t first_, int32_t end_);
void List_Orbit_spite_band_advance(List_Orbit* self);
void List_Orbit_each_advance(List_Orbit* self);
void TypedMemory__Orbit___release(TypedMemory__Orbit* self);
void TypedMemory__Orbit_write_value(TypedMemory__Orbit* self, int64_t address_, int32_t index_, Orbit* value_);
void TypedMemory__Orbit_release_value(TypedMemory__Orbit* self, int64_t address_, int32_t index_);
int64_t TypedMemory__Orbit_value_bytes(TypedMemory__Orbit* self);
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
void Benchmark__Nothing___init(Benchmark__Nothing* self);
Benchmark__Nothing* Benchmark__Nothing___allocate(void);
Benchmark__Nothing* Benchmark__Nothing___make(Spite_Function* work_);
static inline void Benchmark__Nothing___release(Benchmark__Nothing* self);
void Benchmark__Nothing___free(Benchmark__Nothing* self);
void Benchmark__Nothing_Benchmark(Benchmark__Nothing* self, Spite_Function* work_);
static SPITE_CRASH_REPORT void spite_failed_4(int32_t index_, List_ThreadPoolJob* self);
static SPITE_CRASH_REPORT void spite_failed_5(int32_t index_, List_ThreadPoolJob* self);
ThreadPool* spite_singleton_ThreadPool(void);
Spite_Class* spite_class_object_Integer(void);
static void spite_function_value_List_Orbit_spite_band_advance_piece___arguments(Spite_Function* described);
Spite_Function* spite_function_value_List_Orbit_spite_band_advance_piece(List_Orbit* owner);
bool spite_singleton_tracked(void);
void spite_singleton_created(void (*teardown)(void));
void spite_singleton_used_after_exit(const char* name);
void spite_singletons_destroy(void);
void spite_singleton_free_later(void* object);
void spite_singleton_check_circle(const char* name);
void spite_singleton_making(const char* name);
void spite_singleton_made(void);
static void spite_spin_pause(void);
static void spite_guard_wait(SpiteGuard* guard, int64_t spite_me);
static void spite_guard_enter(SpiteGuard* guard);
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
#define SPITE_ALLOCATOR_List_Orbit(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Console_Printable(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Console_Debuggable(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Directory_Entry(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_File(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Symbol(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Spite_Access(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Directory(object, heap) ((void)(object), ((Spite_Allocator)heap()))
static __typeof__(&Memory_Heap___release) spite_folded_Memory_Heap___release = ((__typeof__(&Memory_Heap___release))&TimeText___release);
static __typeof__(&TypedMemory__String___release) spite_folded_TypedMemory__String___release = ((__typeof__(&TypedMemory__String___release))&TimeText___release);
static __typeof__(&TypedMemory__ThreadPoolJob___release) spite_folded_TypedMemory__ThreadPoolJob___release = ((__typeof__(&TypedMemory__ThreadPoolJob___release))&TimeText___release);
static __typeof__(&TypedMemory__Spite_AttributeDeclaration___release) spite_folded_TypedMemory__Spite_AttributeDeclaration___release = ((__typeof__(&TypedMemory__Spite_AttributeDeclaration___release))&TimeText___release);
static __typeof__(&TypedMemory__Spite_Function___release) spite_folded_TypedMemory__Spite_Function___release = ((__typeof__(&TypedMemory__Spite_Function___release))&TimeText___release);
static __typeof__(&TypedMemory__Spite_Argument___release) spite_folded_TypedMemory__Spite_Argument___release = ((__typeof__(&TypedMemory__Spite_Argument___release))&TimeText___release);
static __typeof__(&TypedMemory__Spite_Class___release) spite_folded_TypedMemory__Spite_Class___release = ((__typeof__(&TypedMemory__Spite_Class___release))&TimeText___release);
static __typeof__(&TypedMemory__Spite_Namespace___release) spite_folded_TypedMemory__Spite_Namespace___release = ((__typeof__(&TypedMemory__Spite_Namespace___release))&TimeText___release);
static __typeof__(&TypedMemory__Orbit___release) spite_folded_TypedMemory__Orbit___release = ((__typeof__(&TypedMemory__Orbit___release))&TimeText___release);
static __typeof__(&TypedMemory__Console_Printable___release) spite_folded_TypedMemory__Console_Printable___release = ((__typeof__(&TypedMemory__Console_Printable___release))&TimeText___release);
static __typeof__(&TypedMemory__Symbol___release) spite_folded_TypedMemory__Symbol___release = ((__typeof__(&TypedMemory__Symbol___release))&TimeText___release);
static __typeof__(&List_Console_Printable_count) spite_folded_List_Console_Printable_count = ((__typeof__(&List_Console_Printable_count))&List_ThreadPoolJob_count);
static __typeof__(&List_Symbol_clear) spite_folded_List_Symbol_clear = ((__typeof__(&List_Symbol_clear))&List_String_clear);
static __typeof__(&TypedMemory__Symbol_release_value) spite_folded_TypedMemory__Symbol_release_value = ((__typeof__(&TypedMemory__Symbol_release_value))&TypedMemory__String_release_value);
Memory_Heap* spite_singleton_Memory_Heap(void) {
static Memory_Heap spite_object = { { 1, 96 } };
return &spite_object;
}


Console_Printable Console_Printable___retain(Console_Printable self) {
if (self.plain == 0 && self.value.object != 0) SPITE_COUNT_UP(((SpiteHeader*)self.value.object)->ref_count);
return self;
}
static void* ThreadPool___thread_entry(void* pool) {
spite_fault_thread();
ThreadPool__serve((ThreadPool*)pool);
return 0;
}
Build* spite_singleton_Build(void) {
static Build spite_object = { { 1, 13 } };
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
TimeText* spite_singleton_TimeText(void) {
static TimeText spite_object = { { 1, 77 } };
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
TypedMemory__ThreadPoolJob* spite_singleton_TypedMemory__ThreadPoolJob(void) {
static TypedMemory__ThreadPoolJob spite_object = { { 1, 140 } };
return &spite_object;
}
TypedMemory__Spite_AttributeDeclaration* spite_singleton_TypedMemory__Spite_AttributeDeclaration(void) {
static TypedMemory__Spite_AttributeDeclaration spite_object = { { 1, 142 } };
return &spite_object;
}
TypedMemory__Spite_Function* spite_singleton_TypedMemory__Spite_Function(void) {
static TypedMemory__Spite_Function spite_object = { { 1, 144 } };
return &spite_object;
}
TypedMemory__Spite_Argument* spite_singleton_TypedMemory__Spite_Argument(void) {
static TypedMemory__Spite_Argument spite_object = { { 1, 146 } };
return &spite_object;
}
TypedMemory__Orbit* spite_singleton_TypedMemory__Orbit(void) {
static TypedMemory__Orbit spite_object = { { 1, 152 } };
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
static TypedMemory__Console_Printable spite_object = { { 1, 154 } };
return &spite_object;
}
static List_Console_Printable* List_Console_Printable___framed(List_Console_Printable* self, int64_t items, int32_t count) {
List_Console_Printable___init(self);
self->header.ref_count = 2;
self->header.class_id = 153;
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
self->header.class_id = 14;
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
spite_folded_Memory_Heap___release(self->heap_);
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
self->header.class_id = 18;
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
spite_folded_Memory_Heap___release(self->heap_);
DynamicLibrary___release(self->library_);
#ifdef SPITE_TRACKS_Console
spite_untrack_Console(self);
#endif
#ifdef SPITE_WEAK_Console
spite_weak_object_freed(self);
#endif
spite_singleton_free_later(self);
}
void Duration___init(Duration* self) {
self->_time_text_ = spite_singleton_TimeText();
self->_seconds_ = SpiteInteger_to_long(0);
self->_nanoseconds_ = 0;
}
Duration* Duration___allocate(void) {
Duration* self = (Duration*)SPITE_MALLOC(sizeof(Duration));
self->header.ref_count = 1;
self->header.class_id = 27;
Duration___init(self);
#ifdef SPITE_TRACKS_Duration
spite_track_Duration(self);
#endif
return self;
}
Duration* Duration___default(void) { return Duration___allocate(); }
Duration* Duration___make(int64_t amount_, Duration_Unit unit_) {
Duration* self = Duration___allocate();
Duration_Duration(self, amount_, unit_);
return self;
}
static inline void Duration___release(Duration* self) {
if (self == 0) return;
if (SPITE_PLAIN_COUNT_DOWN(self->header.ref_count) > 0) return;
Duration___free(self);
}
void Duration___free(Duration* self) {
TimeText___release(self->_time_text_);
#ifdef SPITE_TRACKS_Duration
spite_untrack_Duration(self);
#endif
#ifdef SPITE_WEAK_Duration
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void DynamicLibrary___init(DynamicLibrary* self) {
self->file_name_ = spite_lit_1;
self->handle_ = SpiteInteger_to_long(0);
}
DynamicLibrary* DynamicLibrary___allocate(void) {
DynamicLibrary* self = (DynamicLibrary*)SPITE_MALLOC(sizeof(DynamicLibrary));
self->header.ref_count = 1;
self->header.class_id = 28;
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
void Nothing___init(Nothing* self) {
}
Nothing* Nothing___allocate(void) {
Nothing* self = (Nothing*)SPITE_MALLOC(sizeof(Nothing));
self->header.ref_count = 1;
self->header.class_id = 54;
Nothing___init(self);
#ifdef SPITE_TRACKS_Nothing
spite_track_Nothing(self);
#endif
return self;
}
Nothing* Nothing___default(void) { return Nothing___allocate(); }
static inline void Nothing___release(Nothing* self) {
if (self == 0) return;
if (SPITE_PLAIN_COUNT_DOWN(self->header.ref_count) > 0) return;
Nothing___free(self);
}
void Nothing___free(Nothing* self) {
#ifdef SPITE_TRACKS_Nothing
spite_untrack_Nothing(self);
#endif
#ifdef SPITE_WEAK_Nothing
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
void ThreadPool___init(ThreadPool* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->workers_ = ((int64_t)(0));
self->worker_count_ = 0;
self->worker_threads_ = ((int64_t)(0));
self->registered_ = 0;
self->jobs_ = List_ThreadPoolJob___make();
self->queue_lock_ = SpiteInteger_to_long(0);
self->work_ready_ = SpiteInteger_to_long(0);
self->work_done_ = SpiteInteger_to_long(0);
self->stopping_ = false;
self->kernel_ = spite_foreign_library_1();
}
ThreadPool* ThreadPool___allocate(void) {
ThreadPool* self = (ThreadPool*)SPITE_MALLOC(sizeof(ThreadPool));
self->header.ref_count = 1;
self->header.class_id = 73;
ThreadPool___init(self);
#ifdef SPITE_TRACKS_ThreadPool
spite_track_ThreadPool(self);
#endif
return self;
}
ThreadPool* ThreadPool___make(void) {
ThreadPool* self = ThreadPool___allocate();
return self;
}
void ThreadPool___release(ThreadPool* self) { (void)self; }
void ThreadPool___destroy(ThreadPool* self) {
if (self == 0) return;
ThreadPool_drop(self);
ThreadPool___discard(self);
}
void ThreadPool___discard(ThreadPool* self) {
if (self == 0) return;
spite_folded_Memory_Heap___release(self->heap_);
List_ThreadPoolJob___release(self->jobs_);
DynamicLibrary___release(self->kernel_);
#ifdef SPITE_TRACKS_ThreadPool
spite_untrack_ThreadPool(self);
#endif
#ifdef SPITE_WEAK_ThreadPool
spite_weak_object_freed(self);
#endif
spite_singleton_free_later(self);
}
void ThreadPoolJob___init(ThreadPoolJob* self) {
self->work_ = 0;
self->first_ = 0;
self->end_ = 0;
self->state_ = ((int64_t)(0));
}
ThreadPoolJob* ThreadPoolJob___allocate(void) {
ThreadPoolJob* self = (ThreadPoolJob*)SPITE_MALLOC(sizeof(ThreadPoolJob));
self->header.ref_count = 1;
self->header.class_id = 74;
ThreadPoolJob___init(self);
#ifdef SPITE_TRACKS_ThreadPoolJob
spite_track_ThreadPoolJob(self);
#endif
return self;
}
ThreadPoolJob* ThreadPoolJob___make(void) {
ThreadPoolJob* self = ThreadPoolJob___allocate();
return self;
}
static inline ThreadPoolJob* ThreadPoolJob___retain(ThreadPoolJob* self) {
if (self != 0) SPITE_COUNT_UP(self->header.ref_count);
return self;
}
static inline void ThreadPoolJob___release(ThreadPoolJob* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
ThreadPoolJob___free(self);
}
void ThreadPoolJob___free(ThreadPoolJob* self) {
Spite_Function___release(self->work_);
#ifdef SPITE_TRACKS_ThreadPoolJob
spite_untrack_ThreadPoolJob(self);
#endif
#ifdef SPITE_WEAK_ThreadPoolJob
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void TimeText___release(TimeText* self) { (void)self; }
static void Spite_Argument___init_constructed(Spite_Argument* self) {
self->_name_ = spite_symbol_1;
self->_class_ = 0;
self->_index_ = 0;
self->_mutated_ = false;
}
static Spite_Argument* Spite_Argument___allocate_constructed(void) {
Spite_Argument* self = (Spite_Argument*)SPITE_MALLOC(sizeof(Spite_Argument));
self->header.ref_count = 1;
self->header.class_id = 99;
Spite_Argument___init_constructed(self);
#ifdef SPITE_TRACKS_Spite_Argument
spite_track_Spite_Argument(self);
#endif
return self;
}
Spite_Argument* Spite_Argument___make(SpiteString starting_name_, Spite_Class* starting_class_) {
Spite_Argument* self = Spite_Argument___allocate_constructed();
Spite_Argument_Argument(self, starting_name_, starting_class_);
return self;
}
static inline Spite_Argument* Spite_Argument___retain(Spite_Argument* self) {
if (self != 0) SPITE_COUNT_UP(self->header.ref_count);
return self;
}
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
self->_source_folder_ = spite_lit_2;
self->_source_paths_ = 0;
self->_attributes_ = List_Spite_AttributeDeclaration___make();
self->_functions_ = List_Spite_Function___make();
self->_unbound_functions_ = 0;
}
Spite_Class* Spite_Class___allocate(void) {
Spite_Class* self = (Spite_Class*)SPITE_MALLOC(sizeof(Spite_Class));
self->header.ref_count = 1;
self->header.class_id = 102;
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
self->_returned_literal_ = spite_lit_3;
self->_has_returned_literal_ = false;
self->_accessed_ = spite_lit_4;
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
self->header.class_id = 106;
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
self->orbits_ = List_Orbit___make();
}
Naive* Naive___allocate(void) {
Naive* self = (Naive*)SPITE_MALLOC(sizeof(Naive));
self->header.ref_count = 1;
self->header.class_id = 111;
Naive___init(self);
#ifdef SPITE_TRACKS_Naive
spite_track_Naive(self);
#endif
return self;
}
static inline Naive* Naive___retain(Naive* self) {
if (self != 0) SPITE_COUNT_UP(self->header.ref_count);
return self;
}
static inline void Naive___release(Naive* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Naive___free(self);
}
void Naive___free(Naive* self) {
Console___release(self->console_);
List_Orbit___release(self->orbits_);
#ifdef SPITE_TRACKS_Naive
spite_untrack_Naive(self);
#endif
#ifdef SPITE_WEAK_Naive
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
Spite_Function* spite_function_value_Naive_advance_ticks(Naive* owner) {
Spite_Function* described = Spite_Function___make(spite_symbol_3, spite_class_object_Nothing());
described->spite_owner = Naive___retain(owner);
described->spite_release_owner = (void (*)(void*))Naive___release;
described->spite_call = (void (*)(void*))Naive_advance_ticks;
described->spite_typed_call = (void*)Naive_advance_ticks;
return described;
}
static inline SpiteTagged spite_tagged_SpiteInteger(int32_t value) {
SpiteTagged tagged;
tagged.tag = 171;
tagged.plain = 1;
tagged.value.bits = 0;
memcpy(&tagged.value, &value, sizeof(value));
return tagged;
}
void Orbit___init(Orbit* self) {
self->angle_ = 0.0;
self->speed_ = 0.0;
self->turns_ = 0;
}
Orbit* Orbit___allocate(void) {
Orbit* self = (Orbit*)SPITE_MALLOC(sizeof(Orbit));
self->header.ref_count = 1;
self->header.class_id = 112;
Orbit___init(self);
#ifdef SPITE_TRACKS_Orbit
spite_track_Orbit(self);
#endif
return self;
}
Orbit* Orbit___make(int32_t seed_) {
Orbit* self = Orbit___allocate();
Orbit_Orbit(self, seed_);
return self;
}
static inline Orbit* Orbit___retain(Orbit* self) {
if (self != 0) SPITE_COUNT_UP(self->header.ref_count);
return self;
}
static inline void Orbit___release(Orbit* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Orbit___free(self);
}
void Orbit___free(Orbit* self) {
#ifdef SPITE_TRACKS_Orbit
spite_untrack_Orbit(self);
#endif
#ifdef SPITE_WEAK_Orbit
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
static inline void List_String___release(List_String* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
List_String___free(self);
}
void List_String___free(List_String* self) {
List_String_drop(self);
spite_folded_Memory_Heap___release(self->heap_);
spite_folded_TypedMemory__String___release(self->values_);
#ifdef SPITE_TRACKS_List_String
spite_untrack_List_String(self);
#endif
#ifdef SPITE_WEAK_List_String
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void List_ThreadPoolJob___init(List_ThreadPoolJob* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->values_ = spite_singleton_TypedMemory__ThreadPoolJob();
self->items_ = ((int64_t)(0));
self->item_count_ = 0;
self->capacity_ = 0;
}
List_ThreadPoolJob* List_ThreadPoolJob___allocate(void) {
List_ThreadPoolJob* self = (List_ThreadPoolJob*)SPITE_MALLOC(sizeof(List_ThreadPoolJob));
self->header.ref_count = 1;
self->header.class_id = 139;
List_ThreadPoolJob___init(self);
#ifdef SPITE_TRACKS_List_ThreadPoolJob
spite_track_List_ThreadPoolJob(self);
#endif
return self;
}
List_ThreadPoolJob* List_ThreadPoolJob___make(void) {
List_ThreadPoolJob* self = List_ThreadPoolJob___allocate();
return self;
}
static inline void List_ThreadPoolJob___release(List_ThreadPoolJob* self) {
if (self == 0) return;
if (SPITE_PLAIN_COUNT_DOWN(self->header.ref_count) > 0) return;
List_ThreadPoolJob___free(self);
}
void List_ThreadPoolJob___free(List_ThreadPoolJob* self) {
List_ThreadPoolJob_drop(self);
spite_folded_Memory_Heap___release(self->heap_);
spite_folded_TypedMemory__ThreadPoolJob___release(self->values_);
#ifdef SPITE_TRACKS_List_ThreadPoolJob
spite_untrack_List_ThreadPoolJob(self);
#endif
#ifdef SPITE_WEAK_List_ThreadPoolJob
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
self->header.class_id = 141;
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
spite_folded_Memory_Heap___release(self->heap_);
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
self->header.class_id = 143;
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
spite_folded_Memory_Heap___release(self->heap_);
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
self->header.class_id = 145;
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
spite_folded_Memory_Heap___release(self->heap_);
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
spite_folded_Memory_Heap___release(self->heap_);
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
spite_folded_Memory_Heap___release(self->heap_);
spite_folded_TypedMemory__Spite_Namespace___release(self->values_);
#ifdef SPITE_TRACKS_List_Spite_Namespace
spite_untrack_List_Spite_Namespace(self);
#endif
#ifdef SPITE_WEAK_List_Spite_Namespace
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void List_Orbit___init(List_Orbit* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->values_ = spite_singleton_TypedMemory__Orbit();
self->items_ = ((int64_t)(0));
self->item_count_ = 0;
self->capacity_ = 0;
}
List_Orbit* List_Orbit___allocate(void) {
List_Orbit* self = (List_Orbit*)SPITE_MALLOC(sizeof(List_Orbit));
self->header.ref_count = 1;
self->header.class_id = 151;
List_Orbit___init(self);
#ifdef SPITE_TRACKS_List_Orbit
spite_track_List_Orbit(self);
#endif
return self;
}
List_Orbit* List_Orbit___make(void) {
List_Orbit* self = List_Orbit___allocate();
return self;
}
static inline void List_Orbit___release(List_Orbit* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
List_Orbit___free(self);
}
void List_Orbit___free(List_Orbit* self) {
List_Orbit_drop(self);
spite_folded_Memory_Heap___release(self->heap_);
spite_folded_TypedMemory__Orbit___release(self->values_);
#ifdef SPITE_TRACKS_List_Orbit
spite_untrack_List_Orbit(self);
#endif
#ifdef SPITE_WEAK_List_Orbit
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
spite_folded_Memory_Heap___release(self->heap_);
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
spite_folded_Memory_Heap___release(self->heap_);
spite_folded_TypedMemory__Symbol___release(self->values_);
#ifdef SPITE_TRACKS_List_Symbol
spite_untrack_List_Symbol(self);
#endif
#ifdef SPITE_WEAK_List_Symbol
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void Benchmark__Nothing___init(Benchmark__Nothing* self) {
self->_clock_ = spite_singleton_Clock();
self->answer_ = Nothing___default();
self->duration_ = Duration___default();
}
Benchmark__Nothing* Benchmark__Nothing___allocate(void) {
Benchmark__Nothing* self = (Benchmark__Nothing*)SPITE_MALLOC(sizeof(Benchmark__Nothing));
self->header.ref_count = 1;
self->header.class_id = 170;
Benchmark__Nothing___init(self);
#ifdef SPITE_TRACKS_Benchmark__Nothing
spite_track_Benchmark__Nothing(self);
#endif
return self;
}
Benchmark__Nothing* Benchmark__Nothing___make(Spite_Function* work_) {
Benchmark__Nothing* self = Benchmark__Nothing___allocate();
Benchmark__Nothing_Benchmark(self, work_);
return self;
}
static inline void Benchmark__Nothing___release(Benchmark__Nothing* self) {
if (self == 0) return;
if (SPITE_PLAIN_COUNT_DOWN(self->header.ref_count) > 0) return;
Benchmark__Nothing___free(self);
}
void Benchmark__Nothing___free(Benchmark__Nothing* self) {
Clock___release(self->_clock_);
Nothing___release(self->answer_);
Duration___release(self->duration_);
#ifdef SPITE_TRACKS_Benchmark__Nothing
spite_untrack_Benchmark__Nothing(self);
#endif
#ifdef SPITE_WEAK_Benchmark__Nothing
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
static void spite_singleton_ThreadPool_teardown(void) {
ThreadPool* object = spite_singleton_ThreadPool_cache;
spite_singleton_ThreadPool_cache = 0;
spite_singleton_ThreadPool_destroyed = true;
ThreadPool___destroy(object);
}
ThreadPool* spite_singleton_ThreadPool(void) {
ThreadPool* found = SPITE_SINGLETON_FOUND(spite_singleton_ThreadPool_cache);
if (found != 0) return found;
spite_singleton_check_circle("ThreadPool");
SPITE_LOCK(spite_singleton_ThreadPool_lock);
if (spite_singleton_ThreadPool_cache == 0) {
if (spite_singleton_ThreadPool_destroyed) spite_singleton_used_after_exit("ThreadPool");
spite_singleton_making("ThreadPool");
ThreadPool* made = ThreadPool___make();
spite_singleton_made();
spite_singleton_created(spite_singleton_ThreadPool_teardown);
SPITE_SINGLETON_PUBLISH(spite_singleton_ThreadPool_cache, made);
}
SPITE_UNLOCK(spite_singleton_ThreadPool_lock);
return spite_singleton_ThreadPool_cache;
}
static void spite_function_value_List_Orbit_spite_band_advance_piece___arguments(Spite_Function* described) {
List_Spite_Argument_append(described->_arguments_, Spite_Argument___make(spite_symbol_4, spite_class_object_Integer()));
List_Spite_Argument_append(described->_arguments_, Spite_Argument___make(spite_symbol_5, spite_class_object_Integer()));
}
Spite_Function* spite_function_value_List_Orbit_spite_band_advance_piece(List_Orbit* owner) {
Spite_Function* described = Spite_Function___make(spite_symbol_6, spite_class_object_Nothing());
described->spite_add_arguments = spite_function_value_List_Orbit_spite_band_advance_piece___arguments;
described->spite_owner = (void*)owner;
described->spite_release_owner = 0;
described->spite_typed_call = (void*)List_Orbit_spite_band_advance_piece;
return described;
}
Spite_Class* spite_class_object_Nothing(void) {
if (SPITE_SINGLETON_FOUND(spite_class_object_Nothing_ready)) return Spite_Class___retain(spite_class_object_Nothing_cache);
spite_described_enter();
if (spite_class_object_Nothing_cache == 0) {
spite_class_object_Nothing_cache = Spite_Class___make(spite_symbol_2);
spite_class_object_Nothing_cache->_fits_vector_ = true;
}
spite_described_leave(&spite_class_object_Nothing_ready);
return Spite_Class___retain(spite_class_object_Nothing_cache);
}
Spite_Class* spite_class_object_Integer(void) {
if (SPITE_SINGLETON_FOUND(spite_class_object_Integer_ready)) return Spite_Class___retain(spite_class_object_Integer_cache);
spite_described_enter();
if (spite_class_object_Integer_cache == 0) {
spite_class_object_Integer_cache = Spite_Class___make(spite_symbol_7);
}
spite_described_leave(&spite_class_object_Integer_ready);
return Spite_Class___retain(spite_class_object_Integer_cache);
}
void Console_Printable___release(Console_Printable self) {
if (self.plain != 0 || self.value.object == 0) return;
if (((self).tag == 0) && ((self).plain == 0)) { spite_string_box_release(self.value.object); return; }
}
SpiteString Console_Printable___call_to_string(Console_Printable self) {
if (((self).tag == 0) && ((self).plain == 0)) return SpiteString_to_string((((SpiteBox_SpiteString*)(self).value.object)->value));
if ((self).tag == 155) return SpiteLong_to_string(SPITE_TAGGED_VALUE(self, int64_t));
if ((self).tag == 171) return SpiteInteger_to_string(SPITE_TAGGED_VALUE(self, int32_t));
fputs("spite.crash\tPrintable.to_string was called on a value of a class it was not compiled for\n", stderr);
abort();
}
static int32_t spite_foreign_library_1_lock = 0;
DynamicLibrary* spite_foreign_library_1(void) {
DynamicLibrary* found = SPITE_SINGLETON_FOUND(spite_foreign_library_1_cache);
if (found != 0) return found;
SPITE_LOCK(spite_foreign_library_1_lock);
if (spite_foreign_library_1_cache == 0) {
DynamicLibrary* made = DynamicLibrary___make(((SpiteString)SPITE_STATIC_STRING("kernel32.dll", 12)), spite_symbol_8, ((SpiteString)SPITE_STATIC_STRING("", 0)));
spite_foreign_library_1_tracked = spite_singleton_tracked();
(void)&DynamicLibrary_find_symbol;
spite_foreign_1_0 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("QueryPerformanceFrequency", 25)), ((SpiteString)SPITE_STATIC_STRING("Clock.Clock", 11)));
spite_foreign_1_1 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("QueryPerformanceCounter", 23)), ((SpiteString)SPITE_STATIC_STRING("Clock.elapsed_nanoseconds", 25)));











spite_foreign_1_22 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("CloseHandle", 11)), ((SpiteString)SPITE_STATIC_STRING("File.map", 8)));











spite_foreign_1_34 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("InitializeSRWLock", 17)), ((SpiteString)SPITE_STATIC_STRING("Lock.create_lock", 16)));
spite_foreign_1_35 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("AcquireSRWLockExclusive", 23)), ((SpiteString)SPITE_STATIC_STRING("Lock.acquire", 12)));
spite_foreign_1_36 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("ReleaseSRWLockExclusive", 23)), ((SpiteString)SPITE_STATIC_STRING("Lock.release_lock", 17)));



spite_foreign_1_45 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("CreateThread", 12)), ((SpiteString)SPITE_STATIC_STRING("ReadEvaluatePrintLoop.start_thread", 34)));
spite_foreign_1_46 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("WaitForSingleObject", 19)), ((SpiteString)SPITE_STATIC_STRING("ReadEvaluatePrintLoop.join_thread", 33)));
spite_foreign_1_47 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("GetCurrentThreadId", 18)), ((SpiteString)SPITE_STATIC_STRING("Scheduler.current_thread", 24)));


spite_foreign_1_67 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("GetActiveProcessorCount", 23)), ((SpiteString)SPITE_STATIC_STRING("ThreadPool.processor_count", 26)));
spite_foreign_1_68 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("InitializeConditionVariable", 27)), ((SpiteString)SPITE_STATIC_STRING("ThreadPool.create_condition", 27)));
spite_foreign_1_69 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("SleepConditionVariableSRW", 25)), ((SpiteString)SPITE_STATIC_STRING("ThreadPool.wait_for_signal", 26)));
spite_foreign_1_70 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("WakeConditionVariable", 21)), ((SpiteString)SPITE_STATIC_STRING("ThreadPool.signal_one", 21)));
spite_foreign_1_71 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("WakeAllConditionVariable", 24)), ((SpiteString)SPITE_STATIC_STRING("ThreadPool.signal_all", 21)));





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
DynamicLibrary* made = DynamicLibrary___make(((SpiteString)SPITE_STATIC_STRING("ucrtbase.dll", 12)), spite_symbol_8, ((SpiteString)SPITE_STATIC_STRING("", 0)));
spite_foreign_library_2_tracked = spite_singleton_tracked();
(void)&DynamicLibrary_find_symbol;















SPITE_SINGLETON_PUBLISH(spite_foreign_library_2_cache, made);
}
SPITE_UNLOCK(spite_foreign_library_2_lock);
return spite_foreign_library_2_cache;
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
static inline void spite_spin_pause(void) {
#if defined(__x86_64__) || defined(__i386__)
__builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
__asm__ __volatile__("yield");
#endif
}
static void spite_guard_wait(SpiteGuard* guard, int64_t spite_me) {
int32_t spite_backoff = 1;
for (;;) {
for (int32_t spite_spin = 0; spite_spin < spite_backoff; spite_spin = spite_spin + 1) spite_spin_pause();
if (spite_backoff < 1024) {
spite_backoff = spite_backoff * 2;
} else {
#ifdef _WIN32
SwitchToThread();
#else
extern int sched_yield(void);
sched_yield();
#endif
}
int64_t spite_free = 0;
if (__atomic_load_n(&guard->owner, __ATOMIC_RELAXED) == 0 && __atomic_compare_exchange_n(&guard->owner, &spite_free, spite_me, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) return;
}
}
static SPITE_THREAD_LOCAL char spite_guard_thread;
static void spite_guard_enter(SpiteGuard* guard) {
int64_t spite_me = (int64_t)(intptr_t)&spite_guard_thread;
if (__atomic_load_n(&guard->owner, __ATOMIC_ACQUIRE) == spite_me) { guard->depth = guard->depth + 1; return; }
int64_t spite_free = 0;
if (!__atomic_compare_exchange_n(&guard->owner, &spite_free, spite_me, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) spite_guard_wait(guard, spite_me);
guard->depth = 1;
}
static void spite_enter_skipped(void* guard) { spite_guard_enter((SpiteGuard*)guard); }
void Launcher_Launcher(Launcher* self) {
(void)0;
(void)0;
({ Naive* spite_entry_instance = Naive___allocate(); Naive_Naive(spite_entry_instance); Naive___release(spite_entry_instance); (void)0; });
}
SpiteString SpiteBoolean_to_string(bool self) {
if ((self)) {
SpiteString spite_temp_1 = spite_lit_5;
return spite_temp_1;
}
SpiteString spite_temp_2 = spite_lit_6;
return spite_temp_2;
}
void Clock_Clock(Clock* self) {
int64_t spite_temp_3[1];
int64_t spite_temp_4 = SpiteInteger_to_long(8);
int64_t frequency_ = spite_temp_4 <= 8 ? (int64_t)(intptr_t)spite_temp_3 : Memory_Heap_allocate(self->heap_, spite_temp_4);
(void)(({ spite_last_foreign_call = "QueryPerformanceFrequency\tlibrary=kernel32.dll\tfrom=library/windows/clock.spite:6"; int32_t spite_temp_5 = ((int32_t (*)(int64_t))spite_foreign_1_0)((int64_t)(frequency_));  int32_t spite_foreign_result = spite_temp_5;  (void)spite_foreign_result; spite_temp_5; }));
self->_ticks_per_second_ = SpiteMemory_Address_read_long(frequency_, SpiteInteger_to_long(0));
if (frequency_ != (int64_t)(intptr_t)spite_temp_3) Memory_Heap_free(self->heap_, frequency_);
}
int64_t Clock_elapsed_nanoseconds(Clock* self) {
int64_t spite_temp_6[1];
int64_t spite_temp_7 = SpiteInteger_to_long(8);
int64_t counter_ = spite_temp_7 <= 8 ? (int64_t)(intptr_t)spite_temp_6 : Memory_Heap_allocate(self->heap_, spite_temp_7);
(void)(({ spite_last_foreign_call = "QueryPerformanceCounter\tlibrary=kernel32.dll\tfrom=library/windows/clock.spite:13"; int32_t spite_temp_8 = ((int32_t (*)(int64_t))spite_foreign_1_1)((int64_t)(counter_));  int32_t spite_foreign_result = spite_temp_8;  (void)spite_foreign_result; spite_temp_8; }));
int64_t ticks_ = SpiteMemory_Address_read_long(counter_, SpiteInteger_to_long(0));
if (counter_ != (int64_t)(intptr_t)spite_temp_6) Memory_Heap_free(self->heap_, counter_);
int64_t spite_temp_9 = ({ int64_t spite_temp_10 = ({ int64_t spite_temp_11 = ({ int64_t spite_temp_12 = ticks_; int64_t spite_temp_13 = self->_ticks_per_second_; if (spite_temp_13 == 0) spite_divided_by_zero("ticks / _ticks_per_second", spite_site_1()); int64_t spite_temp_14 = 0; if (__builtin_expect(spite_temp_13 == -1 && __builtin_sub_overflow((int64_t)0, spite_temp_12, &spite_temp_14), 0)) spite_overflowed("ticks / _ticks_per_second", "a Long", "/", (int64_t)spite_temp_12, (int64_t)spite_temp_13, spite_site_1()); (int64_t)(spite_temp_13 == -1 ? spite_temp_14 : spite_temp_12 / spite_temp_13); }); int64_t spite_temp_15 = SpiteInteger_to_long(1000000000); int64_t spite_temp_16; if (__builtin_expect(__builtin_mul_overflow(spite_temp_11, spite_temp_15, &spite_temp_16), 0)) spite_overflowed("ticks / _ticks_per_second * 1000000000", "a Long", "*", (int64_t)spite_temp_11, (int64_t)spite_temp_15, spite_site_1()); spite_temp_16; }); int64_t spite_temp_17 = ({ int64_t spite_temp_18 = ({ int64_t spite_temp_19 = ({ int64_t spite_temp_20 = ticks_; int64_t spite_temp_21 = self->_ticks_per_second_; if (spite_temp_21 == 0) spite_divided_by_zero("ticks % _ticks_per_second", spite_site_1()); (int64_t)(spite_temp_21 == -1 ? (int64_t)0 : spite_temp_20 % spite_temp_21); }); int64_t spite_temp_22 = SpiteInteger_to_long(1000000000); int64_t spite_temp_23; if (__builtin_expect(__builtin_mul_overflow(spite_temp_19, spite_temp_22, &spite_temp_23), 0)) spite_overflowed("ticks % _ticks_per_second * 1000000000", "a Long", "*", (int64_t)spite_temp_19, (int64_t)spite_temp_22, spite_site_1()); spite_temp_23; }); int64_t spite_temp_24 = self->_ticks_per_second_; if (spite_temp_24 == 0) spite_divided_by_zero("ticks % _ticks_per_second * 1000000000 / _ticks_per_second", spite_site_1()); int64_t spite_temp_25 = 0; if (__builtin_expect(spite_temp_24 == -1 && __builtin_sub_overflow((int64_t)0, spite_temp_18, &spite_temp_25), 0)) spite_overflowed("ticks % _ticks_per_second * 1000000000 / _ticks_per_second", "a Long", "/", (int64_t)spite_temp_18, (int64_t)spite_temp_24, spite_site_1()); (int64_t)(spite_temp_24 == -1 ? spite_temp_25 : spite_temp_18 / spite_temp_24); }); int64_t spite_temp_26; if (__builtin_expect(__builtin_add_overflow(spite_temp_10, spite_temp_17, &spite_temp_26), 0)) spite_overflowed("ticks / _ticks_per_second * 1000000000 + ticks % _ticks_per_second * 1000000000 / _ticks_per_second", "a Long", "+", (int64_t)spite_temp_10, (int64_t)spite_temp_17, spite_site_1()); spite_temp_26; });
return spite_temp_9;
}
void Console_print(Console* self, List_Console_Printable* values_) {
Console__write_values(self, List_Console_Printable___retain(values_), Console_Stream_output);
Console__write_output(self, spite_lit_7);
Console__flush(self);
List_Console_Printable___release(values_);
}
void Console_error(Console* self, List_Console_Printable* values_) {
Console__flush(self);
Console__write_values(self, List_Console_Printable___retain(values_), Console_Stream_error);
Console__write_error(self, spite_lit_8);
Console__flush(self);
List_Console_Printable___release(values_);
}
void Console__write_values(Console* self, List_Console_Printable* values_, Console_Stream stream_) {
int32_t index_ = 0;
while (((index_ < spite_folded_List_Console_Printable_count(values_)))) {
if (((index_ > 0))) {
Console__write_to(self, spite_lit_9, stream_);
}
SpiteString text_ = ({ Console_Printable spite_temp_27 = ({ Console_Printable spite_temp_28 = List_Console_Printable_get_at(values_, index_); if (__builtin_expect(!(SPITE_TAGGED_PRESENT(spite_temp_28)), 0)) spite_outside_list("values[index]", spite_site_2()); spite_temp_28; }); SpiteString spite_temp_29 = Console_Printable___call_to_string(spite_temp_27); Console_Printable___release(spite_temp_27); spite_temp_29; });
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
void Duration_Duration(Duration* self, int64_t amount_, Duration_Unit unit_) {
int64_t per_second_ = Duration__units_per_second(self, unit_);
int64_t seconds_per_unit_ = Duration__seconds_per_unit(self, unit_);
self->_seconds_ = ({ int64_t spite_temp_30 = ({ int64_t spite_temp_31 = amount_; int64_t spite_temp_32 = per_second_; if (spite_temp_32 == 0) spite_divided_by_zero("amount / per_second", spite_site_3()); int64_t spite_temp_33 = 0; if (__builtin_expect(spite_temp_32 == -1 && __builtin_sub_overflow((int64_t)0, spite_temp_31, &spite_temp_33), 0)) spite_overflowed("amount / per_second", "a Long", "/", (int64_t)spite_temp_31, (int64_t)spite_temp_32, spite_site_3()); (int64_t)(spite_temp_32 == -1 ? spite_temp_33 : spite_temp_31 / spite_temp_32); }); int64_t spite_temp_34 = seconds_per_unit_; int64_t spite_temp_35; if (__builtin_expect(__builtin_mul_overflow(spite_temp_30, spite_temp_34, &spite_temp_35), 0)) spite_overflowed("amount / per_second * seconds_per_unit", "a Long", "*", (int64_t)spite_temp_30, (int64_t)spite_temp_34, spite_site_3()); spite_temp_35; });
int32_t nanoseconds_per_unit_ = Duration__nanoseconds_per_unit(self, unit_);
self->_nanoseconds_ = ({ int64_t spite_temp_36 = ({ int64_t spite_temp_37 = ({ int64_t spite_temp_38 = amount_; int64_t spite_temp_39 = per_second_; if (spite_temp_39 == 0) spite_divided_by_zero("amount % per_second", spite_site_4()); (int64_t)(spite_temp_39 == -1 ? (int64_t)0 : spite_temp_38 % spite_temp_39); }); int64_t spite_temp_40 = SpiteInteger_to_long(nanoseconds_per_unit_); int64_t spite_temp_41; if (__builtin_expect(__builtin_mul_overflow(spite_temp_37, spite_temp_40, &spite_temp_41), 0)) spite_overflowed("amount % per_second * nanoseconds_per_unit", "a Long", "*", (int64_t)spite_temp_37, (int64_t)spite_temp_40, spite_site_4()); spite_temp_41; }); if (__builtin_expect(spite_temp_36 < INT32_MIN || spite_temp_36 > INT32_MAX, 0)) spite_narrowed((int64_t)spite_temp_36, "a Long", "an Integer", spite_site_4()); (int32_t)spite_temp_36; });
}
int64_t Duration_total(Duration* self, Duration_Unit unit_) {
int64_t per_second_ = Duration__units_per_second(self, unit_);
int64_t seconds_per_unit_ = Duration__seconds_per_unit(self, unit_);
int32_t nanoseconds_per_unit_ = Duration__nanoseconds_per_unit(self, unit_);
int64_t spite_temp_42 = ({ int64_t spite_temp_43 = ({ int64_t spite_temp_44 = ({ int64_t spite_temp_45 = self->_seconds_; int64_t spite_temp_46 = per_second_; int64_t spite_temp_47; if (__builtin_expect(__builtin_mul_overflow(spite_temp_45, spite_temp_46, &spite_temp_47), 0)) spite_overflowed("_seconds * per_second", "a Long", "*", (int64_t)spite_temp_45, (int64_t)spite_temp_46, spite_site_5()); spite_temp_47; }); int64_t spite_temp_48 = seconds_per_unit_; if (spite_temp_48 == 0) spite_divided_by_zero("_seconds * per_second / seconds_per_unit", spite_site_5()); int64_t spite_temp_49 = 0; if (__builtin_expect(spite_temp_48 == -1 && __builtin_sub_overflow((int64_t)0, spite_temp_44, &spite_temp_49), 0)) spite_overflowed("_seconds * per_second / seconds_per_unit", "a Long", "/", (int64_t)spite_temp_44, (int64_t)spite_temp_48, spite_site_5()); (int64_t)(spite_temp_48 == -1 ? spite_temp_49 : spite_temp_44 / spite_temp_48); }); int64_t spite_temp_50 = SpiteInteger_to_long(({ int32_t spite_temp_51 = self->_nanoseconds_; int32_t spite_temp_52 = nanoseconds_per_unit_; if (spite_temp_52 == 0) spite_divided_by_zero("_nanoseconds / nanoseconds_per_unit", spite_site_5()); int32_t spite_temp_53 = 0; if (__builtin_expect(spite_temp_52 == -1 && __builtin_sub_overflow((int32_t)0, spite_temp_51, &spite_temp_53), 0)) spite_overflowed("_nanoseconds / nanoseconds_per_unit", "an Integer", "/", (int64_t)spite_temp_51, (int64_t)spite_temp_52, spite_site_5()); (int32_t)(spite_temp_52 == -1 ? spite_temp_53 : spite_temp_51 / spite_temp_52); })); int64_t spite_temp_54; if (__builtin_expect(__builtin_add_overflow(spite_temp_43, spite_temp_50, &spite_temp_54), 0)) spite_overflowed("_seconds * per_second / seconds_per_unit + _nanoseconds / nanoseconds_per_unit", "a Long", "+", (int64_t)spite_temp_43, (int64_t)spite_temp_50, spite_site_5()); spite_temp_54; });
return spite_temp_42;
}
int64_t Duration__units_per_second(Duration* self, Duration_Unit unit_) {
{
Duration_Unit spite_temp_55 = unit_;
if (spite_temp_55 == Duration_Unit_nanoseconds) {
int64_t spite_temp_56 = SpiteInteger_to_long(1000000000);
return spite_temp_56;
}
else if (spite_temp_55 == Duration_Unit_microseconds) {
int64_t spite_temp_57 = SpiteInteger_to_long(1000000);
return spite_temp_57;
}
else if (spite_temp_55 == Duration_Unit_milliseconds) {
int64_t spite_temp_58 = SpiteInteger_to_long(1000);
return spite_temp_58;
}
else {
int64_t spite_temp_59 = SpiteInteger_to_long(1);
return spite_temp_59;
}
}
return 0;
}
int32_t Duration__nanoseconds_per_unit(Duration* self, Duration_Unit unit_) {
{
Duration_Unit spite_temp_60 = unit_;
if (spite_temp_60 == Duration_Unit_nanoseconds) {
int32_t spite_temp_61 = 1;
return spite_temp_61;
}
else if (spite_temp_60 == Duration_Unit_microseconds) {
int32_t spite_temp_62 = 1000;
return spite_temp_62;
}
else if (spite_temp_60 == Duration_Unit_milliseconds) {
int32_t spite_temp_63 = 1000000;
return spite_temp_63;
}
else {
int32_t spite_temp_64 = 1000000000;
return spite_temp_64;
}
}
return 0;
}
int64_t Duration__seconds_per_unit(Duration* self, Duration_Unit unit_) {
if (((unit_ == Duration_Unit_hours))) {
int64_t spite_temp_65 = SpiteInteger_to_long(3600);
return spite_temp_65;
}
if (((unit_ == Duration_Unit_minutes))) {
int64_t spite_temp_66 = SpiteInteger_to_long(60);
return spite_temp_66;
}
int64_t spite_temp_67 = SpiteInteger_to_long(1);
return spite_temp_67;
}
void DynamicLibrary_DynamicLibrary(DynamicLibrary* self, SpiteString file_, SpiteString _naming_, SpiteString _header_) {
SpiteString spite_temp_68 = SpiteString___retain(file_);
SpiteString___release(self->file_name_);
self->file_name_ = spite_temp_68;
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
SpiteString SpiteInteger_to_string(int32_t self) {
int64_t wide_ = SpiteInteger_to_long(self);
SpiteString spite_temp_69 = SpiteLong_to_string(wide_);
return spite_temp_69;
}
SpiteString SpiteLong_to_string(int64_t self) {
if (((self == SpiteInteger_to_long(0)))) {
SpiteString spite_temp_70 = spite_lit_10;
return spite_temp_70;
}
Memory_Heap* heap_ = spite_singleton_Memory_Heap();
int64_t buffer_bytes_ = SpiteInteger_to_long(24);
int64_t spite_temp_71[32];
int64_t spite_temp_72 = buffer_bytes_;
int64_t address_ = spite_temp_72 <= 256 ? (int64_t)(intptr_t)spite_temp_71 : Memory_Heap_allocate(heap_, spite_temp_72);
int64_t position_ = buffer_bytes_;
int64_t rest_ = self;
while (((rest_ != SpiteInteger_to_long(0)))) {
int64_t digit_ = (rest_ % SpiteInteger_to_long(10));
if (((digit_ < SpiteInteger_to_long(0)))) {
digit_ = (-(digit_));
}
position_ = ({ int64_t spite_temp_73 = position_; int64_t spite_temp_74 = SpiteInteger_to_long(1); int64_t spite_temp_75; if (__builtin_expect(__builtin_sub_overflow(spite_temp_73, spite_temp_74, &spite_temp_75), 0)) spite_overflowed("position - 1", "a Long", "-", (int64_t)spite_temp_73, (int64_t)spite_temp_74, spite_site_6()); spite_temp_75; });
SpiteMemory_Address_write_byte(address_, position_, ({ int64_t spite_temp_76 = (digit_ + SpiteInteger_to_long(48)); if (__builtin_expect(spite_temp_76 < 0 || spite_temp_76 > UINT8_MAX, 0)) spite_narrowed((int64_t)spite_temp_76, "a Long", "a Byte", spite_site_7()); (uint8_t)spite_temp_76; }));
rest_ = (rest_ / SpiteInteger_to_long(10));
}
if (((self < SpiteInteger_to_long(0)))) {
position_ = ({ int64_t spite_temp_77 = position_; int64_t spite_temp_78 = SpiteInteger_to_long(1); int64_t spite_temp_79; if (__builtin_expect(__builtin_sub_overflow(spite_temp_77, spite_temp_78, &spite_temp_79), 0)) spite_overflowed("position - 1", "a Long", "-", (int64_t)spite_temp_77, (int64_t)spite_temp_78, spite_site_8()); spite_temp_79; });
SpiteMemory_Address_write_byte(address_, position_, SpiteInteger_to_byte(45));
}
int64_t first_digit_ = (address_ + ((int64_t)(position_)));
SpiteString text_ = SpiteMemory_Address_text(first_digit_, ({ int64_t spite_temp_80 = buffer_bytes_; int64_t spite_temp_81 = position_; int64_t spite_temp_82; if (__builtin_expect(__builtin_sub_overflow(spite_temp_80, spite_temp_81, &spite_temp_82), 0)) spite_overflowed("buffer_bytes - position", "a Long", "-", (int64_t)spite_temp_80, (int64_t)spite_temp_81, spite_site_9()); spite_temp_82; }));
if (address_ != (int64_t)(intptr_t)spite_temp_71) Memory_Heap_free(heap_, address_);
SpiteString spite_temp_83 = SpiteString___retain(text_);
SpiteString___release(text_);
spite_folded_Memory_Heap___release(heap_);
return spite_temp_83;
}
SpiteString SpiteString_to_string(SpiteString self) {
SpiteString spite_temp_84 = SpiteString___retain(self);
return spite_temp_84;
}
int32_t ThreadPool_size(ThreadPool* self) {
ThreadPool_start(self);
int32_t spite_temp_85 = self->worker_count_;
return spite_temp_85;
}
bool ThreadPool_is_done(ThreadPool* self, int64_t state_) {
bool spite_temp_86 = (SpiteMemory_Address_read_long_atomically(state_, SpiteInteger_to_long(0)) == SpiteInteger_to_long(2));
return spite_temp_86;
}
void ThreadPool_join(ThreadPool* self, int64_t state_) {
ThreadPool_lock_queue(self);
int32_t position_ = (List_ThreadPoolJob_count(self->jobs_) - 1);
while (((((((position_ >= 0)) && ((position_ < List_ThreadPoolJob_count(self->jobs_))))) && ((({ ThreadPoolJob* spite_temp_87 = ({ ThreadPoolJob* spite_temp_88 = List_ThreadPoolJob_get_at(self->jobs_, position_); if (__builtin_expect(!(((spite_temp_88) != 0)), 0)) spite_outside_list("jobs[position]", spite_site_10()); spite_temp_88; }); int64_t spite_temp_89 = (spite_temp_87)->state_; ThreadPoolJob___release(spite_temp_87); spite_temp_89; }) != state_))))) {
position_ = (position_ - 1);
}
if (((position_ >= 0))) {
ThreadPool_run_queued(self, position_);
}
while (((!(ThreadPool_is_done(self, state_))))) {
ThreadPool_wait_for_signal(self, self->work_done_);
}
ThreadPool_unlock_queue(self);
}
int32_t ThreadPool_piece_start(ThreadPool* self, int32_t index_, int32_t base_, int32_t extra_) {
if (((index_ < extra_))) {
int32_t spite_temp_90 = ({ int32_t spite_temp_91 = ({ int32_t spite_temp_92 = index_; int32_t spite_temp_93 = base_; int32_t spite_temp_94; if (__builtin_expect(__builtin_mul_overflow(spite_temp_92, spite_temp_93, &spite_temp_94), 0)) spite_overflowed("index * base", "an Integer", "*", (int64_t)spite_temp_92, (int64_t)spite_temp_93, spite_site_11()); spite_temp_94; }); int32_t spite_temp_95 = index_; int32_t spite_temp_96; if (__builtin_expect(__builtin_add_overflow(spite_temp_91, spite_temp_95, &spite_temp_96), 0)) spite_overflowed("index * base + index", "an Integer", "+", (int64_t)spite_temp_91, (int64_t)spite_temp_95, spite_site_11()); spite_temp_96; });
return spite_temp_90;
}
int32_t spite_temp_97 = ({ int32_t spite_temp_98 = ({ int32_t spite_temp_99 = index_; int32_t spite_temp_100 = base_; int32_t spite_temp_101; if (__builtin_expect(__builtin_mul_overflow(spite_temp_99, spite_temp_100, &spite_temp_101), 0)) spite_overflowed("index * base", "an Integer", "*", (int64_t)spite_temp_99, (int64_t)spite_temp_100, spite_site_12()); spite_temp_101; }); int32_t spite_temp_102 = extra_; int32_t spite_temp_103; if (__builtin_expect(__builtin_add_overflow(spite_temp_98, spite_temp_102, &spite_temp_103), 0)) spite_overflowed("index * base + extra", "an Integer", "+", (int64_t)spite_temp_98, (int64_t)spite_temp_102, spite_site_12()); spite_temp_103; });
return spite_temp_97;
}
void ThreadPool_start(ThreadPool* self) {
if (((self->worker_count_ == 0))) {
self->queue_lock_ = ThreadPool_create_lock(self);
self->work_ready_ = ThreadPool_create_condition(self);
self->work_done_ = ThreadPool_create_condition(self);
int32_t wanted_ = ({ int32_t spite_temp_104 = ThreadPool_processor_count(self); int32_t spite_temp_105 = 1; int32_t spite_temp_106; if (__builtin_expect(__builtin_sub_overflow(spite_temp_104, spite_temp_105, &spite_temp_106), 0)) spite_overflowed("processor_count() - 1", "an Integer", "-", (int64_t)spite_temp_104, (int64_t)spite_temp_105, spite_site_13()); spite_temp_106; });
if (((wanted_ < 1))) {
wanted_ = 1;
}
self->workers_ = Memory_Heap_allocate(self->heap_, SpiteInteger_to_long(({ int32_t spite_temp_107 = wanted_; int32_t spite_temp_108 = 8; int32_t spite_temp_109; if (__builtin_expect(__builtin_mul_overflow(spite_temp_107, spite_temp_108, &spite_temp_109), 0)) spite_overflowed("wanted * 8", "an Integer", "*", (int64_t)spite_temp_107, (int64_t)spite_temp_108, spite_site_14()); spite_temp_109; })));
self->worker_threads_ = Memory_Heap_allocate(self->heap_, SpiteInteger_to_long(({ int32_t spite_temp_110 = wanted_; int32_t spite_temp_111 = 8; int32_t spite_temp_112; if (__builtin_expect(__builtin_mul_overflow(spite_temp_110, spite_temp_111, &spite_temp_112), 0)) spite_overflowed("wanted * 8", "an Integer", "*", (int64_t)spite_temp_110, (int64_t)spite_temp_111, spite_site_15()); spite_temp_112; })));
int64_t entry_ = ThreadPool_entry_address(self);
int64_t pool_ = ThreadPool_address(self);
int32_t index_ = 0;
while (((index_ < wanted_))) {
int64_t thread_ = ThreadPool_start_thread(self, entry_, pool_);
if (!(((thread_ != SpiteInteger_to_long(0))))) {
spite_failed_1(thread_, wanted_, entry_, pool_, index_, self);
}
SpiteMemory_Address_write_long(self->workers_, SpiteInteger_to_long(({ int32_t spite_temp_113 = index_; int32_t spite_temp_114 = 8; int32_t spite_temp_115; if (__builtin_expect(__builtin_mul_overflow(spite_temp_113, spite_temp_114, &spite_temp_115), 0)) spite_overflowed("index * 8", "an Integer", "*", (int64_t)spite_temp_113, (int64_t)spite_temp_114, spite_site_17()); spite_temp_115; })), thread_);
index_ = (index_ + 1);
}
self->worker_count_ = wanted_;
}
}
static SPITE_CRASH_REPORT void spite_failed_1(int64_t thread_, int32_t wanted_, int64_t entry_, int64_t pool_, int32_t index_, ThreadPool* self) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_16(), stderr);
fputs("\tthread=", stderr);
{ SpiteString spite_temp_116 = SpiteLong_to_string(thread_); fwrite(spite_string_bytes(&spite_temp_116), 1, (size_t)spite_string_length(spite_temp_116), stderr); SpiteString___release(spite_temp_116); }
fputs("\twanted=", stderr);
{ SpiteString spite_temp_117 = SpiteInteger_to_string(wanted_); spite_crash_text(spite_string_bytes(&spite_temp_117), spite_string_length(spite_temp_117)); SpiteString___release(spite_temp_117); }
fputs("\tentry=", stderr);
{ SpiteString spite_temp_118 = SpiteLong_to_string(entry_); spite_crash_text(spite_string_bytes(&spite_temp_118), spite_string_length(spite_temp_118)); SpiteString___release(spite_temp_118); }
fputs("\tpool=", stderr);
{ SpiteString spite_temp_119 = SpiteLong_to_string(pool_); spite_crash_text(spite_string_bytes(&spite_temp_119), spite_string_length(spite_temp_119)); SpiteString___release(spite_temp_119); }
fputs("\tindex=", stderr);
{ SpiteString spite_temp_120 = SpiteInteger_to_string(index_); spite_crash_text(spite_string_bytes(&spite_temp_120), spite_string_length(spite_temp_120)); SpiteString___release(spite_temp_120); }
fputs("\tworkers=", stderr);
{ SpiteString spite_temp_121 = SpiteMemory_Address_to_string(self->workers_); spite_crash_text(spite_string_bytes(&spite_temp_121), spite_string_length(spite_temp_121)); SpiteString___release(spite_temp_121); }
fputs("\tworker_count=", stderr);
{ SpiteString spite_temp_122 = SpiteInteger_to_string(self->worker_count_); spite_crash_text(spite_string_bytes(&spite_temp_122), spite_string_length(spite_temp_122)); SpiteString___release(spite_temp_122); }
fputs("\tworker_threads=", stderr);
{ SpiteString spite_temp_123 = SpiteMemory_Address_to_string(self->worker_threads_); spite_crash_text(spite_string_bytes(&spite_temp_123), spite_string_length(spite_temp_123)); SpiteString___release(spite_temp_123); }
fputs("\tregistered=", stderr);
{ SpiteString spite_temp_124 = SpiteInteger_to_string(self->registered_); spite_crash_text(spite_string_bytes(&spite_temp_124), spite_string_length(spite_temp_124)); SpiteString___release(spite_temp_124); }
fputs("\tqueue_lock=", stderr);
{ SpiteString spite_temp_125 = SpiteLong_to_string(self->queue_lock_); spite_crash_text(spite_string_bytes(&spite_temp_125), spite_string_length(spite_temp_125)); SpiteString___release(spite_temp_125); }
fputs("\twork_ready=", stderr);
{ SpiteString spite_temp_126 = SpiteLong_to_string(self->work_ready_); spite_crash_text(spite_string_bytes(&spite_temp_126), spite_string_length(spite_temp_126)); SpiteString___release(spite_temp_126); }
fputs("\twork_done=", stderr);
{ SpiteString spite_temp_127 = SpiteLong_to_string(self->work_done_); spite_crash_text(spite_string_bytes(&spite_temp_127), spite_string_length(spite_temp_127)); SpiteString___release(spite_temp_127); }
fputs("\tstopping=", stderr);
{ SpiteString spite_temp_128 = SpiteBoolean_to_string(self->stopping_); spite_crash_text(spite_string_bytes(&spite_temp_128), spite_string_length(spite_temp_128)); SpiteString___release(spite_temp_128); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
void ThreadPool__serve(ThreadPool* self) {
int64_t thread_ = ThreadPool_current_thread(self);
ThreadPool_lock_queue(self);
SpiteMemory_Address_write_long(self->worker_threads_, SpiteInteger_to_long(({ int32_t spite_temp_129 = self->registered_; int32_t spite_temp_130 = 8; int32_t spite_temp_131; if (__builtin_expect(__builtin_mul_overflow(spite_temp_129, spite_temp_130, &spite_temp_131), 0)) spite_overflowed("registered * 8", "an Integer", "*", (int64_t)spite_temp_129, (int64_t)spite_temp_130, spite_site_18()); spite_temp_131; })), thread_);
self->registered_ = ({ int32_t spite_temp_132 = self->registered_; int32_t spite_temp_133 = 1; int32_t spite_temp_134; if (__builtin_expect(__builtin_add_overflow(spite_temp_132, spite_temp_133, &spite_temp_134), 0)) spite_overflowed("registered + 1", "an Integer", "+", (int64_t)spite_temp_132, (int64_t)spite_temp_133, spite_site_19()); spite_temp_134; });
while (((((!(self->stopping_))) || ((!(List_ThreadPoolJob_is_empty(self->jobs_))))))) {
if ((List_ThreadPoolJob_is_empty(self->jobs_))) {
ThreadPool_wait_for_signal(self, self->work_ready_);
}
else {
ThreadPool_run_queued(self, 0);
}
}
ThreadPool_unlock_queue(self);
}
void ThreadPool_run_queued(ThreadPool* self, int32_t position_) {
if (!(({ List_ThreadPoolJob* spite_temp_135 = self->jobs_; int32_t spite_temp_136 = position_; (spite_temp_136 >= 0 && spite_temp_136 < (spite_temp_135)->item_count_) && ((((ThreadPoolJob**)(intptr_t)(spite_temp_135)->items_)[spite_temp_136]) != 0); }))) {
spite_failed_2(position_, self);
}
ThreadPoolJob* job_ = List_ThreadPoolJob_get_at(self->jobs_, position_);
List_ThreadPoolJob_remove_at(self->jobs_, position_);
SpiteMemory_Address_write_long_atomically((job_)->state_, SpiteInteger_to_long(0), SpiteInteger_to_long(1));
ThreadPool_unlock_queue(self);
if (!((((job_)->work_) != 0))) {
spite_failed_3(position_, self);
}
ThreadPool___release(self);
({ Spite_Function* spite_temp_137 = (job_)->work_; ((void (*)(void*, int32_t, int32_t))spite_temp_137->spite_typed_call)(spite_temp_137->spite_owner, (job_)->first_, (job_)->end_); });
ThreadPool___release(self);
ThreadPool__task_ended(self);
ThreadPool_lock_queue(self);
SpiteMemory_Address_write_long_atomically((job_)->state_, SpiteInteger_to_long(0), SpiteInteger_to_long(2));
ThreadPool_signal_all(self, self->work_done_);
ThreadPoolJob___release(job_);
}
static SPITE_CRASH_REPORT void spite_failed_2(int32_t position_, ThreadPool* self) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_20(), stderr);
{
fputs("\tjobs[position] is missing: index ", stderr);
{ SpiteString spite_temp_138 = SpiteInteger_to_string(position_); fwrite(spite_string_bytes(&spite_temp_138), 1, (size_t)spite_string_length(spite_temp_138), stderr); SpiteString___release(spite_temp_138); }
fputs(", count ", stderr);
{ SpiteString spite_temp_139 = SpiteInteger_to_string(((self->jobs_)->item_count_)); fwrite(spite_string_bytes(&spite_temp_139), 1, (size_t)spite_string_length(spite_temp_139), stderr); SpiteString___release(spite_temp_139); }
}
fputs("\tworkers=", stderr);
{ SpiteString spite_temp_140 = SpiteMemory_Address_to_string(self->workers_); spite_crash_text(spite_string_bytes(&spite_temp_140), spite_string_length(spite_temp_140)); SpiteString___release(spite_temp_140); }
fputs("\tworker_count=", stderr);
{ SpiteString spite_temp_141 = SpiteInteger_to_string(self->worker_count_); spite_crash_text(spite_string_bytes(&spite_temp_141), spite_string_length(spite_temp_141)); SpiteString___release(spite_temp_141); }
fputs("\tworker_threads=", stderr);
{ SpiteString spite_temp_142 = SpiteMemory_Address_to_string(self->worker_threads_); spite_crash_text(spite_string_bytes(&spite_temp_142), spite_string_length(spite_temp_142)); SpiteString___release(spite_temp_142); }
fputs("\tregistered=", stderr);
{ SpiteString spite_temp_143 = SpiteInteger_to_string(self->registered_); spite_crash_text(spite_string_bytes(&spite_temp_143), spite_string_length(spite_temp_143)); SpiteString___release(spite_temp_143); }
fputs("\tqueue_lock=", stderr);
{ SpiteString spite_temp_144 = SpiteLong_to_string(self->queue_lock_); spite_crash_text(spite_string_bytes(&spite_temp_144), spite_string_length(spite_temp_144)); SpiteString___release(spite_temp_144); }
fputs("\twork_ready=", stderr);
{ SpiteString spite_temp_145 = SpiteLong_to_string(self->work_ready_); spite_crash_text(spite_string_bytes(&spite_temp_145), spite_string_length(spite_temp_145)); SpiteString___release(spite_temp_145); }
fputs("\twork_done=", stderr);
{ SpiteString spite_temp_146 = SpiteLong_to_string(self->work_done_); spite_crash_text(spite_string_bytes(&spite_temp_146), spite_string_length(spite_temp_146)); SpiteString___release(spite_temp_146); }
fputs("\tstopping=", stderr);
{ SpiteString spite_temp_147 = SpiteBoolean_to_string(self->stopping_); spite_crash_text(spite_string_bytes(&spite_temp_147), spite_string_length(spite_temp_147)); SpiteString___release(spite_temp_147); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
static SPITE_CRASH_REPORT void spite_failed_3(int32_t position_, ThreadPool* self) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_21(), stderr);
{
fputs("\tjob.work is null", stderr);
}
fputs("\tposition=", stderr);
{ SpiteString spite_temp_148 = SpiteInteger_to_string(position_); spite_crash_text(spite_string_bytes(&spite_temp_148), spite_string_length(spite_temp_148)); SpiteString___release(spite_temp_148); }
fputs("\tworkers=", stderr);
{ SpiteString spite_temp_149 = SpiteMemory_Address_to_string(self->workers_); spite_crash_text(spite_string_bytes(&spite_temp_149), spite_string_length(spite_temp_149)); SpiteString___release(spite_temp_149); }
fputs("\tworker_count=", stderr);
{ SpiteString spite_temp_150 = SpiteInteger_to_string(self->worker_count_); spite_crash_text(spite_string_bytes(&spite_temp_150), spite_string_length(spite_temp_150)); SpiteString___release(spite_temp_150); }
fputs("\tworker_threads=", stderr);
{ SpiteString spite_temp_151 = SpiteMemory_Address_to_string(self->worker_threads_); spite_crash_text(spite_string_bytes(&spite_temp_151), spite_string_length(spite_temp_151)); SpiteString___release(spite_temp_151); }
fputs("\tregistered=", stderr);
{ SpiteString spite_temp_152 = SpiteInteger_to_string(self->registered_); spite_crash_text(spite_string_bytes(&spite_temp_152), spite_string_length(spite_temp_152)); SpiteString___release(spite_temp_152); }
fputs("\tqueue_lock=", stderr);
{ SpiteString spite_temp_153 = SpiteLong_to_string(self->queue_lock_); spite_crash_text(spite_string_bytes(&spite_temp_153), spite_string_length(spite_temp_153)); SpiteString___release(spite_temp_153); }
fputs("\twork_ready=", stderr);
{ SpiteString spite_temp_154 = SpiteLong_to_string(self->work_ready_); spite_crash_text(spite_string_bytes(&spite_temp_154), spite_string_length(spite_temp_154)); SpiteString___release(spite_temp_154); }
fputs("\twork_done=", stderr);
{ SpiteString spite_temp_155 = SpiteLong_to_string(self->work_done_); spite_crash_text(spite_string_bytes(&spite_temp_155), spite_string_length(spite_temp_155)); SpiteString___release(spite_temp_155); }
fputs("\tstopping=", stderr);
{ SpiteString spite_temp_156 = SpiteBoolean_to_string(self->stopping_); spite_crash_text(spite_string_bytes(&spite_temp_156), spite_string_length(spite_temp_156)); SpiteString___release(spite_temp_156); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
void ThreadPool_drop(ThreadPool* self) {
if (((self->worker_count_ > 0))) {
ThreadPool_lock_queue(self);
self->stopping_ = true;
ThreadPool_signal_all(self, self->work_ready_);
ThreadPool_unlock_queue(self);
int32_t index_ = 0;
while (((index_ < self->worker_count_))) {
int64_t worker_ = SpiteMemory_Address_read_long(self->workers_, SpiteInteger_to_long(({ int32_t spite_temp_157 = index_; int32_t spite_temp_158 = 8; int32_t spite_temp_159; if (__builtin_expect(__builtin_mul_overflow(spite_temp_157, spite_temp_158, &spite_temp_159), 0)) spite_overflowed("index * 8", "an Integer", "*", (int64_t)spite_temp_157, (int64_t)spite_temp_158, spite_site_22()); spite_temp_159; })));
ThreadPool_join_thread(self, worker_);
index_ = (index_ + 1);
}
Memory_Heap_free(self->heap_, self->workers_);
Memory_Heap_free(self->heap_, self->worker_threads_);
ThreadPool_destroy_lock(self, self->work_ready_);
ThreadPool_destroy_lock(self, self->work_done_);
ThreadPool_destroy_lock(self, self->queue_lock_);
}
}
int32_t ThreadPool_processor_count(ThreadPool* self) {
int32_t all_groups_ = 65535;
int32_t spite_temp_160 = ({ spite_last_foreign_call = "GetActiveProcessorCount\tlibrary=kernel32.dll\tfrom=library/windows/thread_pool.spite:5"; int32_t spite_temp_161 = ((int32_t (*)(int64_t))spite_foreign_1_67)((int64_t)(all_groups_));  int32_t spite_foreign_result = spite_temp_161;  (void)spite_foreign_result; spite_temp_161; });
return spite_temp_160;
}
int64_t ThreadPool_current_thread(ThreadPool* self) {
int64_t spite_temp_162 = ({ spite_last_foreign_call = "GetCurrentThreadId\tlibrary=kernel32.dll\tfrom=library/windows/thread_pool.spite:9"; int64_t spite_temp_163 = ((int64_t (*)(void))spite_foreign_1_47)();  int64_t spite_foreign_result = spite_temp_163;  (void)spite_foreign_result; spite_temp_163; });
return spite_temp_162;
}
int64_t ThreadPool_start_thread(ThreadPool* self, int64_t entry_, int64_t argument_) {
int64_t no_value_ = SpiteInteger_to_long(0);
int64_t spite_temp_164 = ({ spite_last_foreign_call = "CreateThread\tlibrary=kernel32.dll\tfrom=library/windows/thread_pool.spite:14"; int64_t spite_temp_165 = ((int64_t (*)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t))spite_foreign_1_45)((int64_t)(no_value_), (int64_t)(no_value_), (int64_t)(entry_), (int64_t)(argument_), (int64_t)(0), (int64_t)(no_value_));  int64_t spite_foreign_result = spite_temp_165;  (void)spite_foreign_result; spite_temp_165; });
return spite_temp_164;
}
void ThreadPool_join_thread(ThreadPool* self, int64_t thread_) {
(void)(({ spite_last_foreign_call = "WaitForSingleObject\tlibrary=kernel32.dll\tfrom=library/windows/thread_pool.spite:18"; int32_t spite_temp_166 = ((int32_t (*)(int64_t, int64_t))spite_foreign_1_46)((int64_t)(thread_), (int64_t)((-(1))));  int32_t spite_foreign_result = spite_temp_166;  (void)spite_foreign_result; spite_temp_166; }));
(void)(({ spite_last_foreign_call = "CloseHandle\tlibrary=kernel32.dll\tfrom=library/windows/thread_pool.spite:19"; int32_t spite_temp_167 = ((int32_t (*)(int64_t))spite_foreign_1_22)((int64_t)(thread_));  int32_t spite_foreign_result = spite_temp_167;  (void)spite_foreign_result; spite_temp_167; }));
}
int64_t ThreadPool_create_lock(ThreadPool* self) {
int64_t created_ = Memory_Heap_allocate(self->heap_, SpiteInteger_to_long(8));
(void)(({ spite_last_foreign_call = "InitializeSRWLock\tlibrary=kernel32.dll\tfrom=library/windows/thread_pool.spite:24"; int32_t spite_temp_168 = ((int32_t (*)(int64_t))spite_foreign_1_34)((int64_t)(created_));  int32_t spite_foreign_result = spite_temp_168;  (void)spite_foreign_result; spite_temp_168; }));
int64_t spite_temp_169 = SpiteMemory_Address_to_long(created_);
return spite_temp_169;
}
void ThreadPool_destroy_lock(ThreadPool* self, int64_t created_) {
Memory_Heap_free(self->heap_, ((int64_t)(created_)));
}
void ThreadPool_lock_queue(ThreadPool* self) {
(void)(({ spite_last_foreign_call = "AcquireSRWLockExclusive\tlibrary=kernel32.dll\tfrom=library/windows/thread_pool.spite:33"; int32_t spite_temp_170 = ((int32_t (*)(int64_t))spite_foreign_1_35)((int64_t)(self->queue_lock_));  int32_t spite_foreign_result = spite_temp_170;  (void)spite_foreign_result; spite_temp_170; }));
}
void ThreadPool_unlock_queue(ThreadPool* self) {
(void)(({ spite_last_foreign_call = "ReleaseSRWLockExclusive\tlibrary=kernel32.dll\tfrom=library/windows/thread_pool.spite:37"; int32_t spite_temp_171 = ((int32_t (*)(int64_t))spite_foreign_1_36)((int64_t)(self->queue_lock_));  int32_t spite_foreign_result = spite_temp_171;  (void)spite_foreign_result; spite_temp_171; }));
}
int64_t ThreadPool_create_condition(ThreadPool* self) {
int64_t created_ = Memory_Heap_allocate(self->heap_, SpiteInteger_to_long(8));
(void)(({ spite_last_foreign_call = "InitializeConditionVariable\tlibrary=kernel32.dll\tfrom=library/windows/thread_pool.spite:42"; int32_t spite_temp_172 = ((int32_t (*)(int64_t))spite_foreign_1_68)((int64_t)(created_));  int32_t spite_foreign_result = spite_temp_172;  (void)spite_foreign_result; spite_temp_172; }));
int64_t spite_temp_173 = SpiteMemory_Address_to_long(created_);
return spite_temp_173;
}
void ThreadPool_wait_for_signal(ThreadPool* self, int64_t condition_) {
(void)(({ spite_last_foreign_call = "SleepConditionVariableSRW\tlibrary=kernel32.dll\tfrom=library/windows/thread_pool.spite:51"; int32_t spite_temp_174 = ((int32_t (*)(int64_t, int64_t, int64_t, int64_t))spite_foreign_1_69)((int64_t)(condition_), (int64_t)(self->queue_lock_), (int64_t)((-(1))), (int64_t)(0));  int32_t spite_foreign_result = spite_temp_174;  (void)spite_foreign_result; spite_temp_174; }));
}
void ThreadPool_signal_one(ThreadPool* self, int64_t condition_) {
(void)(({ spite_last_foreign_call = "WakeConditionVariable\tlibrary=kernel32.dll\tfrom=library/windows/thread_pool.spite:55"; int32_t spite_temp_175 = ((int32_t (*)(int64_t))spite_foreign_1_70)((int64_t)(condition_));  int32_t spite_foreign_result = spite_temp_175;  (void)spite_foreign_result; spite_temp_175; }));
}
void ThreadPool_signal_all(ThreadPool* self, int64_t condition_) {
(void)(({ spite_last_foreign_call = "WakeAllConditionVariable\tlibrary=kernel32.dll\tfrom=library/windows/thread_pool.spite:59"; int32_t spite_temp_176 = ((int32_t (*)(int64_t))spite_foreign_1_71)((int64_t)(condition_));  int32_t spite_foreign_result = spite_temp_176;  (void)spite_foreign_result; spite_temp_176; }));
}
int64_t ThreadPool_entry_address(ThreadPool* self) {
return (int64_t)(intptr_t)&ThreadPool___thread_entry;
}
int64_t ThreadPool_address(ThreadPool* self) {
return (int64_t)(intptr_t)self;
}
void ThreadPool__task_begun(ThreadPool* self) {
#ifdef SPITE_THREADS
for (int32_t spite_index = 0; spite_index < spite_skipped_depth; spite_index++) { if (!spite_skipped_taken[spite_index]) { spite_enter_skipped(spite_skipped[spite_index]); spite_skipped_taken[spite_index] = 1; } }
__atomic_fetch_add(&spite_tasks_in_flight, 1, __ATOMIC_SEQ_CST);
#endif
}
void ThreadPool__task_ended(ThreadPool* self) {
#ifdef SPITE_THREADS
__atomic_fetch_sub(&spite_tasks_in_flight, 1, __ATOMIC_RELEASE);
#endif
}
SpiteString SpiteMemory_Address_text(int64_t self, int64_t length_) {
return spite_string_from_bytes((const char*)(intptr_t)self, length_);
}
SpiteString SpiteMemory_Address_to_string(int64_t self) {
int64_t number_ = SpiteMemory_Address_to_long(self);
SpiteString spite_temp_177 = SpiteLong_to_string(number_);
return spite_temp_177;
}
int64_t Memory_Arena_allocate(Memory_Arena* self, int64_t bytes_) {
int64_t rounded_ = ({ int64_t spite_temp_178 = (({ int64_t spite_temp_179 = bytes_; int64_t spite_temp_180 = SpiteInteger_to_long(15); int64_t spite_temp_181; if (__builtin_expect(__builtin_add_overflow(spite_temp_179, spite_temp_180, &spite_temp_181), 0)) spite_overflowed("bytes + 15", "a Long", "+", (int64_t)spite_temp_179, (int64_t)spite_temp_180, spite_site_23()); spite_temp_181; }) / SpiteInteger_to_long(16)); int64_t spite_temp_182 = SpiteInteger_to_long(16); int64_t spite_temp_183; if (__builtin_expect(__builtin_mul_overflow(spite_temp_178, spite_temp_182, &spite_temp_183), 0)) spite_overflowed("(bytes + 15) / 16 * 16", "a Long", "*", (int64_t)spite_temp_178, (int64_t)spite_temp_182, spite_site_23()); spite_temp_183; });
if (((((self->_block_ == ((int64_t)(0)))) || ((({ int64_t spite_temp_184 = self->_used_; int64_t spite_temp_185 = rounded_; int64_t spite_temp_186; if (__builtin_expect(__builtin_add_overflow(spite_temp_184, spite_temp_185, &spite_temp_186), 0)) spite_overflowed("_used + rounded", "a Long", "+", (int64_t)spite_temp_184, (int64_t)spite_temp_185, spite_site_24()); spite_temp_186; }) > self->_end_))))) {
Memory_Arena_start_block(self, rounded_);
}
int64_t address_ = (self->_block_ + ((int64_t)(self->_used_)));
self->_used_ = ({ int64_t spite_temp_187 = self->_used_; int64_t spite_temp_188 = rounded_; int64_t spite_temp_189; if (__builtin_expect(__builtin_add_overflow(spite_temp_187, spite_temp_188, &spite_temp_189), 0)) spite_overflowed("_used + rounded", "a Long", "+", (int64_t)spite_temp_187, (int64_t)spite_temp_188, spite_site_25()); spite_temp_189; });
int64_t spite_temp_190 = address_;
return spite_temp_190;
}
void Memory_Arena_free(Memory_Arena* self, int64_t _address_) {
}
void Memory_Arena_start_block(Memory_Arena* self, int64_t at_least_) {
int64_t size_ = self->_block_bytes_;
if (((({ int64_t spite_temp_191 = at_least_; int64_t spite_temp_192 = SpiteInteger_to_long(16); int64_t spite_temp_193; if (__builtin_expect(__builtin_add_overflow(spite_temp_191, spite_temp_192, &spite_temp_193), 0)) spite_overflowed("at_least + 16", "a Long", "+", (int64_t)spite_temp_191, (int64_t)spite_temp_192, spite_site_26()); spite_temp_193; }) > size_))) {
size_ = ({ int64_t spite_temp_194 = at_least_; int64_t spite_temp_195 = SpiteInteger_to_long(16); int64_t spite_temp_196; if (__builtin_expect(__builtin_add_overflow(spite_temp_194, spite_temp_195, &spite_temp_196), 0)) spite_overflowed("at_least + 16", "a Long", "+", (int64_t)spite_temp_194, (int64_t)spite_temp_195, spite_site_27()); spite_temp_196; });
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
void Spite_Argument_Argument(Spite_Argument* self, SpiteString starting_name_, Spite_Class* starting_class_) {
SpiteString spite_temp_197 = SpiteString___retain(starting_name_);
SpiteString___release(self->_name_);
self->_name_ = spite_temp_197;
Spite_Class* spite_temp_198 = Spite_Class___retain(starting_class_);
Spite_Class___release(self->_class_);
self->_class_ = spite_temp_198;
Spite_Class___release(starting_class_);
SpiteString___release(starting_name_);
}
void Spite_Class_Class(Spite_Class* self, SpiteString starting_name_) {
SpiteString spite_temp_199 = SpiteString___retain(starting_name_);
SpiteString___release(self->_name_);
self->_name_ = spite_temp_199;
SpiteString___release(starting_name_);
}
void Spite_Function_Function(Spite_Function* self, SpiteString starting_name_, Spite_Class* starting_returns_) {
SpiteString spite_temp_200 = SpiteString___retain(starting_name_);
SpiteString___release(self->_name_);
self->_name_ = spite_temp_200;
Spite_Class* spite_temp_201 = Spite_Class___retain(starting_returns_);
Spite_Class___release(self->_returns_);
self->_returns_ = spite_temp_201;
Spite_Class___release(starting_returns_);
SpiteString___release(starting_name_);
}
void Naive_Naive(Naive* self) {
Naive_make_orbits(self, 100000);
Benchmark__Nothing* benchmark_ = Benchmark__Nothing___make(spite_function_value_Naive_advance_ticks(self));
int32_t turns_ = List_Orbit_sum_turns(self->orbits_);
List_Console_Printable spite_framed_1; Console_Printable spite_framed_1_items[2]; int32_t spite_framed_1_count = 0;
Console_print(self->console_, ({ spite_framed_1_items[0] = spite_tagged_object(0, ((void*)&spite_lit_11_box)); spite_framed_1_items[1] = spite_tagged_SpiteInteger(turns_); spite_framed_1_count = 2; List_Console_Printable___framed(&spite_framed_1, (int64_t)(intptr_t)spite_framed_1_items, 2); }));
for (int32_t spite_index = 0; spite_index < spite_framed_1_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_1_items[spite_index]); }
int64_t microseconds_ = Duration_total((benchmark_)->duration_, Duration_Unit_microseconds);
List_Console_Printable spite_framed_2; Console_Printable spite_framed_2_items[1]; int32_t spite_framed_2_count = 0;
Console_error(self->console_, ({ spite_framed_2_items[0] = spite_tagged_object(0, spite_box_SpiteString(({ char spite_temp_202_digits[24]; SpiteString spite_temp_202 = SPITE_STATIC_STRING(spite_temp_202_digits, spite_long_digits(spite_temp_202_digits, (int64_t)(microseconds_))); SpiteString spite_temp_203[] = {spite_lit_12, spite_temp_202}; SpiteString spite_temp_204 = spite_string_join(2, spite_temp_203); spite_temp_204; }))); spite_framed_2_count = 1; List_Console_Printable___framed(&spite_framed_2, (int64_t)(intptr_t)spite_framed_2_items, 1); }));
for (int32_t spite_index = 0; spite_index < spite_framed_2_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_2_items[spite_index]); }
Benchmark__Nothing___release(benchmark_);
}
void Naive_make_orbits(Naive* self, int32_t count_) {
int32_t index_ = 0;
while (((index_ < count_))) {
Orbit* orbit_ = Orbit___make(index_);
List_Orbit_append(self->orbits_, Orbit___retain(orbit_));
index_ = (index_ + 1);
Orbit___release(orbit_);
}
}
void Naive_advance_ticks(Naive* self) {
int32_t tick_ = 0;
while (((tick_ < 10))) {

{

if (({ List_Orbit* spite_band_list = self->orbits_; int32_t spite_band_n = spite_band_list->item_count_; bool spite_band_ok = spite_band_n >= 91; for (int32_t spite_band_i = 0; spite_band_ok && spite_band_i < spite_band_n; spite_band_i++) spite_band_ok = ((SpiteHeader*)(((void**)(intptr_t)spite_band_list->items_)[spite_band_i]))->ref_count == 1; spite_band_ok; })) {
List_Orbit_spite_band_advance(self->orbits_);
} else {

List_Orbit_each_advance(self->orbits_);

}}

tick_ = (tick_ + 1);
}
}
void Orbit_Orbit(Orbit* self, int32_t seed_) {
self->speed_ = SpiteInteger_to_float(((seed_ % 17) + 1));
}
void Orbit_advance(Orbit* self) {
int32_t step_ = 0;
while (((step_ < 1000))) {
self->angle_ = (self->angle_ + (self->speed_ * 0.001f));
if (((self->angle_ > 6.28318f))) {
self->angle_ = (self->angle_ - 6.28318f);
self->turns_ = ({ int32_t spite_temp_205 = self->turns_; int32_t spite_temp_206 = 1; int32_t spite_temp_207; if (__builtin_expect(__builtin_add_overflow(spite_temp_205, spite_temp_206, &spite_temp_207), 0)) spite_overflowed("turns + 1", "an Integer", "+", (int64_t)spite_temp_205, (int64_t)spite_temp_206, spite_site_28()); spite_temp_207; });
}
step_ = (step_ + 1);
}
}
void List_String_clear(List_String* self) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
TypedMemory__String_release_value(self->values_, self->items_, index_);
index_ = (index_ + 1);
}
self->item_count_ = 0;
}
void List_String_drop(List_String* self) {
List_String_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_208 = SPITE_ALLOCATOR_List_String(self, spite_singleton_Memory_Heap); int64_t spite_temp_209 = self->items_; if (((SpiteHeader*)(spite_temp_208))->class_id == 95) { Memory_Arena_free(((Memory_Arena*)spite_temp_208), spite_temp_209); } else if (((SpiteHeader*)(spite_temp_208))->class_id == 96) { Memory_Heap_free(((Memory_Heap*)spite_temp_208), spite_temp_209); } });
}
}
void TypedMemory__String_release_value(TypedMemory__String* self, int64_t address_, int32_t index_) {
SpiteString___release(((SpiteString*)(intptr_t)address_)[index_]);
}
int32_t List_ThreadPoolJob_count(List_ThreadPoolJob* self) {
int32_t spite_temp_210 = self->item_count_;
return spite_temp_210;
}
bool List_ThreadPoolJob_is_empty(List_ThreadPoolJob* self) {
bool spite_temp_211 = (self->item_count_ == 0);
return spite_temp_211;
}
ThreadPoolJob* List_ThreadPoolJob_get_at(List_ThreadPoolJob* self, int32_t index_) {
if ((((index_ >= 0))) && (((index_ < self->item_count_)))) {
ThreadPoolJob* spite_temp_212 = TypedMemory__ThreadPoolJob_read_value(self->values_, self->items_, index_);
return spite_temp_212;
}
ThreadPoolJob* spite_temp_213 = 0;
return spite_temp_213;
}
void List_ThreadPoolJob_remove_at(List_ThreadPoolJob* self, int32_t index_) {
if (!(((index_ >= 0)))) {
spite_failed_4(index_, self);
}
if (!(((index_ < self->item_count_)))) {
spite_failed_5(index_, self);
}
TypedMemory__ThreadPoolJob_release_value(self->values_, self->items_, index_);
List_ThreadPoolJob_move_items(self, (index_ + 1), index_, ({ int32_t spite_temp_214 = ({ int32_t spite_temp_215 = self->item_count_; int32_t spite_temp_216 = index_; int32_t spite_temp_217; if (__builtin_expect(__builtin_sub_overflow(spite_temp_215, spite_temp_216, &spite_temp_217), 0)) spite_overflowed("item_count - index", "an Integer", "-", (int64_t)spite_temp_215, (int64_t)spite_temp_216, spite_site_32()); spite_temp_217; }); int32_t spite_temp_218 = 1; int32_t spite_temp_219; if (__builtin_expect(__builtin_sub_overflow(spite_temp_214, spite_temp_218, &spite_temp_219), 0)) spite_overflowed("item_count - index - 1", "an Integer", "-", (int64_t)spite_temp_214, (int64_t)spite_temp_218, spite_site_32()); spite_temp_219; }));
self->item_count_ = ({ int32_t spite_temp_220 = self->item_count_; int32_t spite_temp_221 = 1; int32_t spite_temp_222; if (__builtin_expect(__builtin_sub_overflow(spite_temp_220, spite_temp_221, &spite_temp_222), 0)) spite_overflowed("item_count - 1", "an Integer", "-", (int64_t)spite_temp_220, (int64_t)spite_temp_221, spite_site_33()); spite_temp_222; });
}
static SPITE_CRASH_REPORT void spite_failed_4(int32_t index_, List_ThreadPoolJob* self) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_30(), stderr);
fputs("\tindex=", stderr);
{ SpiteString spite_temp_223 = SpiteInteger_to_string(index_); fwrite(spite_string_bytes(&spite_temp_223), 1, (size_t)spite_string_length(spite_temp_223), stderr); SpiteString___release(spite_temp_223); }
fputs("\titems=", stderr);
{ SpiteString spite_temp_224 = SpiteMemory_Address_to_string(self->items_); spite_crash_text(spite_string_bytes(&spite_temp_224), spite_string_length(spite_temp_224)); SpiteString___release(spite_temp_224); }
fputs("\titem_count=", stderr);
{ SpiteString spite_temp_225 = SpiteInteger_to_string(self->item_count_); spite_crash_text(spite_string_bytes(&spite_temp_225), spite_string_length(spite_temp_225)); SpiteString___release(spite_temp_225); }
fputs("\tcapacity=", stderr);
{ SpiteString spite_temp_226 = SpiteInteger_to_string(self->capacity_); spite_crash_text(spite_string_bytes(&spite_temp_226), spite_string_length(spite_temp_226)); SpiteString___release(spite_temp_226); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
static SPITE_CRASH_REPORT void spite_failed_5(int32_t index_, List_ThreadPoolJob* self) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_31(), stderr);
fputs("\tindex=", stderr);
{ SpiteString spite_temp_227 = SpiteInteger_to_string(index_); fwrite(spite_string_bytes(&spite_temp_227), 1, (size_t)spite_string_length(spite_temp_227), stderr); SpiteString___release(spite_temp_227); }
fputs("\titem_count=", stderr);
{ SpiteString spite_temp_228 = SpiteInteger_to_string(self->item_count_); fwrite(spite_string_bytes(&spite_temp_228), 1, (size_t)spite_string_length(spite_temp_228), stderr); SpiteString___release(spite_temp_228); }
fputs("\titems=", stderr);
{ SpiteString spite_temp_229 = SpiteMemory_Address_to_string(self->items_); spite_crash_text(spite_string_bytes(&spite_temp_229), spite_string_length(spite_temp_229)); SpiteString___release(spite_temp_229); }
fputs("\tcapacity=", stderr);
{ SpiteString spite_temp_230 = SpiteInteger_to_string(self->capacity_); spite_crash_text(spite_string_bytes(&spite_temp_230), spite_string_length(spite_temp_230)); SpiteString___release(spite_temp_230); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
void List_ThreadPoolJob_drop(List_ThreadPoolJob* self) {
List_ThreadPoolJob_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_231 = SPITE_ALLOCATOR_List_ThreadPoolJob(self, spite_singleton_Memory_Heap); int64_t spite_temp_232 = self->items_; if (((SpiteHeader*)(spite_temp_231))->class_id == 95) { Memory_Arena_free(((Memory_Arena*)spite_temp_231), spite_temp_232); } else if (((SpiteHeader*)(spite_temp_231))->class_id == 96) { Memory_Heap_free(((Memory_Heap*)spite_temp_231), spite_temp_232); } });
}
}
void List_ThreadPoolJob_move_items(List_ThreadPoolJob* self, int32_t from_, int32_t to_, int32_t moved_count_) {
int64_t bytes_ = TypedMemory__ThreadPoolJob_value_bytes(self->values_);
int64_t moved_items_ = (self->items_ + ((int64_t)(({ int64_t spite_temp_233 = bytes_; int64_t spite_temp_234 = SpiteInteger_to_long(from_); int64_t spite_temp_235; if (__builtin_expect(__builtin_mul_overflow(spite_temp_233, spite_temp_234, &spite_temp_235), 0)) spite_overflowed("bytes * from", "a Long", "*", (int64_t)spite_temp_233, (int64_t)spite_temp_234, spite_site_36()); spite_temp_235; }))));
SpiteMemory_Address_copy_to(moved_items_, (self->items_ + ((int64_t)(({ int64_t spite_temp_236 = bytes_; int64_t spite_temp_237 = SpiteInteger_to_long(to_); int64_t spite_temp_238; if (__builtin_expect(__builtin_mul_overflow(spite_temp_236, spite_temp_237, &spite_temp_238), 0)) spite_overflowed("bytes * to", "a Long", "*", (int64_t)spite_temp_236, (int64_t)spite_temp_237, spite_site_37()); spite_temp_238; })))), ({ int64_t spite_temp_239 = bytes_; int64_t spite_temp_240 = SpiteInteger_to_long(moved_count_); int64_t spite_temp_241; if (__builtin_expect(__builtin_mul_overflow(spite_temp_239, spite_temp_240, &spite_temp_241), 0)) spite_overflowed("bytes * moved_count", "a Long", "*", (int64_t)spite_temp_239, (int64_t)spite_temp_240, spite_site_37()); spite_temp_241; }));
}
ThreadPoolJob* TypedMemory__ThreadPoolJob_read_value(TypedMemory__ThreadPoolJob* self, int64_t address_, int32_t index_) {
return ThreadPoolJob___retain(((ThreadPoolJob**)(intptr_t)address_)[index_]);
}
void TypedMemory__ThreadPoolJob_release_value(TypedMemory__ThreadPoolJob* self, int64_t address_, int32_t index_) {
ThreadPoolJob___release(((ThreadPoolJob**)(intptr_t)address_)[index_]);
}
int64_t TypedMemory__ThreadPoolJob_value_bytes(TypedMemory__ThreadPoolJob* self) {
return (int64_t)sizeof(ThreadPoolJob*);
}
void List_Spite_AttributeDeclaration_drop(List_Spite_AttributeDeclaration* self) {
List_Spite_AttributeDeclaration_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_242 = SPITE_ALLOCATOR_List_Spite_AttributeDeclaration(self, spite_singleton_Memory_Heap); int64_t spite_temp_243 = self->items_; if (((SpiteHeader*)(spite_temp_242))->class_id == 95) { Memory_Arena_free(((Memory_Arena*)spite_temp_242), spite_temp_243); } else if (((SpiteHeader*)(spite_temp_242))->class_id == 96) { Memory_Heap_free(((Memory_Heap*)spite_temp_242), spite_temp_243); } });
}
}
void List_Spite_Function_drop(List_Spite_Function* self) {
List_Spite_Function_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_244 = SPITE_ALLOCATOR_List_Spite_Function(self, spite_singleton_Memory_Heap); int64_t spite_temp_245 = self->items_; if (((SpiteHeader*)(spite_temp_244))->class_id == 95) { Memory_Arena_free(((Memory_Arena*)spite_temp_244), spite_temp_245); } else if (((SpiteHeader*)(spite_temp_244))->class_id == 96) { Memory_Heap_free(((Memory_Heap*)spite_temp_244), spite_temp_245); } });
}
}
void List_Spite_Argument_drop(List_Spite_Argument* self) {
List_Spite_Argument_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_246 = SPITE_ALLOCATOR_List_Spite_Argument(self, spite_singleton_Memory_Heap); int64_t spite_temp_247 = self->items_; if (((SpiteHeader*)(spite_temp_246))->class_id == 95) { Memory_Arena_free(((Memory_Arena*)spite_temp_246), spite_temp_247); } else if (((SpiteHeader*)(spite_temp_246))->class_id == 96) { Memory_Heap_free(((Memory_Heap*)spite_temp_246), spite_temp_247); } });
}
}
void List_Spite_Class_drop(List_Spite_Class* self) {
List_Spite_Class_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_248 = SPITE_ALLOCATOR_List_Spite_Class(self, spite_singleton_Memory_Heap); int64_t spite_temp_249 = self->items_; if (((SpiteHeader*)(spite_temp_248))->class_id == 95) { Memory_Arena_free(((Memory_Arena*)spite_temp_248), spite_temp_249); } else if (((SpiteHeader*)(spite_temp_248))->class_id == 96) { Memory_Heap_free(((Memory_Heap*)spite_temp_248), spite_temp_249); } });
}
}
void List_Spite_Namespace_drop(List_Spite_Namespace* self) {
List_Spite_Namespace_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_250 = SPITE_ALLOCATOR_List_Spite_Namespace(self, spite_singleton_Memory_Heap); int64_t spite_temp_251 = self->items_; if (((SpiteHeader*)(spite_temp_250))->class_id == 95) { Memory_Arena_free(((Memory_Arena*)spite_temp_250), spite_temp_251); } else if (((SpiteHeader*)(spite_temp_250))->class_id == 96) { Memory_Heap_free(((Memory_Heap*)spite_temp_250), spite_temp_251); } });
}
}
void List_Orbit_append(List_Orbit* self, Orbit* value_) {
List_Orbit_make_room(self);
TypedMemory__Orbit_write_value(self->values_, self->items_, self->item_count_, Orbit___retain(value_));
self->item_count_ = ({ int32_t spite_temp_252 = self->item_count_; int32_t spite_temp_253 = 1; int32_t spite_temp_254; if (__builtin_expect(__builtin_add_overflow(spite_temp_252, spite_temp_253, &spite_temp_254), 0)) spite_overflowed("item_count + 1", "an Integer", "+", (int64_t)spite_temp_252, (int64_t)spite_temp_253, spite_site_29()); spite_temp_254; });
Orbit___release(value_);
}
void List_Orbit_drop(List_Orbit* self) {
List_Orbit_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_255 = SPITE_ALLOCATOR_List_Orbit(self, spite_singleton_Memory_Heap); int64_t spite_temp_256 = self->items_; if (((SpiteHeader*)(spite_temp_255))->class_id == 95) { Memory_Arena_free(((Memory_Arena*)spite_temp_255), spite_temp_256); } else if (((SpiteHeader*)(spite_temp_255))->class_id == 96) { Memory_Heap_free(((Memory_Heap*)spite_temp_255), spite_temp_256); } });
}
}
void List_Orbit_make_room(List_Orbit* self) {
if (((self->item_count_ == self->capacity_))) {
List_Orbit__grow(self);
}
}
void List_Orbit__grow(List_Orbit* self) {
int32_t grown_ = ({ int32_t spite_temp_257 = self->capacity_; int32_t spite_temp_258 = 2; int32_t spite_temp_259; if (__builtin_expect(__builtin_mul_overflow(spite_temp_257, spite_temp_258, &spite_temp_259), 0)) spite_overflowed("capacity * 2", "an Integer", "*", (int64_t)spite_temp_257, (int64_t)spite_temp_258, spite_site_34()); spite_temp_259; });
if (((self->capacity_ == 0))) {
grown_ = 4;
}
int64_t bytes_ = TypedMemory__Orbit_value_bytes(self->values_);
self->items_ = List_Orbit__resized(self, ({ int64_t spite_temp_260 = bytes_; int64_t spite_temp_261 = SpiteInteger_to_long(self->capacity_); int64_t spite_temp_262; if (__builtin_expect(__builtin_mul_overflow(spite_temp_260, spite_temp_261, &spite_temp_262), 0)) spite_overflowed("bytes * capacity", "a Long", "*", (int64_t)spite_temp_260, (int64_t)spite_temp_261, spite_site_35()); spite_temp_262; }), ({ int64_t spite_temp_263 = bytes_; int64_t spite_temp_264 = SpiteInteger_to_long(grown_); int64_t spite_temp_265; if (__builtin_expect(__builtin_mul_overflow(spite_temp_263, spite_temp_264, &spite_temp_265), 0)) spite_overflowed("bytes * grown", "a Long", "*", (int64_t)spite_temp_263, (int64_t)spite_temp_264, spite_site_35()); spite_temp_265; }));
self->capacity_ = grown_;
}
int64_t List_Orbit__resized(List_Orbit* self, int64_t old_bytes_, int64_t new_bytes_) {
if ((({ Spite_Allocator spite_temp_266 = SPITE_ALLOCATOR_List_Orbit(self, spite_singleton_Memory_Heap); bool spite_temp_267 = (((SpiteHeader*)(spite_temp_266))->class_id == 96); spite_temp_267; }))) {
int64_t spite_temp_268 = Memory_Heap_resize(self->heap_, self->items_, new_bytes_);
return spite_temp_268;
}
int64_t moved_ = ({ Spite_Allocator spite_temp_269 = SPITE_ALLOCATOR_List_Orbit(self, spite_singleton_Memory_Heap); int64_t spite_temp_270 = new_bytes_; int64_t spite_temp_271 = 0; if (((SpiteHeader*)(spite_temp_269))->class_id == 95) { spite_temp_271 = Memory_Arena_allocate(((Memory_Arena*)spite_temp_269), spite_temp_270); } else if (((SpiteHeader*)(spite_temp_269))->class_id == 96) { spite_temp_271 = Memory_Heap_allocate(((Memory_Heap*)spite_temp_269), spite_temp_270); } spite_temp_271; });
if (((self->items_ != ((int64_t)(0))))) {
SpiteMemory_Address_copy_to(self->items_, moved_, old_bytes_);
({ Spite_Allocator spite_temp_272 = SPITE_ALLOCATOR_List_Orbit(self, spite_singleton_Memory_Heap); int64_t spite_temp_273 = self->items_; if (((SpiteHeader*)(spite_temp_272))->class_id == 95) { Memory_Arena_free(((Memory_Arena*)spite_temp_272), spite_temp_273); } else if (((SpiteHeader*)(spite_temp_272))->class_id == 96) { Memory_Heap_free(((Memory_Heap*)spite_temp_272), spite_temp_273); } });
}
int64_t spite_temp_274 = moved_;
return spite_temp_274;
}
int32_t List_Orbit_sum_turns(List_Orbit* self) {
int32_t total_ = 0;
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
Orbit* item_ = ((Orbit**)(intptr_t)self->items_)[index_];
total_ = ({ int32_t spite_temp_275 = total_; int32_t spite_temp_276 = (item_)->turns_; int32_t spite_temp_277; if (__builtin_expect(__builtin_add_overflow(spite_temp_275, spite_temp_276, &spite_temp_277), 0)) spite_overflowed("total + item.attributes[member]", "an Integer", "+", (int64_t)spite_temp_275, (int64_t)spite_temp_276, spite_site_38()); spite_temp_277; });
index_ = (index_ + 1);
}
int32_t spite_temp_278 = total_;
return spite_temp_278;
}
void List_Orbit_spite_band_advance(List_Orbit* self) {
ThreadPool* pool_ = spite_singleton_ThreadPool();
ThreadPool_run_bands(pool_, spite_function_value_List_Orbit_spite_band_advance_piece(self), self->item_count_);
ThreadPool___release(pool_);
}
void List_Orbit_each_advance(List_Orbit* self) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
Orbit* item_ = ((Orbit**)(intptr_t)self->items_)[index_];
Orbit_advance(item_);
index_ = (index_ + 1);
}
}
void TypedMemory__Orbit_write_value(TypedMemory__Orbit* self, int64_t address_, int32_t index_, Orbit* value_) {
((Orbit**)(intptr_t)address_)[index_] = value_;
}
int64_t TypedMemory__Orbit_value_bytes(TypedMemory__Orbit* self) {
return (int64_t)sizeof(Orbit*);
}
Console_Printable List_Console_Printable_get_at(List_Console_Printable* self, int32_t index_) {
if ((((index_ >= 0))) && (((index_ < self->item_count_)))) {
Console_Printable spite_temp_279 = TypedMemory__Console_Printable_read_value(self->values_, self->items_, index_);
return spite_temp_279;
}
Console_Printable spite_temp_280 = SPITE_TAGGED_NULL;
return spite_temp_280;
}
void List_Console_Printable_drop(List_Console_Printable* self) {
List_Console_Printable_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_281 = SPITE_ALLOCATOR_List_Console_Printable(self, spite_singleton_Memory_Heap); int64_t spite_temp_282 = self->items_; if (((SpiteHeader*)(spite_temp_281))->class_id == 95) { Memory_Arena_free(((Memory_Arena*)spite_temp_281), spite_temp_282); } else if (((SpiteHeader*)(spite_temp_281))->class_id == 96) { Memory_Heap_free(((Memory_Heap*)spite_temp_281), spite_temp_282); } });
}
}
Console_Printable TypedMemory__Console_Printable_read_value(TypedMemory__Console_Printable* self, int64_t address_, int32_t index_) {
return Console_Printable___retain(((Console_Printable*)(intptr_t)address_)[index_]);
}
void List_Symbol_drop(List_Symbol* self) {
spite_folded_List_Symbol_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_283 = SPITE_ALLOCATOR_List_Symbol(self, spite_singleton_Memory_Heap); int64_t spite_temp_284 = self->items_; if (((SpiteHeader*)(spite_temp_283))->class_id == 95) { Memory_Arena_free(((Memory_Arena*)spite_temp_283), spite_temp_284); } else if (((SpiteHeader*)(spite_temp_283))->class_id == 96) { Memory_Heap_free(((Memory_Heap*)spite_temp_283), spite_temp_284); } });
}
}
void Benchmark__Nothing_Benchmark(Benchmark__Nothing* self, Spite_Function* work_) {
int64_t started_ = Clock_elapsed_nanoseconds(self->_clock_);
Nothing* spite_temp_285 = ({ ({ Spite_Function* spite_temp_286 = work_; ((void (*)(void*))spite_temp_286->spite_typed_call)(spite_temp_286->spite_owner); }); Nothing___default(); });
Nothing___release(self->answer_);
self->answer_ = spite_temp_285;
int64_t finished_ = Clock_elapsed_nanoseconds(self->_clock_);
Duration* spite_temp_287 = Duration___make(({ int64_t spite_temp_288 = finished_; int64_t spite_temp_289 = started_; int64_t spite_temp_290; if (__builtin_expect(__builtin_sub_overflow(spite_temp_288, spite_temp_289, &spite_temp_290), 0)) spite_overflowed("finished - started", "a Long", "-", (int64_t)spite_temp_288, (int64_t)spite_temp_289, spite_site_39()); spite_temp_290; }), Duration_Unit_nanoseconds);
Duration___release(self->duration_);
self->duration_ = spite_temp_287;
Spite_Function___release(work_);
}
void ThreadPool_run_bands(ThreadPool* self, Spite_Function* piece_, int32_t count_) {
int32_t bands_ = ({ int32_t spite_temp_291 = ThreadPool_size(self); int32_t spite_temp_292 = 1; int32_t spite_temp_293; if (__builtin_expect(__builtin_add_overflow(spite_temp_291, spite_temp_292, &spite_temp_293), 0)) spite_overflowed("size() + 1", "an Integer", "+", (int64_t)spite_temp_291, (int64_t)spite_temp_292, spite_site_40()); spite_temp_293; });
if (((bands_ > count_))) {
bands_ = count_;
}
if (((bands_ < 2))) {
({ Spite_Function* spite_temp_294 = piece_; ((void (*)(void*, int32_t, int32_t))spite_temp_294->spite_typed_call)(spite_temp_294->spite_owner, 0, count_); });
Spite_Function___release(piece_);
return;
}
int64_t block_ = Memory_Heap_allocate(self->heap_, SpiteInteger_to_long(({ int32_t spite_temp_295 = bands_; int32_t spite_temp_296 = 8; int32_t spite_temp_297; if (__builtin_expect(__builtin_mul_overflow(spite_temp_295, spite_temp_296, &spite_temp_297), 0)) spite_overflowed("bands * 8", "an Integer", "*", (int64_t)spite_temp_295, (int64_t)spite_temp_296, spite_site_41()); spite_temp_297; })));
int32_t base_ = ({ int32_t spite_temp_298 = count_; int32_t spite_temp_299 = bands_; if (spite_temp_299 == 0) spite_divided_by_zero("count / bands", spite_site_42()); int32_t spite_temp_300 = 0; if (__builtin_expect(spite_temp_299 == -1 && __builtin_sub_overflow((int32_t)0, spite_temp_298, &spite_temp_300), 0)) spite_overflowed("count / bands", "an Integer", "/", (int64_t)spite_temp_298, (int64_t)spite_temp_299, spite_site_42()); (int32_t)(spite_temp_299 == -1 ? spite_temp_300 : spite_temp_298 / spite_temp_299); });
int32_t extra_ = ({ int32_t spite_temp_301 = count_; int32_t spite_temp_302 = bands_; if (spite_temp_302 == 0) spite_divided_by_zero("count % bands", spite_site_43()); (int32_t)(spite_temp_302 == -1 ? (int32_t)0 : spite_temp_301 % spite_temp_302); });
int32_t index_ = 1;
while (((index_ < bands_))) {
int32_t first_ = ThreadPool_piece_start(self, index_, base_, extra_);
int32_t end_ = ThreadPool_piece_start(self, (index_ + 1), base_, extra_);
ThreadPool_submit(self, Spite_Function___retain(piece_), first_, end_, (block_ + ((int64_t)(({ int32_t spite_temp_303 = index_; int32_t spite_temp_304 = 8; int32_t spite_temp_305; if (__builtin_expect(__builtin_mul_overflow(spite_temp_303, spite_temp_304, &spite_temp_305), 0)) spite_overflowed("index * 8", "an Integer", "*", (int64_t)spite_temp_303, (int64_t)spite_temp_304, spite_site_44()); spite_temp_305; })))));
index_ = (index_ + 1);
}
int32_t first_end_ = ThreadPool_piece_start(self, 1, base_, extra_);
({ Spite_Function* spite_temp_306 = piece_; ((void (*)(void*, int32_t, int32_t))spite_temp_306->spite_typed_call)(spite_temp_306->spite_owner, 0, first_end_); });
index_ = 1;
while (((index_ < bands_))) {
ThreadPool_join(self, (block_ + ((int64_t)(({ int32_t spite_temp_307 = index_; int32_t spite_temp_308 = 8; int32_t spite_temp_309; if (__builtin_expect(__builtin_mul_overflow(spite_temp_307, spite_temp_308, &spite_temp_309), 0)) spite_overflowed("index * 8", "an Integer", "*", (int64_t)spite_temp_307, (int64_t)spite_temp_308, spite_site_45()); spite_temp_309; })))));
index_ = (index_ + 1);
}
Memory_Heap_free(self->heap_, block_);
Spite_Function___release(piece_);
}
void List_ThreadPoolJob_clear(List_ThreadPoolJob* self) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
TypedMemory__ThreadPoolJob_release_value(self->values_, self->items_, index_);
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
void List_Spite_Argument_append(List_Spite_Argument* self, Spite_Argument* value_) {
List_Spite_Argument_make_room(self);
TypedMemory__Spite_Argument_write_value(self->values_, self->items_, self->item_count_, Spite_Argument___retain(value_));
self->item_count_ = ({ int32_t spite_temp_310 = self->item_count_; int32_t spite_temp_311 = 1; int32_t spite_temp_312; if (__builtin_expect(__builtin_add_overflow(spite_temp_310, spite_temp_311, &spite_temp_312), 0)) spite_overflowed("item_count + 1", "an Integer", "+", (int64_t)spite_temp_310, (int64_t)spite_temp_311, spite_site_29()); spite_temp_312; });
Spite_Argument___release(value_);
}
void List_Spite_Argument_clear(List_Spite_Argument* self) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
TypedMemory__Spite_Argument_release_value(self->values_, self->items_, index_);
index_ = (index_ + 1);
}
self->item_count_ = 0;
}
void List_Spite_Argument_make_room(List_Spite_Argument* self) {
if (((self->item_count_ == self->capacity_))) {
List_Spite_Argument__grow(self);
}
}
void List_Spite_Argument__grow(List_Spite_Argument* self) {
int32_t grown_ = ({ int32_t spite_temp_313 = self->capacity_; int32_t spite_temp_314 = 2; int32_t spite_temp_315; if (__builtin_expect(__builtin_mul_overflow(spite_temp_313, spite_temp_314, &spite_temp_315), 0)) spite_overflowed("capacity * 2", "an Integer", "*", (int64_t)spite_temp_313, (int64_t)spite_temp_314, spite_site_34()); spite_temp_315; });
if (((self->capacity_ == 0))) {
grown_ = 4;
}
int64_t bytes_ = TypedMemory__Spite_Argument_value_bytes(self->values_);
self->items_ = List_Spite_Argument__resized(self, ({ int64_t spite_temp_316 = bytes_; int64_t spite_temp_317 = SpiteInteger_to_long(self->capacity_); int64_t spite_temp_318; if (__builtin_expect(__builtin_mul_overflow(spite_temp_316, spite_temp_317, &spite_temp_318), 0)) spite_overflowed("bytes * capacity", "a Long", "*", (int64_t)spite_temp_316, (int64_t)spite_temp_317, spite_site_35()); spite_temp_318; }), ({ int64_t spite_temp_319 = bytes_; int64_t spite_temp_320 = SpiteInteger_to_long(grown_); int64_t spite_temp_321; if (__builtin_expect(__builtin_mul_overflow(spite_temp_319, spite_temp_320, &spite_temp_321), 0)) spite_overflowed("bytes * grown", "a Long", "*", (int64_t)spite_temp_319, (int64_t)spite_temp_320, spite_site_35()); spite_temp_321; }));
self->capacity_ = grown_;
}
int64_t List_Spite_Argument__resized(List_Spite_Argument* self, int64_t old_bytes_, int64_t new_bytes_) {
if ((({ Spite_Allocator spite_temp_322 = SPITE_ALLOCATOR_List_Spite_Argument(self, spite_singleton_Memory_Heap); bool spite_temp_323 = (((SpiteHeader*)(spite_temp_322))->class_id == 96); spite_temp_323; }))) {
int64_t spite_temp_324 = Memory_Heap_resize(self->heap_, self->items_, new_bytes_);
return spite_temp_324;
}
int64_t moved_ = ({ Spite_Allocator spite_temp_325 = SPITE_ALLOCATOR_List_Spite_Argument(self, spite_singleton_Memory_Heap); int64_t spite_temp_326 = new_bytes_; int64_t spite_temp_327 = 0; if (((SpiteHeader*)(spite_temp_325))->class_id == 95) { spite_temp_327 = Memory_Arena_allocate(((Memory_Arena*)spite_temp_325), spite_temp_326); } else if (((SpiteHeader*)(spite_temp_325))->class_id == 96) { spite_temp_327 = Memory_Heap_allocate(((Memory_Heap*)spite_temp_325), spite_temp_326); } spite_temp_327; });
if (((self->items_ != ((int64_t)(0))))) {
SpiteMemory_Address_copy_to(self->items_, moved_, old_bytes_);
({ Spite_Allocator spite_temp_328 = SPITE_ALLOCATOR_List_Spite_Argument(self, spite_singleton_Memory_Heap); int64_t spite_temp_329 = self->items_; if (((SpiteHeader*)(spite_temp_328))->class_id == 95) { Memory_Arena_free(((Memory_Arena*)spite_temp_328), spite_temp_329); } else if (((SpiteHeader*)(spite_temp_328))->class_id == 96) { Memory_Heap_free(((Memory_Heap*)spite_temp_328), spite_temp_329); } });
}
int64_t spite_temp_330 = moved_;
return spite_temp_330;
}
void TypedMemory__Spite_Argument_write_value(TypedMemory__Spite_Argument* self, int64_t address_, int32_t index_, Spite_Argument* value_) {
((Spite_Argument**)(intptr_t)address_)[index_] = value_;
}
void TypedMemory__Spite_Argument_release_value(TypedMemory__Spite_Argument* self, int64_t address_, int32_t index_) {
Spite_Argument___release(((Spite_Argument**)(intptr_t)address_)[index_]);
}
int64_t TypedMemory__Spite_Argument_value_bytes(TypedMemory__Spite_Argument* self) {
return (int64_t)sizeof(Spite_Argument*);
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
void List_Orbit_clear(List_Orbit* self) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
TypedMemory__Orbit_release_value(self->values_, self->items_, index_);
index_ = (index_ + 1);
}
self->item_count_ = 0;
}
void List_Orbit_spite_band_advance_piece(List_Orbit* self, int32_t first_, int32_t end_) {
int32_t index_ = first_;
while (((index_ < end_))) {
Orbit* item_ = ((Orbit**)(intptr_t)self->items_)[index_];
Orbit_advance(item_);
index_ = (index_ + 1);
}
}
void TypedMemory__Orbit_release_value(TypedMemory__Orbit* self, int64_t address_, int32_t index_) {
Orbit___release(((Orbit**)(intptr_t)address_)[index_]);
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
void ThreadPool_submit(ThreadPool* self, Spite_Function* job_, int32_t first_, int32_t end_, int64_t state_) {
ThreadPool_start(self);
SpiteMemory_Address_write_long_atomically(state_, SpiteInteger_to_long(0), SpiteInteger_to_long(0));
ThreadPoolJob* queued_ = ThreadPoolJob___make();
Spite_Function* spite_temp_331 = Spite_Function___retain(job_);
Spite_Function___release((queued_)->work_);
(queued_)->work_ = spite_temp_331;
(queued_)->first_ = first_;
(queued_)->end_ = end_;
(queued_)->state_ = state_;
ThreadPool__task_begun(self);
ThreadPool_lock_queue(self);
List_ThreadPoolJob_append(self->jobs_, ThreadPoolJob___retain(queued_));
ThreadPool_signal_one(self, self->work_ready_);
ThreadPool_unlock_queue(self);
ThreadPoolJob___release(queued_);
Spite_Function___release(job_);
}
void List_ThreadPoolJob_append(List_ThreadPoolJob* self, ThreadPoolJob* value_) {
List_ThreadPoolJob_make_room(self);
TypedMemory__ThreadPoolJob_write_value(self->values_, self->items_, self->item_count_, ThreadPoolJob___retain(value_));
self->item_count_ = ({ int32_t spite_temp_332 = self->item_count_; int32_t spite_temp_333 = 1; int32_t spite_temp_334; if (__builtin_expect(__builtin_add_overflow(spite_temp_332, spite_temp_333, &spite_temp_334), 0)) spite_overflowed("item_count + 1", "an Integer", "+", (int64_t)spite_temp_332, (int64_t)spite_temp_333, spite_site_29()); spite_temp_334; });
ThreadPoolJob___release(value_);
}
void List_ThreadPoolJob_make_room(List_ThreadPoolJob* self) {
if (((self->item_count_ == self->capacity_))) {
List_ThreadPoolJob__grow(self);
}
}
void List_ThreadPoolJob__grow(List_ThreadPoolJob* self) {
int32_t grown_ = ({ int32_t spite_temp_335 = self->capacity_; int32_t spite_temp_336 = 2; int32_t spite_temp_337; if (__builtin_expect(__builtin_mul_overflow(spite_temp_335, spite_temp_336, &spite_temp_337), 0)) spite_overflowed("capacity * 2", "an Integer", "*", (int64_t)spite_temp_335, (int64_t)spite_temp_336, spite_site_34()); spite_temp_337; });
if (((self->capacity_ == 0))) {
grown_ = 4;
}
int64_t bytes_ = TypedMemory__ThreadPoolJob_value_bytes(self->values_);
self->items_ = List_ThreadPoolJob__resized(self, ({ int64_t spite_temp_338 = bytes_; int64_t spite_temp_339 = SpiteInteger_to_long(self->capacity_); int64_t spite_temp_340; if (__builtin_expect(__builtin_mul_overflow(spite_temp_338, spite_temp_339, &spite_temp_340), 0)) spite_overflowed("bytes * capacity", "a Long", "*", (int64_t)spite_temp_338, (int64_t)spite_temp_339, spite_site_35()); spite_temp_340; }), ({ int64_t spite_temp_341 = bytes_; int64_t spite_temp_342 = SpiteInteger_to_long(grown_); int64_t spite_temp_343; if (__builtin_expect(__builtin_mul_overflow(spite_temp_341, spite_temp_342, &spite_temp_343), 0)) spite_overflowed("bytes * grown", "a Long", "*", (int64_t)spite_temp_341, (int64_t)spite_temp_342, spite_site_35()); spite_temp_343; }));
self->capacity_ = grown_;
}
int64_t List_ThreadPoolJob__resized(List_ThreadPoolJob* self, int64_t old_bytes_, int64_t new_bytes_) {
if ((({ Spite_Allocator spite_temp_344 = SPITE_ALLOCATOR_List_ThreadPoolJob(self, spite_singleton_Memory_Heap); bool spite_temp_345 = (((SpiteHeader*)(spite_temp_344))->class_id == 96); spite_temp_345; }))) {
int64_t spite_temp_346 = Memory_Heap_resize(self->heap_, self->items_, new_bytes_);
return spite_temp_346;
}
int64_t moved_ = ({ Spite_Allocator spite_temp_347 = SPITE_ALLOCATOR_List_ThreadPoolJob(self, spite_singleton_Memory_Heap); int64_t spite_temp_348 = new_bytes_; int64_t spite_temp_349 = 0; if (((SpiteHeader*)(spite_temp_347))->class_id == 95) { spite_temp_349 = Memory_Arena_allocate(((Memory_Arena*)spite_temp_347), spite_temp_348); } else if (((SpiteHeader*)(spite_temp_347))->class_id == 96) { spite_temp_349 = Memory_Heap_allocate(((Memory_Heap*)spite_temp_347), spite_temp_348); } spite_temp_349; });
if (((self->items_ != ((int64_t)(0))))) {
SpiteMemory_Address_copy_to(self->items_, moved_, old_bytes_);
({ Spite_Allocator spite_temp_350 = SPITE_ALLOCATOR_List_ThreadPoolJob(self, spite_singleton_Memory_Heap); int64_t spite_temp_351 = self->items_; if (((SpiteHeader*)(spite_temp_350))->class_id == 95) { Memory_Arena_free(((Memory_Arena*)spite_temp_350), spite_temp_351); } else if (((SpiteHeader*)(spite_temp_350))->class_id == 96) { Memory_Heap_free(((Memory_Heap*)spite_temp_350), spite_temp_351); } });
}
int64_t spite_temp_352 = moved_;
return spite_temp_352;
}
void TypedMemory__ThreadPoolJob_write_value(TypedMemory__ThreadPoolJob* self, int64_t address_, int32_t index_, ThreadPoolJob* value_) {
((ThreadPoolJob**)(intptr_t)address_)[index_] = value_;
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
{(const void*)&Console_Printable___retain, "-\t-", "Console_Printable___retain", 0},
{(const void*)&ThreadPool___thread_entry, "-\t-", "ThreadPool___thread_entry", 0},
{(const void*)&spite_singleton_Build, "-\t-", "spite_singleton_Build", 0},
{(const void*)&spite_singleton_Console_teardown, "-\t-", "spite_singleton_Console_teardown", 0},
{(const void*)&spite_singleton_Console, "-\t-", "spite_singleton_Console", 0},
{(const void*)&spite_singleton_TimeText, "-\t-", "spite_singleton_TimeText", 0},
{(const void*)&spite_singleton_Clock_teardown, "-\t-", "spite_singleton_Clock_teardown", 0},
{(const void*)&spite_singleton_Clock, "-\t-", "spite_singleton_Clock", 0},
{(const void*)&spite_singleton_TypedMemory__ThreadPoolJob, "-\t-", "spite_singleton_TypedMemory__ThreadPoolJob", 0},
{(const void*)&spite_singleton_TypedMemory__Spite_AttributeDeclaration, "-\t-", "spite_singleton_TypedMemory__Spite_AttributeDeclaration", 0},
{(const void*)&spite_singleton_TypedMemory__Spite_Function, "-\t-", "spite_singleton_TypedMemory__Spite_Function", 0},
{(const void*)&spite_singleton_TypedMemory__Spite_Argument, "-\t-", "spite_singleton_TypedMemory__Spite_Argument", 0},
{(const void*)&spite_singleton_TypedMemory__Orbit, "-\t-", "spite_singleton_TypedMemory__Orbit", 0},
{(const void*)&Launcher___init, "-\t-", "Launcher___init", 0},
{(const void*)&Launcher___allocate, "-\t-", "Launcher___allocate", 0},
{(const void*)&Launcher___release, "-\t-", "Launcher___release", 0},
{(const void*)&Launcher___free, "-\t-", "Launcher___free", 0},
{(const void*)&spite_overflowed, "-\t-", "spite_overflowed", 0},
{(const void*)&spite_singleton_TypedMemory__Console_Printable, "-\t-", "spite_singleton_TypedMemory__Console_Printable", 0},
{(const void*)&List_Console_Printable___framed, "-\t-", "List_Console_Printable___framed", 0},
{(const void*)&spite_box_SpiteString, "-\t-", "spite_box_SpiteString", 0},
{(const void*)&spite_string_box_release, "-\t-", "spite_string_box_release", 0},
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
{(const void*)&Duration___init, "-\t-", "Duration___init", 0},
{(const void*)&Duration___allocate, "-\t-", "Duration___allocate", 0},
{(const void*)&Duration___make, "-\t-", "Duration___make", 0},
{(const void*)&Duration___release, "-\t-", "Duration___release", 0},
{(const void*)&Duration___free, "-\t-", "Duration___free", 0},
{(const void*)&DynamicLibrary___init, "-\t-", "DynamicLibrary___init", 0},
{(const void*)&DynamicLibrary___allocate, "-\t-", "DynamicLibrary___allocate", 0},
{(const void*)&DynamicLibrary___make, "-\t-", "DynamicLibrary___make", 0},
{(const void*)&DynamicLibrary___destroy, "-\t-", "DynamicLibrary___destroy", 0},
{(const void*)&DynamicLibrary___discard, "-\t-", "DynamicLibrary___discard", 0},
{(const void*)&Nothing___init, "-\t-", "Nothing___init", 0},
{(const void*)&Nothing___allocate, "-\t-", "Nothing___allocate", 0},
{(const void*)&Nothing___release, "-\t-", "Nothing___release", 0},
{(const void*)&Nothing___free, "-\t-", "Nothing___free", 0},
{(const void*)&spite_long_digits, "-\t-", "spite_long_digits", 0},
{(const void*)&spite_string_block, "-\t-", "spite_string_block", 0},
{(const void*)&spite_string_held, "-\t-", "spite_string_held", 0},
{(const void*)&SpiteString___retain, "-\t-", "SpiteString___retain", 0},
{(const void*)&SpiteString___release, "-\t-", "SpiteString___release", 0},
{(const void*)&spite_string_from_bytes, "-\t-", "spite_string_from_bytes", 0},
{(const void*)&spite_string_join, "-\t-", "spite_string_join", 0},
{(const void*)&ThreadPool___init, "-\t-", "ThreadPool___init", 0},
{(const void*)&ThreadPool___allocate, "-\t-", "ThreadPool___allocate", 0},
{(const void*)&ThreadPool___make, "-\t-", "ThreadPool___make", 0},
{(const void*)&ThreadPool___destroy, "-\t-", "ThreadPool___destroy", 0},
{(const void*)&ThreadPool___discard, "-\t-", "ThreadPool___discard", 0},
{(const void*)&ThreadPoolJob___init, "-\t-", "ThreadPoolJob___init", 0},
{(const void*)&ThreadPoolJob___allocate, "-\t-", "ThreadPoolJob___allocate", 0},
{(const void*)&ThreadPoolJob___make, "-\t-", "ThreadPoolJob___make", 0},
{(const void*)&ThreadPoolJob___retain, "-\t-", "ThreadPoolJob___retain", 0},
{(const void*)&ThreadPoolJob___release, "-\t-", "ThreadPoolJob___release", 0},
{(const void*)&ThreadPoolJob___free, "-\t-", "ThreadPoolJob___free", 0},
{(const void*)&Spite_Argument___init_constructed, "-\t-", "Spite_Argument___init_constructed", 0},
{(const void*)&Spite_Argument___allocate_constructed, "-\t-", "Spite_Argument___allocate_constructed", 0},
{(const void*)&Spite_Argument___make, "-\t-", "Spite_Argument___make", 0},
{(const void*)&Spite_Argument___retain, "-\t-", "Spite_Argument___retain", 0},
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
{(const void*)&Naive___retain, "-\t-", "Naive___retain", 0},
{(const void*)&Naive___release, "-\t-", "Naive___release", 0},
{(const void*)&Naive___free, "-\t-", "Naive___free", 0},
{(const void*)&spite_function_value_Naive_advance_ticks, "-\t-", "spite_function_value_Naive_advance_ticks", 0},
{(const void*)&spite_tagged_SpiteInteger, "-\t-", "spite_tagged_SpiteInteger", 0},
{(const void*)&Orbit___init, "-\t-", "Orbit___init", 0},
{(const void*)&Orbit___allocate, "-\t-", "Orbit___allocate", 0},
{(const void*)&Orbit___make, "-\t-", "Orbit___make", 0},
{(const void*)&Orbit___retain, "-\t-", "Orbit___retain", 0},
{(const void*)&Orbit___release, "-\t-", "Orbit___release", 0},
{(const void*)&Orbit___free, "-\t-", "Orbit___free", 0},
{(const void*)&List_String___release, "-\t-", "List_String___release", 0},
{(const void*)&List_String___free, "-\t-", "List_String___free", 0},
{(const void*)&List_ThreadPoolJob___init, "-\t-", "List_ThreadPoolJob___init", 0},
{(const void*)&List_ThreadPoolJob___allocate, "-\t-", "List_ThreadPoolJob___allocate", 0},
{(const void*)&List_ThreadPoolJob___make, "-\t-", "List_ThreadPoolJob___make", 0},
{(const void*)&List_ThreadPoolJob___release, "-\t-", "List_ThreadPoolJob___release", 0},
{(const void*)&List_ThreadPoolJob___free, "-\t-", "List_ThreadPoolJob___free", 0},
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
{(const void*)&List_Orbit___init, "-\t-", "List_Orbit___init", 0},
{(const void*)&List_Orbit___allocate, "-\t-", "List_Orbit___allocate", 0},
{(const void*)&List_Orbit___make, "-\t-", "List_Orbit___make", 0},
{(const void*)&List_Orbit___release, "-\t-", "List_Orbit___release", 0},
{(const void*)&List_Orbit___free, "-\t-", "List_Orbit___free", 0},
{(const void*)&List_Console_Printable___init, "-\t-", "List_Console_Printable___init", 0},
{(const void*)&List_Console_Printable___retain, "-\t-", "List_Console_Printable___retain", 0},
{(const void*)&List_Console_Printable___release, "-\t-", "List_Console_Printable___release", 0},
{(const void*)&List_Console_Printable___free, "-\t-", "List_Console_Printable___free", 0},
{(const void*)&List_Symbol___release, "-\t-", "List_Symbol___release", 0},
{(const void*)&List_Symbol___free, "-\t-", "List_Symbol___free", 0},
{(const void*)&Benchmark__Nothing___init, "-\t-", "Benchmark__Nothing___init", 0},
{(const void*)&Benchmark__Nothing___allocate, "-\t-", "Benchmark__Nothing___allocate", 0},
{(const void*)&Benchmark__Nothing___make, "-\t-", "Benchmark__Nothing___make", 0},
{(const void*)&Benchmark__Nothing___release, "-\t-", "Benchmark__Nothing___release", 0},
{(const void*)&Benchmark__Nothing___free, "-\t-", "Benchmark__Nothing___free", 0},
{(const void*)&spite_singleton_ThreadPool_teardown, "-\t-", "spite_singleton_ThreadPool_teardown", 0},
{(const void*)&spite_singleton_ThreadPool, "-\t-", "spite_singleton_ThreadPool", 0},
{(const void*)&spite_function_value_List_Orbit_spite_band_advance_piece___arguments, "-\t-", "spite_function_value_List_Orbit_spite_band_advance_piece___arguments", 0},
{(const void*)&spite_function_value_List_Orbit_spite_band_advance_piece, "-\t-", "spite_function_value_List_Orbit_spite_band_advance_piece", 0},
{(const void*)&spite_class_object_Nothing, "-\t-", "spite_class_object_Nothing", 0},
{(const void*)&spite_class_object_Integer, "-\t-", "spite_class_object_Integer", 0},
{(const void*)&Console_Printable___release, "-\t-", "Console_Printable___release", 0},
{(const void*)&Console_Printable___call_to_string, "-\t-", "Console_Printable___call_to_string", 0},
{(const void*)&spite_foreign_library_1, "-\t-", "spite_foreign_library_1", 0},
{(const void*)&spite_foreign_library_2, "-\t-", "spite_foreign_library_2", 0},
{(const void*)&spite_singleton_check_circle, "-\t-", "spite_singleton_check_circle", 0},
{(const void*)&spite_singleton_making, "-\t-", "spite_singleton_making", 0},
{(const void*)&spite_singleton_made, "-\t-", "spite_singleton_made", 0},
{(const void*)&spite_singleton_tracked, "-\t-", "spite_singleton_tracked", 0},
{(const void*)&spite_singleton_created, "-\t-", "spite_singleton_created", 0},
{(const void*)&spite_singleton_used_after_exit, "-\t-", "spite_singleton_used_after_exit", 0},
{(const void*)&spite_singleton_free_later, "-\t-", "spite_singleton_free_later", 0},
{(const void*)&spite_singletons_destroy, "-\t-", "spite_singletons_destroy", 0},
{(const void*)&spite_spin_pause, "-\t-", "spite_spin_pause", 0},
{(const void*)&spite_guard_wait, "-\t-", "spite_guard_wait", 0},
{(const void*)&spite_guard_enter, "-\t-", "spite_guard_enter", 0},
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
{(const void*)&Duration_Duration, "library/duration.spite\tDuration", "Duration", 14},
{(const void*)&Duration_total, "library/duration.spite\tDuration", "total", 22},
{(const void*)&Duration__units_per_second, "library/duration.spite\tDuration", "_units_per_second", 124},
{(const void*)&Duration__nanoseconds_per_unit, "library/duration.spite\tDuration", "_nanoseconds_per_unit", 133},
{(const void*)&Duration__seconds_per_unit, "library/duration.spite\tDuration", "_seconds_per_unit", 142},
{(const void*)&DynamicLibrary_DynamicLibrary, "library/dynamic_library.spite\tDynamicLibrary", "DynamicLibrary", 6},
{(const void*)&DynamicLibrary_drop, "library/dynamic_library.spite\tDynamicLibrary", "drop", 11},
{(const void*)&DynamicLibrary_open_library, "bootstrap/source/generation/prelude.spite\tDynamicLibrary", "open_library", 1},
{(const void*)&DynamicLibrary_find_symbol, "bootstrap/source/generation/prelude.spite\tDynamicLibrary", "find_symbol", 2},
{(const void*)&DynamicLibrary_close_library, "bootstrap/source/generation/prelude.spite\tDynamicLibrary", "close_library", 3},
{(const void*)&SpiteInteger_to_string, "library/integer.spite\tInteger", "to_string", 3},
{(const void*)&SpiteLong_to_string, "library/long.spite\tLong", "to_string", 3},
{(const void*)&SpiteString_to_string, "library/string.spite\tString", "to_string", 184},
{(const void*)&ThreadPool_size, "library/thread_pool.spite\tThreadPool", "size", 14},
{(const void*)&ThreadPool_is_done, "library/thread_pool.spite\tThreadPool", "is_done", 48},
{(const void*)&ThreadPool_join, "library/thread_pool.spite\tThreadPool", "join", 52},
{(const void*)&ThreadPool_piece_start, "library/thread_pool.spite\tThreadPool", "piece_start", 96},
{(const void*)&ThreadPool_start, "library/thread_pool.spite\tThreadPool", "start", 103},
{(const void*)&spite_failed_1, "-\t-", "spite_failed_1", 0},
{(const void*)&ThreadPool__serve, "library/thread_pool.spite\tThreadPool", "_serve", 127},
{(const void*)&ThreadPool_run_queued, "library/thread_pool.spite\tThreadPool", "run_queued", 142},
{(const void*)&spite_failed_2, "-\t-", "spite_failed_2", 0},
{(const void*)&spite_failed_3, "-\t-", "spite_failed_3", 0},
{(const void*)&ThreadPool_drop, "library/thread_pool.spite\tThreadPool", "drop", 158},
{(const void*)&ThreadPool_processor_count, "library/windows/thread_pool.spite\tThreadPool", "processor_count", 3},
{(const void*)&ThreadPool_current_thread, "library/windows/thread_pool.spite\tThreadPool", "current_thread", 8},
{(const void*)&ThreadPool_start_thread, "library/windows/thread_pool.spite\tThreadPool", "start_thread", 12},
{(const void*)&ThreadPool_join_thread, "library/windows/thread_pool.spite\tThreadPool", "join_thread", 17},
{(const void*)&ThreadPool_create_lock, "library/windows/thread_pool.spite\tThreadPool", "create_lock", 22},
{(const void*)&ThreadPool_destroy_lock, "library/windows/thread_pool.spite\tThreadPool", "destroy_lock", 28},
{(const void*)&ThreadPool_lock_queue, "library/windows/thread_pool.spite\tThreadPool", "lock_queue", 32},
{(const void*)&ThreadPool_unlock_queue, "library/windows/thread_pool.spite\tThreadPool", "unlock_queue", 36},
{(const void*)&ThreadPool_create_condition, "library/windows/thread_pool.spite\tThreadPool", "create_condition", 40},
{(const void*)&ThreadPool_wait_for_signal, "library/windows/thread_pool.spite\tThreadPool", "wait_for_signal", 50},
{(const void*)&ThreadPool_signal_one, "library/windows/thread_pool.spite\tThreadPool", "signal_one", 54},
{(const void*)&ThreadPool_signal_all, "library/windows/thread_pool.spite\tThreadPool", "signal_all", 58},
{(const void*)&ThreadPool_entry_address, "bootstrap/source/generation/prelude.spite\tThreadPool", "entry_address", 1},
{(const void*)&ThreadPool_address, "bootstrap/source/generation/prelude.spite\tThreadPool", "address", 2},
{(const void*)&ThreadPool__task_begun, "bootstrap/source/generation/prelude.spite\tThreadPool", "_task_begun", 3},
{(const void*)&ThreadPool__task_ended, "bootstrap/source/generation/prelude.spite\tThreadPool", "_task_ended", 4},
{(const void*)&SpiteMemory_Address_text, "library/memory/address.spite\tMemory.Address", "text", 3},
{(const void*)&SpiteMemory_Address_to_string, "library/memory/address.spite\tMemory.Address", "to_string", 13},
{(const void*)&Memory_Arena_allocate, "library/memory/arena.spite\tMemory.Arena", "allocate", 11},
{(const void*)&Memory_Arena_free, "library/memory/arena.spite\tMemory.Arena", "free", 21},
{(const void*)&Memory_Arena_start_block, "library/memory/arena.spite\tMemory.Arena", "start_block", 23},
{(const void*)&Memory_Heap_allocate, "bootstrap/source/generation/prelude.spite\tMemory.Heap", "allocate", 1},
{(const void*)&Memory_Heap_resize, "bootstrap/source/generation/prelude.spite\tMemory.Heap", "resize", 2},
{(const void*)&Memory_Heap_free, "bootstrap/source/generation/prelude.spite\tMemory.Heap", "free", 3},
{(const void*)&Spite_Argument_Argument, "library/spite/argument.spite\tSpite.Argument", "Argument", 6},
{(const void*)&Spite_Class_Class, "library/spite/class.spite\tSpite.Class", "Class", 16},
{(const void*)&Spite_Function_Function, "library/spite/function.spite\tSpite.Function", "Function", 11},
{(const void*)&Naive_Naive, "benchmarks/a_loop_whose_passes_write_only_their_own_item_runs_in_bands/naive/naive.spite\tNaive", "Naive", 4},
{(const void*)&Naive_make_orbits, "benchmarks/a_loop_whose_passes_write_only_their_own_item_runs_in_bands/naive/naive.spite\tNaive", "make_orbits", 13},
{(const void*)&Naive_advance_ticks, "benchmarks/a_loop_whose_passes_write_only_their_own_item_runs_in_bands/naive/naive.spite\tNaive", "advance_ticks", 22},
{(const void*)&Orbit_Orbit, "benchmarks/a_loop_whose_passes_write_only_their_own_item_runs_in_bands/naive/orbit.spite\tOrbit", "Orbit", 5},
{(const void*)&Orbit_advance, "benchmarks/a_loop_whose_passes_write_only_their_own_item_runs_in_bands/naive/orbit.spite\tOrbit", "advance", 9},
{(const void*)&List_String_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&List_String_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&TypedMemory__String_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&List_ThreadPoolJob_count, "library/list.spite\tList", "count", 9},
{(const void*)&List_ThreadPoolJob_is_empty, "library/list.spite\tList", "is_empty", 13},
{(const void*)&List_ThreadPoolJob_get_at, "library/list.spite\tList", "get_at", 41},
{(const void*)&List_ThreadPoolJob_remove_at, "library/list.spite\tList", "remove_at", 54},
{(const void*)&spite_failed_4, "-\t-", "spite_failed_4", 0},
{(const void*)&spite_failed_5, "-\t-", "spite_failed_5", 0},
{(const void*)&List_ThreadPoolJob_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_ThreadPoolJob_move_items, "library/list.spite\tList", "move_items", 877},
{(const void*)&TypedMemory__ThreadPoolJob_read_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "read_value", 1},
{(const void*)&TypedMemory__ThreadPoolJob_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&TypedMemory__ThreadPoolJob_value_bytes, "bootstrap/source/generation/prelude.spite\tTypedMemory", "value_bytes", 4},
{(const void*)&List_Spite_AttributeDeclaration_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_Spite_Function_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_Spite_Argument_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_Spite_Class_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_Spite_Namespace_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_Orbit_append, "library/list.spite\tList", "append", 17},
{(const void*)&List_Orbit_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_Orbit_make_room, "library/list.spite\tList", "make_room", 849},
{(const void*)&List_Orbit__grow, "library/list.spite\tList", "_grow", 855},
{(const void*)&List_Orbit__resized, "library/list.spite\tList", "_resized", 865},
{(const void*)&List_Orbit_sum_turns, "library/list.spite\tList", "sum_turns", 0},
{(const void*)&List_Orbit_spite_band_advance, "library/list.spite\tList", "spite_band_advance", 1},
{(const void*)&List_Orbit_each_advance, "library/list.spite\tList", "each_advance", 0},
{(const void*)&TypedMemory__Orbit_write_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "write_value", 2},
{(const void*)&TypedMemory__Orbit_value_bytes, "bootstrap/source/generation/prelude.spite\tTypedMemory", "value_bytes", 4},
{(const void*)&List_Console_Printable_get_at, "library/list.spite\tList", "get_at", 41},
{(const void*)&List_Console_Printable_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&TypedMemory__Console_Printable_read_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "read_value", 1},
{(const void*)&List_Symbol_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&Benchmark__Nothing_Benchmark, "library/benchmark.spite\tBenchmark", "Benchmark", 7},
{(const void*)&ThreadPool_run_bands, "library/thread_pool.spite\tThreadPool", "run_bands", 203},
{(const void*)&List_ThreadPoolJob_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&List_Spite_AttributeDeclaration_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&TypedMemory__Spite_AttributeDeclaration_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&List_Spite_Function_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&TypedMemory__Spite_Function_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&List_Spite_Argument_append, "library/list.spite\tList", "append", 17},
{(const void*)&List_Spite_Argument_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&List_Spite_Argument_make_room, "library/list.spite\tList", "make_room", 849},
{(const void*)&List_Spite_Argument__grow, "library/list.spite\tList", "_grow", 855},
{(const void*)&List_Spite_Argument__resized, "library/list.spite\tList", "_resized", 865},
{(const void*)&TypedMemory__Spite_Argument_write_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "write_value", 2},
{(const void*)&TypedMemory__Spite_Argument_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&TypedMemory__Spite_Argument_value_bytes, "bootstrap/source/generation/prelude.spite\tTypedMemory", "value_bytes", 4},
{(const void*)&List_Spite_Class_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&TypedMemory__Spite_Class_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&List_Spite_Namespace_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&TypedMemory__Spite_Namespace_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&List_Orbit_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&List_Orbit_spite_band_advance_piece, "library/list.spite\tList", "spite_band_advance_piece", 1},
{(const void*)&TypedMemory__Orbit_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&List_Console_Printable_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&TypedMemory__Console_Printable_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&ThreadPool_submit, "library/thread_pool.spite\tThreadPool", "submit", 33},
{(const void*)&List_ThreadPoolJob_append, "library/list.spite\tList", "append", 17},
{(const void*)&List_ThreadPoolJob_make_room, "library/list.spite\tList", "make_room", 849},
{(const void*)&List_ThreadPoolJob__grow, "library/list.spite\tList", "_grow", 855},
{(const void*)&List_ThreadPoolJob__resized, "library/list.spite\tList", "_resized", 865},
{(const void*)&TypedMemory__ThreadPoolJob_write_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "write_value", 2},
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
if (spite_class_object_Nothing_cache != 0 && spite_class_object_Nothing_cache->_namespace_ != 0) { Spite_Namespace___release(spite_class_object_Nothing_cache->_namespace_); spite_class_object_Nothing_cache->_namespace_ = 0; }
Spite_Class___release(spite_class_object_Nothing_cache);
if (spite_class_object_Integer_cache != 0 && spite_class_object_Integer_cache->_namespace_ != 0) { Spite_Namespace___release(spite_class_object_Integer_cache->_namespace_); spite_class_object_Integer_cache->_namespace_ = 0; }
Spite_Class___release(spite_class_object_Integer_cache);


return 0;
}
