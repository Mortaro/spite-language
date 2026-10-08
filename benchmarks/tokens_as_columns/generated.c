/* All of the C the compiler writes from naive/ for this case, as an --optimized build for Windows does, with
 * the compiler's own numbered names numbered again from 1. Written by scripts/cases/extract.sh; check.sh
 * compares it with what the compiler writes now. */

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
typedef enum {
Token_TokenKind_identifier,
Token_TokenKind_number,
Token_TokenKind_symbol,
} Token_TokenKind;
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
typedef struct Environment Environment;
typedef struct List List;
typedef struct Program Program;
typedef struct Memory_Arena Memory_Arena;
typedef struct Memory_Heap Memory_Heap;
typedef struct Naive Naive;
typedef struct Token Token;
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
static Program* spite_singleton_Program_cache = 0;
static bool spite_singleton_Program_destroyed = false;
static int32_t spite_singleton_Program_lock = 0;
static Clock* spite_singleton_Clock_cache = 0;
static bool spite_singleton_Clock_destroyed = false;
static int32_t spite_singleton_Clock_lock = 0;
typedef struct { bool has_value; int32_t value; } Nullable_Integer;
static Environment* spite_singleton_Environment_cache = 0;
static bool spite_singleton_Environment_destroyed = false;
static int32_t spite_singleton_Environment_lock = 0;
static SpiteString spite_lit_2 = SPITE_STATIC_STRING("alpha", 5);
static SpiteString spite_lit_3 = SPITE_STATIC_STRING("beta", 4);
static SpiteString spite_lit_4 = SPITE_STATIC_STRING("gamma", 5);
static SpiteString spite_lit_5 = SPITE_STATIC_STRING("delta", 5);
static SpiteString spite_lit_6 = SPITE_STATIC_STRING("value", 5);
static SpiteString spite_lit_7 = SPITE_STATIC_STRING("count", 5);
static SpiteString spite_lit_8 = SPITE_STATIC_STRING("index", 5);
static SpiteString spite_lit_9 = SPITE_STATIC_STRING("total", 5);
static SpiteString spite_lit_10 = SPITE_STATIC_STRING("+", 1);
static SpiteString spite_lit_11 = SPITE_STATIC_STRING("=", 1);
static SpiteString spite_lit_12 = SPITE_STATIC_STRING("(", 1);
static SpiteString spite_lit_13 = SPITE_STATIC_STRING(")", 1);
static SpiteString spite_lit_14 = SPITE_STATIC_STRING(";", 1);
static SpiteString spite_lit_15 = SPITE_STATIC_STRING("*", 1);
typedef void* Spite_Allocator;
struct Launcher {
SpiteHeader header;
Build* build_;
};
typedef struct List_Console_Printable List_Console_Printable;
typedef struct TypedMemory__Console_Printable TypedMemory__Console_Printable;
typedef struct SpiteBox_SpiteLong { SpiteHeader header; int64_t value; } SpiteBox_SpiteLong;
#define SPITE_FRAMED_COUNT 1073741824
static SpiteString spite_lit_16 = SPITE_STATIC_STRING("true", 4);
static SpiteString spite_lit_17 = SPITE_STATIC_STRING("false", 5);
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
static SpiteString spite_lit_18 = SPITE_STATIC_STRING("\n", 1);
static SpiteString spite_lit_19 = SPITE_STATIC_STRING("\n", 1);
static SpiteString spite_lit_20 = SPITE_STATIC_STRING(" ", 1);
#define spite_site_2() "library/console.spite:56 in Console._write_values"

struct DynamicLibrary {
SpiteHeader header;
SpiteString file_name_;
int64_t handle_;
};
struct Environment {
SpiteHeader header;
Program* _program_;
Console* _console_;
int32_t pieces_;
};
static SpiteString spite_lit_21 = SPITE_STATIC_STRING("_", 1);
static SpiteString spite_lit_22 = SPITE_STATIC_STRING("-", 1);
static SpiteString spite_lit_23 = SPITE_STATIC_STRING("--", 2);
static SpiteString spite_lit_24 = SPITE_STATIC_STRING("=", 1);
#define spite_site_3() "spite.crash\t298c4d54"
static SpiteString spite_lit_25 = SPITE_STATIC_STRING("true", 4);
static SpiteString spite_lit_26 = SPITE_STATIC_STRING("--", 2);
static SpiteString spite_lit_27 = SPITE_STATIC_STRING("_", 1);
static SpiteString spite_lit_28 = SPITE_STATIC_STRING("_", 1);
static SpiteString spite_lit_29 = SPITE_STATIC_STRING("-", 1);
static SpiteString spite_lit_30 = SPITE_STATIC_STRING("error: '--", 10);
static SpiteString spite_lit_31 = SPITE_STATIC_STRING("' is written '--", 16);
static SpiteString spite_lit_32 = SPITE_STATIC_STRING("': a program's setting is kebab-case on the command line, like the compiler's flags, and it sets Environment.", 109);
static SpiteString spite_lit_33 = SPITE_STATIC_STRING("=", 1);
#define spite_site_4() "spite.crash\t48a1af91"
static SpiteString spite_lit_34 = SPITE_STATIC_STRING("pieces", 6);
static SpiteString spite_lit_35 = SPITE_STATIC_STRING("pieces", 6);
typedef struct { bool has_value; int64_t value; } Nullable_Long;
#define SpiteFloat_to_long(self) ((int64_t)(self))
#define SpiteFloat_to_double(self) ((double)(self))
#define SpiteInteger_largest() ((int32_t)INT32_MAX)
#define SpiteInteger_smallest() ((int32_t)INT32_MIN)
#define SpiteInteger_to_long(self) ((int64_t)(self))
#define SpiteInteger_to_unsigned_long(self) ((uint64_t)(self))
#define SpiteInteger_to_unsigned_integer(self) ((uint32_t)(self))
#define SpiteInteger_to_byte(self) ((uint8_t)(self))
#define SpiteInteger_to_float(self) ((float)(self))
static SpiteString spite_lit_36 = SPITE_STATIC_STRING("0", 1);
#define spite_site_5() "library/long.spite:17 in Long.to_string"
#define spite_site_6() "library/long.spite:18 in Long.to_string"
#define spite_site_7() "library/long.spite:22 in Long.to_string"
#define spite_site_8() "library/long.spite:26 in Long.to_string"
#define SpiteLong_smallest() ((int64_t)INT64_MIN)
#define SpiteLong_to_unsigned_integer(self) ((uint32_t)(self))
#define SpiteLong_to_unsigned_long(self) ((uint64_t)(self))
struct Program {
SpiteHeader header;
Memory_Heap* heap_;
DynamicLibrary* library_;
DynamicLibrary* kernel_;
};
static void* spite_foreign_2_41 = 0;
static void* spite_foreign_2_42 = 0;
#define SpiteShort_to_integer(self) ((int32_t)(self))
#define SpiteShort_to_long(self) ((int64_t)(self))
#define spite_site_9() "library/string.spite:5 in String.length"
#define SpiteString_code_at(self, index) spite_string_code_at(&(self), (index))
static SpiteString spite_lit_37 = SPITE_STATIC_STRING("", 0);
#define spite_site_10() "library/string.spite:17 in String.slice"
#define spite_site_11() "library/string.spite:25 in String._clamped"
#define spite_site_12() "library/string.spite:57 in String.character_at"
#define spite_site_13() "library/string.spite:61 in String.matches_at"
#define spite_site_14() "library/string.spite:66 in String.matches_at"
#define spite_site_15() "library/string.spite:76 in String.index_of"
#define spite_site_16() "library/string.spite:80 in String.index_of"
#define spite_site_17() "library/string.spite:112 in String.split"
#define spite_site_18() "library/string.spite:116 in String.split"
#define spite_site_19() "library/string.spite:119 in String.split"
#define spite_site_20() "library/string.spite:168 in String.shifted_case"
#define spite_site_21() "library/string.spite:173 in String.shifted_case"
#define spite_site_22() "library/string.spite:175 in String.shifted_case"
#define spite_site_23() "library/string.spite:212 in String.to_long"
#define spite_site_24() "library/string.spite:213 in String.to_long"
#define spite_site_25() "library/string.spite:214 in String.to_long"
#define spite_site_26() "library/string.spite:222 in String.to_long"
#define spite_site_27() "library/string.spite:228 in String.to_integer"
#define SpiteTiny_to_long(self) ((int64_t)(self))
#define SpiteUnsignedInteger_to_long(self) ((int64_t)(self))
#define SpiteUnsignedInteger_to_float(self) ((float)(self))
#define SpiteUnsignedShort_to_integer(self) ((int32_t)(self))
#define SpiteUnsignedShort_to_long(self) ((int64_t)(self))
#define SpiteUnsignedShort_to_unsigned_integer(self) ((uint32_t)(self))
#define spite_site_28() "library/memory/address.spite:8 in Memory.Address.terminated_text"
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
#define spite_site_29() "library/memory/arena.spite:12 in Memory.Arena.allocate"
#define spite_site_30() "library/memory/arena.spite:13 in Memory.Arena.allocate"
#define spite_site_31() "library/memory/arena.spite:17 in Memory.Arena.allocate"
#define spite_site_32() "library/memory/arena.spite:25 in Memory.Arena.start_block"
#define spite_site_33() "library/memory/arena.spite:26 in Memory.Arena.start_block"
struct Memory_Heap {
SpiteHeader header;
};
struct Naive {
SpiteHeader header;
Console* console_;
Clock* clock_;
Environment* environment_;
List_String* words_;
List_String* symbols_;
};
typedef struct List_Token List_Token;
typedef struct TypedMemory__Token TypedMemory__Token;
#define spite_site_34() "benchmarks/tokens_as_columns/naive/naive.spite:16 in Naive.Naive"
static SpiteBox_SpiteString spite_lit_38_box = { { 0, -1 }, SPITE_STATIC_STRING("identifiers", 11) };
typedef struct SpiteBox_SpiteInteger { SpiteHeader header; int32_t value; } SpiteBox_SpiteInteger;
static SpiteBox_SpiteString spite_lit_39_box = { { 0, -1 }, SPITE_STATIC_STRING("numbers", 7) };
static SpiteBox_SpiteString spite_lit_40_box = { { 0, -1 }, SPITE_STATIC_STRING("symbols", 7) };
static SpiteBox_SpiteString spite_lit_41_box = { { 0, -1 }, SPITE_STATIC_STRING("number total", 12) };
static SpiteBox_SpiteString spite_lit_42_box = { { 0, -1 }, SPITE_STATIC_STRING("checksums", 9) };
static SpiteString spite_lit_43 = SPITE_STATIC_STRING("microseconds ", 13);
static SpiteString spite_lit_44 = SPITE_STATIC_STRING("", 0);
#define spite_site_35() "benchmarks/tokens_as_columns/naive/naive.spite:39 in Naive.written"
static SpiteString spite_lit_45 = SPITE_STATIC_STRING(" ", 1);
static SpiteString spite_lit_46 = SPITE_STATIC_STRING("\n", 1);
#define spite_site_36() "spite.crash\t3fec7add"
#define spite_site_37() "spite.crash\t680a7532"
#define spite_site_38() "benchmarks/tokens_as_columns/naive/naive.spite:73 in Naive.tokenized"
#define spite_site_39() "benchmarks/tokens_as_columns/naive/naive.spite:76 in Naive.tokenized"
#define spite_site_40() "benchmarks/tokens_as_columns/naive/naive.spite:83 in Naive.tokenized"
#define spite_site_41() "benchmarks/tokens_as_columns/naive/naive.spite:88 in Naive.tokenized"
#define spite_site_42() "benchmarks/tokens_as_columns/naive/naive.spite:91 in Naive.tokenized"
#define spite_site_43() "benchmarks/tokens_as_columns/naive/naive.spite:94 in Naive.tokenized"
#define spite_site_44() "benchmarks/tokens_as_columns/naive/naive.spite:106 in Naive.total_of_numbers"
#define spite_site_45() "benchmarks/tokens_as_columns/naive/naive.spite:108 in Naive.total_of_numbers"
struct Token {
SpiteHeader header;
Token_TokenKind kind_;
int32_t start_;
int32_t length_;
int32_t line_;
int64_t value_;
};
#define spite_site_46() "benchmarks/tokens_as_columns/naive/token.spite:23 in Token.checksum"
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
struct List_Token {
SpiteHeader header;
Memory_Heap* heap_;
TypedMemory__Token* values_;
int64_t items_;
int32_t item_count_;
int32_t capacity_;
};
struct TypedMemory__Token {
SpiteHeader header;
};
#define SpiteByte_to_unsigned_integer(self) ((uint32_t)(self))
#define SpiteDouble_to_float(self) ((float)(self))
#define SpiteInteger_to_short(self) ((int16_t)(self))
#define SpiteLong_to_double(self) ((double)(self))
#define spite_site_47() "library/list.spite:20 in List.append"
#define spite_site_48() "library/list.spite:185 in List.join"
#define spite_site_49() "library/list.spite:187 in List.join"
#define spite_site_50() "library/list.spite:194 in List.join"
#define spite_site_51() "library/list.spite:615 in List.write_text"
#define spite_site_52() "library/list.spite:618 in List.write_text"
#define spite_site_53() "library/list.spite:856 in List._grow"
#define spite_site_54() "library/list.spite:861 in List._grow"
#define spite_site_55() "library/list.spite:707 in List.sum_checksum"
static SpiteString spite_symbol_1 = { (int64_t)0x797469746e656469ULL, (int64_t)0x0700000000000000ULL };
Memory_Heap* spite_singleton_Memory_Heap(void);
TypedMemory__String* spite_singleton_TypedMemory__String(void);
int64_t SpiteArguments_count(SpiteArguments* self);
SpiteString SpiteArguments_get(SpiteArguments* self, int64_t index);
Console_Printable Console_Printable___retain(Console_Printable self);
void Console_Printable___release(Console_Printable self);
Build* spite_singleton_Build(void);
Console* spite_singleton_Console(void);
DynamicLibrary* spite_foreign_library_1(void);
DynamicLibrary* spite_foreign_library_2(void);
Program* spite_singleton_Program(void);
Clock* spite_singleton_Clock(void);
Environment* spite_singleton_Environment(void);
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
static List_String* List_String___framed(List_String* self, int64_t items, int32_t count);
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
void Environment___init(Environment* self);
Environment* Environment___allocate(void);
Environment* Environment___make(void);
void Environment___release(Environment* self);
SpiteString Environment_setting(Environment* self, SpiteString name_, bool bare_allowed_);
void Environment_reject_misspelled_settings(Environment* self, List_String* declared_);
void Environment_reject_misspelled(Environment* self, SpiteString argument_, List_String* declared_);
SpiteString Environment_setting_written(Environment* self, SpiteString argument_);
int32_t Environment_integer_setting(Environment* self, SpiteString name_, int32_t declared_);
void Environment_Environment(Environment* self);
void Environment___destroy(Environment* self);
void Environment___discard(Environment* self);
static SPITE_CRASH_REPORT void spite_failed_1(bool bare_allowed_, SpiteString name_, SpiteString kebab_name_, SpiteString bare_flag_, SpiteString flag_, int32_t index_, SpiteString argument_, Environment* self);
static SPITE_CRASH_REPORT void spite_failed_2(SpiteString name_, int32_t declared_, SpiteString text_, Environment* self);
SpiteString SpiteInteger_to_string(int32_t self);
SpiteString SpiteLong_to_string(int64_t self);
void Program___init(Program* self);
Program* Program___allocate(void);
Program* Program___make(void);
void Program___release(Program* self);
void Program_exit(Program* self, int32_t code_);
SpiteString Program_environment(Program* self, SpiteString name_);
void Program_exit_process(Program* self, int32_t code_);
int64_t Program_environment_address(Program* self, SpiteString name_);
void Program__flush_output(Program* self);
void Program___destroy(Program* self);
void Program___discard(Program* self);
int32_t SpiteString_length(SpiteString self);
SpiteString SpiteString_slice(SpiteString self, int32_t start_, int32_t end_);
int32_t SpiteString__clamped(SpiteString self, int32_t position_);
bool SpiteString_equals(SpiteString self, SpiteString other_);
bool SpiteString_is_empty(SpiteString self);
SpiteString SpiteString_character_at(SpiteString self, int32_t index_);
bool SpiteString_matches_at(SpiteString self, SpiteString text_, int32_t start_);
int32_t SpiteString_index_of(SpiteString self, SpiteString text_);
bool SpiteString_contains(SpiteString self, SpiteString text_);
bool SpiteString_starts_with(SpiteString self, SpiteString text_);
List_String* SpiteString_split(SpiteString self, SpiteString separator_);
SpiteString SpiteString_replace(SpiteString self, SpiteString from_, SpiteString to_);
bool SpiteString_is_space_at(SpiteString self, int32_t index_);
SpiteString SpiteString_upper_case(SpiteString self);
SpiteString SpiteString_shifted_case(SpiteString self, int32_t first_, int32_t last_, int32_t shift_);
SpiteString SpiteString_to_string(SpiteString self);
Nullable_Long SpiteString_to_long(SpiteString self);
Nullable_Integer SpiteString_to_integer(SpiteString self);
int32_t SpiteString_first_non_space(SpiteString self);
bool SpiteString_only_spaces_from(SpiteString self, int32_t start_);
SpiteString SpiteString___retain(SpiteString self);
void SpiteString___release(SpiteString self);
SpiteString SpiteString_append(SpiteString left, SpiteString right);
SpiteString spite_string_from_bytes(const char* bytes, int64_t length);
SpiteString spite_string_from_cstring_owned(const char* text);
SpiteString spite_string_join(int32_t count, const SpiteString* pieces);
static int64_t spite_long_digits(char* digits, int64_t value);
static SpiteStringBlock* spite_string_block(int64_t length);
static SpiteString spite_string_held(SpiteStringBlock* block, int64_t length);
SpiteString SpiteMemory_Address_text(int64_t self, int64_t length_);
SpiteString SpiteMemory_Address_terminated_text(int64_t self);
int64_t Memory_Arena_allocate(Memory_Arena* self, int64_t bytes_);
void Memory_Arena_free(Memory_Arena* self, int64_t _address_);
void Memory_Arena_start_block(Memory_Arena* self, int64_t at_least_);
void Memory_Heap___release(Memory_Heap* self);
int64_t Memory_Heap_allocate(Memory_Heap* self, int64_t bytes_);
int64_t Memory_Heap_resize(Memory_Heap* self, int64_t address_, int64_t bytes_);
void Memory_Heap_free(Memory_Heap* self, int64_t address_);
void Naive___init(Naive* self);
Naive* Naive___allocate(void);
static inline void Naive___release(Naive* self);
void Naive___free(Naive* self);
void Naive_Naive(Naive* self);
SpiteString Naive_written(Naive* self, int32_t pieces_);
SpiteString Naive_piece_of(Naive* self, int64_t choice_, int32_t picked_, int64_t seed_);
TypedMemory__Token* spite_singleton_TypedMemory__Token(void);
List_Token* Naive_tokenized(Naive* self, SpiteString source_);
int32_t List_Token_count_identifier(List_Token* self);
int32_t List_Token_count_number(List_Token* self);
int32_t List_Token_count_symbol(List_Token* self);
int64_t Naive_total_of_numbers___held_0(Naive* self, List_Token* tokens_);
int64_t List_Token_sum_checksum(List_Token* self);
static inline SpiteTagged spite_tagged_SpiteInteger(int32_t value);
static SPITE_CRASH_REPORT void spite_failed_3(int32_t picked_, Naive* self, int64_t choice_, int64_t seed_);
static SPITE_CRASH_REPORT void spite_failed_4(int32_t symbol_index_, Naive* self, int64_t choice_, int32_t picked_, int64_t seed_);
void Token___init(Token* self);
Token* Token___allocate(void);
Token* Token___make(Token_TokenKind made_kind_, int32_t made_start_, int32_t made_length_, int32_t made_line_, int64_t made_value_);
static inline Token* Token___retain(Token* self);
static inline void Token___release(Token* self);
void Token___free(Token* self);
void Token_Token(Token* self, Token_TokenKind made_kind_, int32_t made_start_, int32_t made_length_, int32_t made_line_, int64_t made_value_);
int64_t Token_checksum(Token* self);
void List_String___init(List_String* self);
List_String* List_String___allocate(void);
List_String* List_String___make(void);
static inline List_String* List_String___retain(List_String* self);
static inline void List_String___release(List_String* self);
void List_String___free(List_String* self);
void List_String_drop(List_String* self);
int32_t List_String_count(List_String* self);
void List_String_append(List_String* self, SpiteString value_);
SpiteString List_String_get_at(List_String* self, int32_t index_);
void List_String_clear(List_String* self);
bool List_String_contains(List_String* self, SpiteString value_);
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
void List_Token___init(List_Token* self);
List_Token* List_Token___allocate(void);
List_Token* List_Token___make(void);
static inline List_Token* List_Token___retain(List_Token* self);
static inline void List_Token___release(List_Token* self);
void List_Token___free(List_Token* self);
void List_Token_drop(List_Token* self);
int32_t List_Token_count(List_Token* self);
void List_Token_append(List_Token* self, Token* value_);
void List_Token_clear(List_Token* self);
void List_Token_drop(List_Token* self);
void List_Token_make_room(List_Token* self);
void List_Token__grow(List_Token* self);
int64_t List_Token__resized(List_Token* self, int64_t old_bytes_, int64_t new_bytes_);
int32_t List_Token_count_identifier(List_Token* self);
int32_t List_Token_count_number(List_Token* self);
int32_t List_Token_count_symbol(List_Token* self);
int64_t List_Token_sum_checksum(List_Token* self);
void TypedMemory__Token___release(TypedMemory__Token* self);
void TypedMemory__Token_write_value(TypedMemory__Token* self, int64_t address_, int32_t index_, Token* value_);
void TypedMemory__Token_release_value(TypedMemory__Token* self, int64_t address_, int32_t index_);
int64_t TypedMemory__Token_value_bytes(TypedMemory__Token* self);
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
#define SPITE_ALLOCATOR_List_Token(object, heap) ((void)(object), ((Spite_Allocator)heap()))
static __typeof__(&TypedMemory__String___release) spite_folded_TypedMemory__String___release = ((__typeof__(&TypedMemory__String___release))&Memory_Heap___release);
static __typeof__(&TypedMemory__Console_Printable___release) spite_folded_TypedMemory__Console_Printable___release = ((__typeof__(&TypedMemory__Console_Printable___release))&Memory_Heap___release);
static __typeof__(&TypedMemory__Token___release) spite_folded_TypedMemory__Token___release = ((__typeof__(&TypedMemory__Token___release))&Memory_Heap___release);
static __typeof__(&List_Console_Printable_count) spite_folded_List_Console_Printable_count = ((__typeof__(&List_Console_Printable_count))&List_String_count);
static __typeof__(&List_Token_count) spite_folded_List_Token_count = ((__typeof__(&List_Token_count))&List_String_count);
static Token* Token___pool_free = 0;
static char* Token___pool_next = 0;
static char* Token___pool_end = 0;
static size_t Token___pool_count = 0;
static void Token___pool_grow(void) {
if (Token___pool_count == 0) { Token___pool_count = 16; } else if (Token___pool_count * sizeof(Token) < 262144) { Token___pool_count = Token___pool_count * 2; }
char* chunk = (char*)SPITE_MALLOC(Token___pool_count * sizeof(Token) + 63);
if (chunk == 0) { fflush(stdout); fputs("spite: out of memory making an object\n", stderr); exit(1); }
Token___pool_next = (char*)(((uintptr_t)chunk + 63) & ~(uintptr_t)63);
Token___pool_end = Token___pool_next + Token___pool_count * sizeof(Token);
}
static inline Token* Token___pool_take(void) {
Token* self = Token___pool_free;
if (self != 0) { Token___pool_free = *(Token**)self; return self; }
if (Token___pool_next == Token___pool_end) Token___pool_grow();
self = (Token*)Token___pool_next;
Token___pool_next = Token___pool_next + sizeof(Token);
return self;
}
static inline void Token___pool_give(Token* self) {
*(Token**)self = Token___pool_free;
Token___pool_free = self;
}
Memory_Heap* spite_singleton_Memory_Heap(void) {
static Memory_Heap spite_object = { { 1, 94 } };
return &spite_object;
}
TypedMemory__String* spite_singleton_TypedMemory__String(void) {
static TypedMemory__String spite_object = { { 1, 112 } };
return &spite_object;
}
int64_t SpiteArguments_count(SpiteArguments* self) {
    return self->count;
}

SpiteString SpiteArguments_get(SpiteArguments* self, int64_t index) {
    if (index < 0 || index >= self->count) return SPITE_STRING_EMPTY;
    return spite_string_from_cstring_owned(self->items[index]);
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
static void spite_singleton_Program_teardown(void) {
Program* object = spite_singleton_Program_cache;
spite_singleton_Program_cache = 0;
spite_singleton_Program_destroyed = true;
Program___destroy(object);
}
Program* spite_singleton_Program(void) {
Program* found = SPITE_SINGLETON_FOUND(spite_singleton_Program_cache);
if (found != 0) return found;
spite_singleton_check_circle("Program");
SPITE_LOCK(spite_singleton_Program_lock);
if (spite_singleton_Program_cache == 0) {
if (spite_singleton_Program_destroyed) spite_singleton_used_after_exit("Program");
spite_singleton_making("Program");
Program* made = Program___make();
spite_singleton_made();
spite_singleton_created(spite_singleton_Program_teardown);
SPITE_SINGLETON_PUBLISH(spite_singleton_Program_cache, made);
}
SPITE_UNLOCK(spite_singleton_Program_lock);
return spite_singleton_Program_cache;
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
static void spite_singleton_Environment_teardown(void) {
Environment* object = spite_singleton_Environment_cache;
spite_singleton_Environment_cache = 0;
spite_singleton_Environment_destroyed = true;
Environment___destroy(object);
}
Environment* spite_singleton_Environment(void) {
Environment* found = SPITE_SINGLETON_FOUND(spite_singleton_Environment_cache);
if (found != 0) return found;
spite_singleton_check_circle("Environment");
SPITE_LOCK(spite_singleton_Environment_lock);
if (spite_singleton_Environment_cache == 0) {
if (spite_singleton_Environment_destroyed) spite_singleton_used_after_exit("Environment");
spite_singleton_making("Environment");
Environment* made = Environment___make();
spite_singleton_made();
spite_singleton_created(spite_singleton_Environment_teardown);
SPITE_SINGLETON_PUBLISH(spite_singleton_Environment_cache, made);
}
SPITE_UNLOCK(spite_singleton_Environment_lock);
return spite_singleton_Environment_cache;
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
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
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
static TypedMemory__Console_Printable spite_object = { { 1, 150 } };
return &spite_object;
}
static List_Console_Printable* List_Console_Printable___framed(List_Console_Printable* self, int64_t items, int32_t count) {
List_Console_Printable___init(self);
self->header.ref_count = 2;
self->header.class_id = 149;
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
tagged.tag = 151;
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
static List_String* List_String___framed(List_String* self, int64_t items, int32_t count) {
List_String___init(self);
self->header.ref_count = 2;
self->header.class_id = 111;
self->items_ = items;
self->item_count_ = count;
self->capacity_ = count;
return self;
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
void Environment___init(Environment* self) {
self->_program_ = spite_singleton_Program();
self->_console_ = spite_singleton_Console();
self->pieces_ = 1500000;
}
Environment* Environment___allocate(void) {
Environment* self = (Environment*)SPITE_MALLOC(sizeof(Environment));
self->header.ref_count = 1;
self->header.class_id = 29;
Environment___init(self);
#ifdef SPITE_TRACKS_Environment
spite_track_Environment(self);
#endif
return self;
}
Environment* Environment___make(void) {
Environment* self = Environment___allocate();
Environment_Environment(self);
return self;
}
void Environment___release(Environment* self) { (void)self; }
void Environment___destroy(Environment* self) {
if (self == 0) return;
Environment___discard(self);
}
void Environment___discard(Environment* self) {
if (self == 0) return;
Program___release(self->_program_);
Console___release(self->_console_);
#ifdef SPITE_TRACKS_Environment
spite_untrack_Environment(self);
#endif
#ifdef SPITE_WEAK_Environment
spite_weak_object_freed(self);
#endif
spite_singleton_free_later(self);
}
void Program___init(Program* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->library_ = spite_foreign_library_2();
self->kernel_ = spite_foreign_library_1();
}
Program* Program___allocate(void) {
Program* self = (Program*)SPITE_MALLOC(sizeof(Program));
self->header.ref_count = 1;
self->header.class_id = 61;
Program___init(self);
#ifdef SPITE_TRACKS_Program
spite_track_Program(self);
#endif
return self;
}
Program* Program___make(void) {
Program* self = Program___allocate();
return self;
}
void Program___release(Program* self) { (void)self; }
void Program___destroy(Program* self) {
if (self == 0) return;
Program___discard(self);
}
void Program___discard(Program* self) {
if (self == 0) return;
Memory_Heap___release(self->heap_);
DynamicLibrary___release(self->library_);
DynamicLibrary___release(self->kernel_);
#ifdef SPITE_TRACKS_Program
spite_untrack_Program(self);
#endif
#ifdef SPITE_WEAK_Program
spite_weak_object_freed(self);
#endif
spite_singleton_free_later(self);
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
SpiteString spite_string_from_cstring_owned(const char* text) {
return spite_string_from_bytes(text, (int64_t)strlen(text));
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
SpiteString SpiteString_append(SpiteString left, SpiteString right) {
int64_t left_length = spite_string_length(left);
int64_t right_length = spite_string_length(right);
int64_t total = left_length + right_length;
const char* right_bytes = spite_string_bytes(&right);
if (SPITE_STRING_IS_INLINE(left) && total <= SPITE_STRING_INLINE) {
if (right_length > 0) memcpy((char*)&left + left_length, right_bytes, (size_t)right_length);
((char*)&left)[15] = (char)(SPITE_STRING_INLINE - total);
return left;
}
if (SPITE_STRING_IS_HEAP(left) && left._bytes_ != right._bytes_) {
SpiteStringBlock* block = SPITE_STRING_BLOCK(left);
if (block->header.ref_count == 1) {
if (total > block->capacity) {
int64_t capacity = block->capacity * 2;
if (capacity < total) capacity = total;
size_t size = ((size_t)capacity + 17 + 15) & ~(size_t)15;
block = (SpiteStringBlock*)SPITE_REALLOC(block, size);
block->capacity = (int64_t)size - 17;
}
memcpy(block->bytes + left_length, right_bytes, (size_t)right_length);
return spite_string_held(block, total);
}
}
int64_t capacity = left_length * 2;
if (capacity < total) capacity = total;
SpiteStringBlock* block = spite_string_block(capacity);
if (left_length > 0) memcpy(block->bytes, spite_string_bytes(&left), (size_t)left_length);
if (right_length > 0) memcpy(block->bytes + left_length, right_bytes, (size_t)right_length);
SpiteString___release(left);
return spite_string_held(block, total);
}
void Memory_Heap___release(Memory_Heap* self) { (void)self; }
TypedMemory__Token* spite_singleton_TypedMemory__Token(void) {
static TypedMemory__Token spite_object = { { 1, 167 } };
return &spite_object;
}
void Naive___init(Naive* self) {
self->console_ = spite_singleton_Console();
self->clock_ = spite_singleton_Clock();
self->environment_ = spite_singleton_Environment();
self->words_ = ({ List_String* spite_temp_1 = List_String___make(); List_String_append(spite_temp_1, spite_lit_2); List_String_append(spite_temp_1, spite_lit_3); List_String_append(spite_temp_1, spite_lit_4); List_String_append(spite_temp_1, spite_lit_5); List_String_append(spite_temp_1, spite_lit_6); List_String_append(spite_temp_1, spite_lit_7); List_String_append(spite_temp_1, spite_lit_8); List_String_append(spite_temp_1, spite_lit_9); spite_temp_1; });
self->symbols_ = ({ List_String* spite_temp_2 = List_String___make(); List_String_append(spite_temp_2, spite_lit_10); List_String_append(spite_temp_2, spite_lit_11); List_String_append(spite_temp_2, spite_lit_12); List_String_append(spite_temp_2, spite_lit_13); List_String_append(spite_temp_2, spite_lit_14); List_String_append(spite_temp_2, spite_lit_15); spite_temp_2; });
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
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Naive___free(self);
}
void Naive___free(Naive* self) {
Console___release(self->console_);
Clock___release(self->clock_);
Environment___release(self->environment_);
List_String___release(self->words_);
List_String___release(self->symbols_);
#ifdef SPITE_TRACKS_Naive
spite_untrack_Naive(self);
#endif
#ifdef SPITE_WEAK_Naive
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
static inline SpiteTagged spite_tagged_SpiteInteger(int32_t value) {
SpiteTagged tagged;
tagged.tag = 168;
tagged.plain = 1;
tagged.value.bits = 0;
memcpy(&tagged.value, &value, sizeof(value));
return tagged;
}
void Token___init(Token* self) {
self->kind_ = Token_TokenKind_symbol;
self->start_ = 0;
self->length_ = 0;
self->line_ = 0;
self->value_ = SpiteInteger_to_long(0);
}
Token* Token___allocate(void) {
Token* self = Token___pool_take();
self->header.ref_count = 1;
self->header.class_id = 110;
Token___init(self);
#ifdef SPITE_TRACKS_Token
spite_track_Token(self);
#endif
return self;
}
Token* Token___make(Token_TokenKind made_kind_, int32_t made_start_, int32_t made_length_, int32_t made_line_, int64_t made_value_) {
Token* self = Token___allocate();
Token_Token(self, made_kind_, made_start_, made_length_, made_line_, made_value_);
return self;
}
static inline Token* Token___retain(Token* self) {
if (self != 0) SPITE_COUNT_UP(self->header.ref_count);
return self;
}
static inline void Token___release(Token* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Token___free(self);
}
void Token___free(Token* self) {
#ifdef SPITE_TRACKS_Token
spite_untrack_Token(self);
#endif
#ifdef SPITE_WEAK_Token
spite_weak_object_freed(self);
#endif
Token___pool_give(self);
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
self->header.class_id = 111;
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
static inline List_String* List_String___retain(List_String* self) {
if (self != 0) SPITE_COUNT_UP(self->header.ref_count);
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
void List_Console_Printable___init(List_Console_Printable* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->values_ = spite_singleton_TypedMemory__Console_Printable();
self->items_ = ((int64_t)(0));
self->item_count_ = 0;
self->capacity_ = 0;
}
static inline List_Console_Printable* List_Console_Printable___retain(List_Console_Printable* self) {
if (self != 0) SPITE_COUNT_UP(self->header.ref_count);
return self;
}
static inline void List_Console_Printable___release(List_Console_Printable* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
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
void List_Token___init(List_Token* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->values_ = spite_singleton_TypedMemory__Token();
self->items_ = ((int64_t)(0));
self->item_count_ = 0;
self->capacity_ = 0;
}
List_Token* List_Token___allocate(void) {
List_Token* self = (List_Token*)SPITE_MALLOC(sizeof(List_Token));
self->header.ref_count = 1;
self->header.class_id = 166;
List_Token___init(self);
#ifdef SPITE_TRACKS_List_Token
spite_track_List_Token(self);
#endif
return self;
}
List_Token* List_Token___make(void) {
List_Token* self = List_Token___allocate();
return self;
}
static inline List_Token* List_Token___retain(List_Token* self) {
if (self != 0) SPITE_COUNT_UP(self->header.ref_count);
return self;
}
static inline void List_Token___release(List_Token* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
List_Token___free(self);
}
void List_Token___free(List_Token* self) {
List_Token_drop(self);
Memory_Heap___release(self->heap_);
spite_folded_TypedMemory__Token___release(self->values_);
#ifdef SPITE_TRACKS_List_Token
spite_untrack_List_Token(self);
#endif
#ifdef SPITE_WEAK_List_Token
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void Console_Printable___release(Console_Printable self) {
if (self.plain != 0 || self.value.object == 0) return;
if (((self).tag == 0) && ((self).plain == 0)) { spite_string_box_release(self.value.object); return; }
}
SpiteString Console_Printable___call_to_string(Console_Printable self) {
if (((self).tag == 0) && ((self).plain == 0)) return SpiteString_to_string((((SpiteBox_SpiteString*)(self).value.object)->value));
if ((self).tag == 151) return SpiteLong_to_string(SPITE_TAGGED_VALUE(self, int64_t));
if ((self).tag == 168) return SpiteInteger_to_string(SPITE_TAGGED_VALUE(self, int32_t));
fputs("spite.crash\tPrintable.to_string was called on a value of a class it was not compiled for\n", stderr);
abort();
}
static int32_t spite_foreign_library_1_lock = 0;
DynamicLibrary* spite_foreign_library_1(void) {
DynamicLibrary* found = SPITE_SINGLETON_FOUND(spite_foreign_library_1_cache);
if (found != 0) return found;
SPITE_LOCK(spite_foreign_library_1_lock);
if (spite_foreign_library_1_cache == 0) {
DynamicLibrary* made = DynamicLibrary___make(((SpiteString)SPITE_STATIC_STRING("kernel32.dll", 12)), spite_symbol_1, ((SpiteString)SPITE_STATIC_STRING("", 0)));
spite_foreign_library_1_tracked = spite_singleton_tracked();
(void)&DynamicLibrary_find_symbol;
spite_foreign_1_0 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("QueryPerformanceFrequency", 25)), ((SpiteString)SPITE_STATIC_STRING("Clock.Clock", 11)));
spite_foreign_1_1 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("QueryPerformanceCounter", 23)), ((SpiteString)SPITE_STATIC_STRING("Clock.elapsed_nanoseconds", 25)));












































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
DynamicLibrary* made = DynamicLibrary___make(((SpiteString)SPITE_STATIC_STRING("ucrtbase.dll", 12)), spite_symbol_1, ((SpiteString)SPITE_STATIC_STRING("", 0)));
spite_foreign_library_2_tracked = spite_singleton_tracked();
(void)&DynamicLibrary_find_symbol;












spite_foreign_2_41 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("exit", 4)), ((SpiteString)SPITE_STATIC_STRING("Program.exit_process", 20)));
spite_foreign_2_42 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("getenv", 6)), ((SpiteString)SPITE_STATIC_STRING("Program.environment_address", 27)));

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
void Launcher_Launcher(Launcher* self) {
(void)0;
(void)0;
({ Naive* spite_entry_instance = Naive___allocate(); Naive_Naive(spite_entry_instance); Naive___release(spite_entry_instance); (void)0; });
}
SpiteString SpiteBoolean_to_string(bool self) {
if ((self)) {
SpiteString spite_temp_3 = spite_lit_16;
return spite_temp_3;
}
SpiteString spite_temp_4 = spite_lit_17;
return spite_temp_4;
}
void Clock_Clock(Clock* self) {
int64_t spite_temp_5[1];
int64_t spite_temp_6 = SpiteInteger_to_long(8);
int64_t frequency_ = spite_temp_6 <= 8 ? (int64_t)(intptr_t)spite_temp_5 : Memory_Heap_allocate(self->heap_, spite_temp_6);
(void)(({ spite_last_foreign_call = "QueryPerformanceFrequency\tlibrary=kernel32.dll\tfrom=library/windows/clock.spite:6"; int32_t spite_temp_7 = ((int32_t (*)(int64_t))spite_foreign_1_0)((int64_t)(frequency_));  int32_t spite_foreign_result = spite_temp_7;  (void)spite_foreign_result; spite_temp_7; }));
self->_ticks_per_second_ = SpiteMemory_Address_read_long(frequency_, SpiteInteger_to_long(0));
if (frequency_ != (int64_t)(intptr_t)spite_temp_5) Memory_Heap_free(self->heap_, frequency_);
}
int64_t Clock_elapsed_nanoseconds(Clock* self) {
int64_t spite_temp_8[1];
int64_t spite_temp_9 = SpiteInteger_to_long(8);
int64_t counter_ = spite_temp_9 <= 8 ? (int64_t)(intptr_t)spite_temp_8 : Memory_Heap_allocate(self->heap_, spite_temp_9);
(void)(({ spite_last_foreign_call = "QueryPerformanceCounter\tlibrary=kernel32.dll\tfrom=library/windows/clock.spite:13"; int32_t spite_temp_10 = ((int32_t (*)(int64_t))spite_foreign_1_1)((int64_t)(counter_));  int32_t spite_foreign_result = spite_temp_10;  (void)spite_foreign_result; spite_temp_10; }));
int64_t ticks_ = SpiteMemory_Address_read_long(counter_, SpiteInteger_to_long(0));
if (counter_ != (int64_t)(intptr_t)spite_temp_8) Memory_Heap_free(self->heap_, counter_);
int64_t spite_temp_11 = ({ int64_t spite_temp_12 = ({ int64_t spite_temp_13 = ({ int64_t spite_temp_14 = ticks_; int64_t spite_temp_15 = self->_ticks_per_second_; if (spite_temp_15 == 0) spite_divided_by_zero("ticks / _ticks_per_second", spite_site_1()); int64_t spite_temp_16 = 0; if (__builtin_expect(spite_temp_15 == -1 && __builtin_sub_overflow((int64_t)0, spite_temp_14, &spite_temp_16), 0)) spite_overflowed("ticks / _ticks_per_second", "a Long", "/", (int64_t)spite_temp_14, (int64_t)spite_temp_15, spite_site_1()); (int64_t)(spite_temp_15 == -1 ? spite_temp_16 : spite_temp_14 / spite_temp_15); }); int64_t spite_temp_17 = SpiteInteger_to_long(1000000000); int64_t spite_temp_18; if (__builtin_expect(__builtin_mul_overflow(spite_temp_13, spite_temp_17, &spite_temp_18), 0)) spite_overflowed("ticks / _ticks_per_second * 1000000000", "a Long", "*", (int64_t)spite_temp_13, (int64_t)spite_temp_17, spite_site_1()); spite_temp_18; }); int64_t spite_temp_19 = ({ int64_t spite_temp_20 = ({ int64_t spite_temp_21 = ({ int64_t spite_temp_22 = ticks_; int64_t spite_temp_23 = self->_ticks_per_second_; if (spite_temp_23 == 0) spite_divided_by_zero("ticks % _ticks_per_second", spite_site_1()); (int64_t)(spite_temp_23 == -1 ? (int64_t)0 : spite_temp_22 % spite_temp_23); }); int64_t spite_temp_24 = SpiteInteger_to_long(1000000000); int64_t spite_temp_25; if (__builtin_expect(__builtin_mul_overflow(spite_temp_21, spite_temp_24, &spite_temp_25), 0)) spite_overflowed("ticks % _ticks_per_second * 1000000000", "a Long", "*", (int64_t)spite_temp_21, (int64_t)spite_temp_24, spite_site_1()); spite_temp_25; }); int64_t spite_temp_26 = self->_ticks_per_second_; if (spite_temp_26 == 0) spite_divided_by_zero("ticks % _ticks_per_second * 1000000000 / _ticks_per_second", spite_site_1()); int64_t spite_temp_27 = 0; if (__builtin_expect(spite_temp_26 == -1 && __builtin_sub_overflow((int64_t)0, spite_temp_20, &spite_temp_27), 0)) spite_overflowed("ticks % _ticks_per_second * 1000000000 / _ticks_per_second", "a Long", "/", (int64_t)spite_temp_20, (int64_t)spite_temp_26, spite_site_1()); (int64_t)(spite_temp_26 == -1 ? spite_temp_27 : spite_temp_20 / spite_temp_26); }); int64_t spite_temp_28; if (__builtin_expect(__builtin_add_overflow(spite_temp_12, spite_temp_19, &spite_temp_28), 0)) spite_overflowed("ticks / _ticks_per_second * 1000000000 + ticks % _ticks_per_second * 1000000000 / _ticks_per_second", "a Long", "+", (int64_t)spite_temp_12, (int64_t)spite_temp_19, spite_site_1()); spite_temp_28; });
return spite_temp_11;
}
void Console_print(Console* self, List_Console_Printable* values_) {
Console__write_values(self, List_Console_Printable___retain(values_), Console_Stream_output);
Console__write_output(self, spite_lit_18);
Console__flush(self);
List_Console_Printable___release(values_);
}
void Console_error(Console* self, List_Console_Printable* values_) {
Console__flush(self);
Console__write_values(self, List_Console_Printable___retain(values_), Console_Stream_error);
Console__write_error(self, spite_lit_19);
Console__flush(self);
List_Console_Printable___release(values_);
}
void Console__write_values(Console* self, List_Console_Printable* values_, Console_Stream stream_) {
int32_t index_ = 0;
while (((index_ < spite_folded_List_Console_Printable_count(values_)))) {
if (((index_ > 0))) {
Console__write_to(self, spite_lit_20, stream_);
}
SpiteString text_ = ({ Console_Printable spite_temp_29 = ({ Console_Printable spite_temp_30 = List_Console_Printable_get_at(values_, index_); if (__builtin_expect(!(SPITE_TAGGED_PRESENT(spite_temp_30)), 0)) spite_outside_list("values[index]", spite_site_2()); spite_temp_30; }); SpiteString spite_temp_31 = Console_Printable___call_to_string(spite_temp_29); Console_Printable___release(spite_temp_29); spite_temp_31; });
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
SpiteString spite_temp_32 = SpiteString___retain(file_);
SpiteString___release(self->file_name_);
self->file_name_ = spite_temp_32;
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
SpiteString Environment_setting(Environment* self, SpiteString name_, bool bare_allowed_) {
SpiteArguments arguments_ = spite_program_arguments;
SpiteString kebab_name_ = SpiteString_replace(name_, spite_lit_21, spite_lit_22);
SpiteString bare_flag_ = ({ SpiteString spite_temp_33 = kebab_name_; SpiteString spite_temp_34[] = {spite_lit_23, spite_temp_33}; SpiteString spite_temp_35 = spite_string_join(2, spite_temp_34); spite_temp_35; });
SpiteString flag_ = ({ SpiteString spite_temp_36 = bare_flag_; SpiteString spite_temp_37[] = {spite_temp_36, spite_lit_24}; SpiteString spite_temp_38 = spite_string_join(2, spite_temp_37); spite_temp_38; });
int32_t index_ = 0;
while (((index_ < ((int32_t)SpiteArguments_count(&(arguments_)))))) {
SpiteString argument_ = SpiteArguments_get(&(arguments_), index_);
if ((SpiteString_starts_with(argument_, SpiteString___retain(flag_)))) {
int32_t flag_length_ = SpiteString_length(flag_);
int32_t argument_length_ = SpiteString_length(argument_);
SpiteString spite_temp_39 = SpiteString_slice(argument_, flag_length_, argument_length_);
SpiteString___release(argument_);
SpiteString___release(flag_);
SpiteString___release(bare_flag_);
SpiteString___release(kebab_name_);
SpiteString___release(name_);
return spite_temp_39;
}
if ((({ SpiteString spite_temp_40 = argument_; SpiteString spite_temp_41 = bare_flag_; bool spite_temp_42 = SpiteString_equals(spite_temp_40, SpiteString___retain(spite_temp_41)); spite_temp_42; }))) {
if (!((bare_allowed_))) {
spite_failed_1(bare_allowed_, name_, kebab_name_, bare_flag_, flag_, index_, argument_, self);
}
SpiteString spite_temp_43 = spite_lit_25;
SpiteString___release(argument_);
SpiteString___release(flag_);
SpiteString___release(bare_flag_);
SpiteString___release(kebab_name_);
SpiteString___release(name_);
return spite_temp_43;
}
index_ = (index_ + 1);
SpiteString___release(argument_);
}
SpiteString upper_case_name_ = SpiteString_upper_case(name_);
SpiteString spite_temp_44 = Program_environment(self->_program_, SpiteString___retain(upper_case_name_));
SpiteString___release(upper_case_name_);
SpiteString___release(flag_);
SpiteString___release(bare_flag_);
SpiteString___release(kebab_name_);
SpiteString___release(name_);
return spite_temp_44;
}
static SPITE_CRASH_REPORT void spite_failed_1(bool bare_allowed_, SpiteString name_, SpiteString kebab_name_, SpiteString bare_flag_, SpiteString flag_, int32_t index_, SpiteString argument_, Environment* self) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_3(), stderr);
fputs("\tbare_allowed=", stderr);
{ SpiteString spite_temp_45 = SpiteBoolean_to_string(bare_allowed_); fwrite(spite_string_bytes(&spite_temp_45), 1, (size_t)spite_string_length(spite_temp_45), stderr); SpiteString___release(spite_temp_45); }
fputs("\tname=", stderr);
{ SpiteString spite_temp_46 = name_; spite_crash_text(spite_string_bytes(&spite_temp_46), spite_string_length(spite_temp_46)); }
fputs("\tkebab_name=", stderr);
{ SpiteString spite_temp_47 = kebab_name_; spite_crash_text(spite_string_bytes(&spite_temp_47), spite_string_length(spite_temp_47)); }
fputs("\tbare_flag=", stderr);
{ SpiteString spite_temp_48 = bare_flag_; spite_crash_text(spite_string_bytes(&spite_temp_48), spite_string_length(spite_temp_48)); }
fputs("\tflag=", stderr);
{ SpiteString spite_temp_49 = flag_; spite_crash_text(spite_string_bytes(&spite_temp_49), spite_string_length(spite_temp_49)); }
fputs("\tindex=", stderr);
{ SpiteString spite_temp_50 = SpiteInteger_to_string(index_); spite_crash_text(spite_string_bytes(&spite_temp_50), spite_string_length(spite_temp_50)); SpiteString___release(spite_temp_50); }
fputs("\targument=", stderr);
{ SpiteString spite_temp_51 = argument_; spite_crash_text(spite_string_bytes(&spite_temp_51), spite_string_length(spite_temp_51)); }
fputs("\tpieces=", stderr);
{ SpiteString spite_temp_52 = SpiteInteger_to_string(self->pieces_); spite_crash_text(spite_string_bytes(&spite_temp_52), spite_string_length(spite_temp_52)); SpiteString___release(spite_temp_52); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
void Environment_reject_misspelled_settings(Environment* self, List_String* declared_) {
SpiteArguments arguments_ = spite_program_arguments;
int32_t index_ = 0;
while (((index_ < ((int32_t)SpiteArguments_count(&(arguments_)))))) {
SpiteString argument_ = SpiteArguments_get(&(arguments_), index_);
Environment_reject_misspelled(self, SpiteString___retain(argument_), List_String___retain(declared_));
index_ = (index_ + 1);
SpiteString___release(argument_);
}
List_String___release(declared_);
}
void Environment_reject_misspelled(Environment* self, SpiteString argument_, List_String* declared_) {
if (!((SpiteString_starts_with(argument_, spite_lit_26)))) {
List_String___release(declared_);
SpiteString___release(argument_);
return;
}
SpiteString written_ = Environment_setting_written(self, SpiteString___retain(argument_));
if (!((SpiteString_contains(written_, spite_lit_27)))) {
SpiteString___release(written_);
List_String___release(declared_);
SpiteString___release(argument_);
return;
}
if (!((List_String_contains(declared_, SpiteString___retain(written_))))) {
SpiteString___release(written_);
List_String___release(declared_);
SpiteString___release(argument_);
return;
}
SpiteString kebab_name_ = SpiteString_replace(written_, spite_lit_28, spite_lit_29);
List_Console_Printable spite_framed_1; Console_Printable spite_framed_1_items[1]; int32_t spite_framed_1_count = 0;
Console_error(self->_console_, ({ spite_framed_1_items[0] = spite_tagged_object(0, spite_box_SpiteString(({ SpiteString spite_temp_53 = written_; SpiteString spite_temp_54 = kebab_name_; SpiteString spite_temp_55 = written_; SpiteString spite_temp_56[] = {spite_lit_30, spite_temp_53, spite_lit_31, spite_temp_54, spite_lit_32, spite_temp_55}; SpiteString spite_temp_57 = spite_string_join(6, spite_temp_56); spite_temp_57; }))); spite_framed_1_count = 1; List_Console_Printable___framed(&spite_framed_1, (int64_t)(intptr_t)spite_framed_1_items, 1); }));
for (int32_t spite_index = 0; spite_index < spite_framed_1_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_1_items[spite_index]); }
Program_exit(self->_program_, 1);
SpiteString___release(kebab_name_);
SpiteString___release(written_);
List_String___release(declared_);
SpiteString___release(argument_);
}
SpiteString Environment_setting_written(Environment* self, SpiteString argument_) {
int32_t argument_length_ = SpiteString_length(argument_);
int32_t equals_position_ = SpiteString_index_of(argument_, spite_lit_33);
if (((equals_position_ > 2))) {
SpiteString spite_temp_58 = SpiteString_slice(argument_, 2, equals_position_);
SpiteString___release(argument_);
return spite_temp_58;
}
SpiteString spite_temp_59 = SpiteString_slice(argument_, 2, argument_length_);
SpiteString___release(argument_);
return spite_temp_59;
}
int32_t Environment_integer_setting(Environment* self, SpiteString name_, int32_t declared_) {
SpiteString text_ = Environment_setting(self, SpiteString___retain(name_), false);
if ((!SPITE_STRING_IS_NULL(text_))) {
Nullable_Integer number_ = SpiteString_to_integer(text_);
if (!((number_).has_value)) {
spite_failed_2(name_, declared_, text_, self);
}
int32_t spite_temp_60 = (number_).value;
SpiteString___release(text_);
SpiteString___release(name_);
return spite_temp_60;
}
int32_t spite_temp_61 = declared_;
SpiteString___release(text_);
SpiteString___release(name_);
return spite_temp_61;
}
static SPITE_CRASH_REPORT void spite_failed_2(SpiteString name_, int32_t declared_, SpiteString text_, Environment* self) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_4(), stderr);
fputs("\tnumber is null", stderr);
fputs("\tname=", stderr);
{ SpiteString spite_temp_62 = name_; spite_crash_text(spite_string_bytes(&spite_temp_62), spite_string_length(spite_temp_62)); }
fputs("\tdeclared=", stderr);
{ SpiteString spite_temp_63 = SpiteInteger_to_string(declared_); spite_crash_text(spite_string_bytes(&spite_temp_63), spite_string_length(spite_temp_63)); SpiteString___release(spite_temp_63); }
fputs("\ttext=", stderr);
{ SpiteString spite_temp_64 = text_; spite_crash_text(spite_string_bytes(&spite_temp_64), spite_string_length(spite_temp_64)); }
fputs("\tpieces=", stderr);
{ SpiteString spite_temp_65 = SpiteInteger_to_string(self->pieces_); spite_crash_text(spite_string_bytes(&spite_temp_65), spite_string_length(spite_temp_65)); SpiteString___release(spite_temp_65); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
void Environment_Environment(Environment* self) {
List_String spite_framed_2; SpiteString spite_framed_2_items[1]; int32_t spite_framed_2_count = 0;
Environment_reject_misspelled_settings(self, ({ spite_framed_2_items[0] = spite_lit_34; spite_framed_2_count = 1; List_String___framed(&spite_framed_2, (int64_t)(intptr_t)spite_framed_2_items, 1); }));
for (int32_t spite_index = 0; spite_index < spite_framed_2_count; spite_index = spite_index + 1) { SpiteString___release(spite_framed_2_items[spite_index]); }
self->pieces_ = Environment_integer_setting(self, spite_lit_35, self->pieces_);
}
SpiteString SpiteInteger_to_string(int32_t self) {
int64_t wide_ = SpiteInteger_to_long(self);
SpiteString spite_temp_66 = SpiteLong_to_string(wide_);
return spite_temp_66;
}
SpiteString SpiteLong_to_string(int64_t self) {
if (((self == SpiteInteger_to_long(0)))) {
SpiteString spite_temp_67 = spite_lit_36;
return spite_temp_67;
}
Memory_Heap* heap_ = spite_singleton_Memory_Heap();
int64_t buffer_bytes_ = SpiteInteger_to_long(24);
int64_t spite_temp_68[32];
int64_t spite_temp_69 = buffer_bytes_;
int64_t address_ = spite_temp_69 <= 256 ? (int64_t)(intptr_t)spite_temp_68 : Memory_Heap_allocate(heap_, spite_temp_69);
int64_t position_ = buffer_bytes_;
int64_t rest_ = self;
while (((rest_ != SpiteInteger_to_long(0)))) {
int64_t digit_ = (rest_ % SpiteInteger_to_long(10));
if (((digit_ < SpiteInteger_to_long(0)))) {
digit_ = (-(digit_));
}
position_ = ({ int64_t spite_temp_70 = position_; int64_t spite_temp_71 = SpiteInteger_to_long(1); int64_t spite_temp_72; if (__builtin_expect(__builtin_sub_overflow(spite_temp_70, spite_temp_71, &spite_temp_72), 0)) spite_overflowed("position - 1", "a Long", "-", (int64_t)spite_temp_70, (int64_t)spite_temp_71, spite_site_5()); spite_temp_72; });
SpiteMemory_Address_write_byte(address_, position_, ({ int64_t spite_temp_73 = (digit_ + SpiteInteger_to_long(48)); if (__builtin_expect(spite_temp_73 < 0 || spite_temp_73 > UINT8_MAX, 0)) spite_narrowed((int64_t)spite_temp_73, "a Long", "a Byte", spite_site_6()); (uint8_t)spite_temp_73; }));
rest_ = (rest_ / SpiteInteger_to_long(10));
}
if (((self < SpiteInteger_to_long(0)))) {
position_ = ({ int64_t spite_temp_74 = position_; int64_t spite_temp_75 = SpiteInteger_to_long(1); int64_t spite_temp_76; if (__builtin_expect(__builtin_sub_overflow(spite_temp_74, spite_temp_75, &spite_temp_76), 0)) spite_overflowed("position - 1", "a Long", "-", (int64_t)spite_temp_74, (int64_t)spite_temp_75, spite_site_7()); spite_temp_76; });
SpiteMemory_Address_write_byte(address_, position_, SpiteInteger_to_byte(45));
}
int64_t first_digit_ = (address_ + ((int64_t)(position_)));
SpiteString text_ = SpiteMemory_Address_text(first_digit_, ({ int64_t spite_temp_77 = buffer_bytes_; int64_t spite_temp_78 = position_; int64_t spite_temp_79; if (__builtin_expect(__builtin_sub_overflow(spite_temp_77, spite_temp_78, &spite_temp_79), 0)) spite_overflowed("buffer_bytes - position", "a Long", "-", (int64_t)spite_temp_77, (int64_t)spite_temp_78, spite_site_8()); spite_temp_79; }));
if (address_ != (int64_t)(intptr_t)spite_temp_68) Memory_Heap_free(heap_, address_);
SpiteString spite_temp_80 = SpiteString___retain(text_);
SpiteString___release(text_);
Memory_Heap___release(heap_);
return spite_temp_80;
}
void Program_exit(Program* self, int32_t code_) {
Program__flush_output(self);
Program_exit_process(self, code_);
}
SpiteString Program_environment(Program* self, SpiteString name_) {
int64_t address_ = ((int64_t)(Program_environment_address(self, SpiteString___retain(name_))));
if (!(((address_ != ((int64_t)(0)))))) {
SpiteString___release(name_);
return SPITE_STRING_NULL;
}
SpiteString spite_temp_81 = SpiteMemory_Address_terminated_text(address_);
SpiteString___release(name_);
return spite_temp_81;
}
void Program_exit_process(Program* self, int32_t code_) {
(void)(({ spite_last_foreign_call = "exit\tlibrary=ucrtbase.dll\tfrom=library/windows/program.spite:9"; int32_t spite_temp_82 = ((int32_t (*)(int64_t))spite_foreign_2_41)((int64_t)(code_));  int32_t spite_foreign_result = spite_temp_82;  (void)spite_foreign_result; spite_temp_82; }));
}
int64_t Program_environment_address(Program* self, SpiteString name_) {
int64_t spite_temp_83 = ({ SpiteString spite_temp_84 = name_; spite_last_foreign_call = "getenv\tlibrary=ucrtbase.dll\tfrom=library/windows/program.spite:13"; int64_t spite_temp_85 = ((int64_t (*)(const char*))spite_foreign_2_42)(spite_string_bytes(&spite_temp_84));  int64_t spite_foreign_result = spite_temp_85;  (void)spite_foreign_result; spite_temp_85; });
SpiteString___release(name_);
return spite_temp_83;
}
void Program__flush_output(Program* self) {
fflush(stdout);
fflush(stderr);
}
int32_t SpiteString_length(SpiteString self) {
int32_t spite_temp_86 = ({ int64_t spite_temp_87 = spite_string_length(self); if (__builtin_expect(spite_temp_87 < INT32_MIN || spite_temp_87 > INT32_MAX, 0)) spite_narrowed((int64_t)spite_temp_87, "a Long", "an Integer", spite_site_9()); (int32_t)spite_temp_87; });
return spite_temp_86;
}
SpiteString SpiteString_slice(SpiteString self, int32_t start_, int32_t end_) {
int32_t first_ = SpiteString__clamped(self, start_);
int32_t last_ = SpiteString__clamped(self, end_);
if (((last_ <= first_))) {
SpiteString spite_temp_88 = spite_lit_37;
return spite_temp_88;
}
int64_t first_byte_ = (((int64_t)(intptr_t)spite_string_bytes(&(self))) + ((int64_t)(first_)));
SpiteString spite_temp_89 = SpiteMemory_Address_text(first_byte_, SpiteInteger_to_long(({ int32_t spite_temp_90 = last_; int32_t spite_temp_91 = first_; int32_t spite_temp_92; if (__builtin_expect(__builtin_sub_overflow(spite_temp_90, spite_temp_91, &spite_temp_92), 0)) spite_overflowed("last - first", "an Integer", "-", (int64_t)spite_temp_90, (int64_t)spite_temp_91, spite_site_10()); spite_temp_92; })));
return spite_temp_89;
}
int32_t SpiteString__clamped(SpiteString self, int32_t position_) {
if (((position_ < 0))) {
int32_t spite_temp_93 = 0;
return spite_temp_93;
}
if (((spite_string_length(self) < SpiteInteger_to_long(position_)))) {
int32_t spite_temp_94 = ({ int64_t spite_temp_95 = spite_string_length(self); if (__builtin_expect(spite_temp_95 < INT32_MIN || spite_temp_95 > INT32_MAX, 0)) spite_narrowed((int64_t)spite_temp_95, "a Long", "an Integer", spite_site_11()); (int32_t)spite_temp_95; });
return spite_temp_94;
}
int32_t spite_temp_96 = position_;
return spite_temp_96;
}
bool SpiteString_equals(SpiteString self, SpiteString other_) {
bool spite_temp_97 = (((spite_string_length(self) == spite_string_length(other_))) && ((((((int64_t)(intptr_t)spite_string_bytes(&(self))) == ((int64_t)(intptr_t)spite_string_bytes(&(other_))))) || ((SpiteMemory_Address_compare_bytes(((int64_t)(intptr_t)spite_string_bytes(&(self))), ((int64_t)(intptr_t)spite_string_bytes(&(other_))), spite_string_length(self)) == 0)))));
SpiteString___release(other_);
return spite_temp_97;
}
bool SpiteString_is_empty(SpiteString self) {
bool spite_temp_98 = (SpiteString_length(self) == 0);
return spite_temp_98;
}
SpiteString SpiteString_character_at(SpiteString self, int32_t index_) {
SpiteString spite_temp_99 = SpiteString_slice(self, index_, ({ int32_t spite_temp_100 = index_; int32_t spite_temp_101 = 1; int32_t spite_temp_102; if (__builtin_expect(__builtin_add_overflow(spite_temp_100, spite_temp_101, &spite_temp_102), 0)) spite_overflowed("index + 1", "an Integer", "+", (int64_t)spite_temp_100, (int64_t)spite_temp_101, spite_site_12()); spite_temp_102; }));
return spite_temp_99;
}
bool SpiteString_matches_at(SpiteString self, SpiteString text_, int32_t start_) {
if (((((start_ < 0)) || ((({ int32_t spite_temp_103 = start_; int32_t spite_temp_104 = SpiteString_length(text_); int32_t spite_temp_105; if (__builtin_expect(__builtin_add_overflow(spite_temp_103, spite_temp_104, &spite_temp_105), 0)) spite_overflowed("start + text.length()", "an Integer", "+", (int64_t)spite_temp_103, (int64_t)spite_temp_104, spite_site_13()); spite_temp_105; }) > SpiteString_length(self)))))) {
bool spite_temp_106 = false;
SpiteString___release(text_);
return spite_temp_106;
}
int32_t index_ = 0;
while (((index_ < SpiteString_length(text_)))) {
if (((SpiteString_code_at(self, ({ int32_t spite_temp_107 = start_; int32_t spite_temp_108 = index_; int32_t spite_temp_109; if (__builtin_expect(__builtin_add_overflow(spite_temp_107, spite_temp_108, &spite_temp_109), 0)) spite_overflowed("start + index", "an Integer", "+", (int64_t)spite_temp_107, (int64_t)spite_temp_108, spite_site_14()); spite_temp_109; })) != SpiteString_code_at(text_, index_)))) {
bool spite_temp_110 = false;
SpiteString___release(text_);
return spite_temp_110;
}
index_ = (index_ + 1);
}
bool spite_temp_111 = true;
SpiteString___release(text_);
return spite_temp_111;
}
int32_t SpiteString_index_of(SpiteString self, SpiteString text_) {
int32_t start_ = 0;
while (((({ int32_t spite_temp_112 = start_; int32_t spite_temp_113 = SpiteString_length(text_); int32_t spite_temp_114; if (__builtin_expect(__builtin_add_overflow(spite_temp_112, spite_temp_113, &spite_temp_114), 0)) spite_overflowed("start + text.length()", "an Integer", "+", (int64_t)spite_temp_112, (int64_t)spite_temp_113, spite_site_15()); spite_temp_114; }) <= SpiteString_length(self)))) {
if ((SpiteString_matches_at(self, SpiteString___retain(text_), start_))) {
int32_t spite_temp_115 = start_;
SpiteString___release(text_);
return spite_temp_115;
}
start_ = ({ int32_t spite_temp_116 = start_; int32_t spite_temp_117 = 1; int32_t spite_temp_118; if (__builtin_expect(__builtin_add_overflow(spite_temp_116, spite_temp_117, &spite_temp_118), 0)) spite_overflowed("start + 1", "an Integer", "+", (int64_t)spite_temp_116, (int64_t)spite_temp_117, spite_site_16()); spite_temp_118; });
}
int32_t spite_temp_119 = (-(1));
SpiteString___release(text_);
return spite_temp_119;
}
bool SpiteString_contains(SpiteString self, SpiteString text_) {
bool spite_temp_120 = (SpiteString_index_of(self, SpiteString___retain(text_)) >= 0);
SpiteString___release(text_);
return spite_temp_120;
}
bool SpiteString_starts_with(SpiteString self, SpiteString text_) {
bool spite_temp_121 = SpiteString_matches_at(self, SpiteString___retain(text_), 0);
SpiteString___release(text_);
return spite_temp_121;
}
List_String* SpiteString_split(SpiteString self, SpiteString separator_) {
List_String* pieces_ = List_String___make();
if ((SpiteString_is_empty(separator_))) {
int32_t character_index_ = 0;
while (((character_index_ < SpiteString_length(self)))) {
SpiteString character_ = SpiteString_character_at(self, character_index_);
List_String_append(pieces_, SpiteString___retain(character_));
character_index_ = (character_index_ + 1);
SpiteString___release(character_);
}
List_String* spite_temp_122 = List_String___retain(pieces_);
List_String___release(pieces_);
SpiteString___release(separator_);
return spite_temp_122;
}
int32_t piece_start_ = 0;
int32_t index_ = 0;
while (((index_ <= ({ int32_t spite_temp_123 = SpiteString_length(self); int32_t spite_temp_124 = SpiteString_length(separator_); int32_t spite_temp_125; if (__builtin_expect(__builtin_sub_overflow(spite_temp_123, spite_temp_124, &spite_temp_125), 0)) spite_overflowed("length() - separator.length()", "an Integer", "-", (int64_t)spite_temp_123, (int64_t)spite_temp_124, spite_site_17()); spite_temp_125; })))) {
if ((SpiteString_matches_at(self, SpiteString___retain(separator_), index_))) {
SpiteString piece_ = SpiteString_slice(self, piece_start_, index_);
List_String_append(pieces_, SpiteString___retain(piece_));
index_ = ({ int32_t spite_temp_126 = index_; int32_t spite_temp_127 = SpiteString_length(separator_); int32_t spite_temp_128; if (__builtin_expect(__builtin_add_overflow(spite_temp_126, spite_temp_127, &spite_temp_128), 0)) spite_overflowed("index + separator.length()", "an Integer", "+", (int64_t)spite_temp_126, (int64_t)spite_temp_127, spite_site_18()); spite_temp_128; });
piece_start_ = index_;
SpiteString___release(piece_);
}
else {
index_ = ({ int32_t spite_temp_129 = index_; int32_t spite_temp_130 = 1; int32_t spite_temp_131; if (__builtin_expect(__builtin_add_overflow(spite_temp_129, spite_temp_130, &spite_temp_131), 0)) spite_overflowed("index + 1", "an Integer", "+", (int64_t)spite_temp_129, (int64_t)spite_temp_130, spite_site_19()); spite_temp_131; });
}
}
int32_t own_length_ = SpiteString_length(self);
SpiteString rest_ = SpiteString_slice(self, piece_start_, own_length_);
List_String_append(pieces_, SpiteString___retain(rest_));
List_String* spite_temp_132 = List_String___retain(pieces_);
SpiteString___release(rest_);
List_String___release(pieces_);
SpiteString___release(separator_);
return spite_temp_132;
}
SpiteString SpiteString_replace(SpiteString self, SpiteString from_, SpiteString to_) {
if ((SpiteString_is_empty(from_))) {
int32_t own_length_ = SpiteString_length(self);
SpiteString spite_temp_133 = SpiteString_slice(self, 0, own_length_);
SpiteString___release(to_);
SpiteString___release(from_);
return spite_temp_133;
}
SpiteString spite_temp_134 = ({ List_String* spite_temp_135 = SpiteString_split(self, SpiteString___retain(from_)); SpiteString spite_temp_136 = List_String_join(spite_temp_135, SpiteString___retain(to_)); List_String___release(spite_temp_135); spite_temp_136; });
SpiteString___release(to_);
SpiteString___release(from_);
return spite_temp_134;
}
bool SpiteString_is_space_at(SpiteString self, int32_t index_) {
int32_t code_ = SpiteString_code_at(self, index_);
bool spite_temp_137 = (((code_ == 32)) || ((((code_ >= 9)) && ((code_ <= 13)))));
return spite_temp_137;
}
SpiteString SpiteString_upper_case(SpiteString self) {
SpiteString spite_temp_138 = SpiteString_shifted_case(self, 97, 122, (-(32)));
return spite_temp_138;
}
SpiteString SpiteString_shifted_case(SpiteString self, int32_t first_, int32_t last_, int32_t shift_) {
Memory_Heap* heap_ = spite_singleton_Memory_Heap();
int32_t own_length_ = SpiteString_length(self);
int64_t spite_temp_139[32];
int64_t spite_temp_140 = SpiteInteger_to_long(({ int32_t spite_temp_141 = own_length_; int32_t spite_temp_142 = 1; int32_t spite_temp_143; if (__builtin_expect(__builtin_add_overflow(spite_temp_141, spite_temp_142, &spite_temp_143), 0)) spite_overflowed("own_length + 1", "an Integer", "+", (int64_t)spite_temp_141, (int64_t)spite_temp_142, spite_site_20()); spite_temp_143; }));
int64_t address_ = spite_temp_140 <= 256 ? (int64_t)(intptr_t)spite_temp_139 : Memory_Heap_allocate(heap_, spite_temp_140);
int32_t index_ = 0;
while (((index_ < SpiteString_length(self)))) {
int32_t code_ = SpiteString_code_at(self, index_);
if ((((code_ >= first_))) && (((code_ <= last_)))) {
code_ = ({ int32_t spite_temp_144 = code_; int32_t spite_temp_145 = shift_; int32_t spite_temp_146; if (__builtin_expect(__builtin_add_overflow(spite_temp_144, spite_temp_145, &spite_temp_146), 0)) spite_overflowed("code + shift", "an Integer", "+", (int64_t)spite_temp_144, (int64_t)spite_temp_145, spite_site_21()); spite_temp_146; });
}
SpiteMemory_Address_write_byte(address_, SpiteInteger_to_long(index_), ({ int32_t spite_temp_147 = code_; if (__builtin_expect(spite_temp_147 < 0 || spite_temp_147 > UINT8_MAX, 0)) spite_narrowed((int64_t)spite_temp_147, "an Integer", "a Byte", spite_site_22()); (uint8_t)spite_temp_147; }));
index_ = (index_ + 1);
}
own_length_ = SpiteString_length(self);
SpiteString result_ = SpiteMemory_Address_text(address_, SpiteInteger_to_long(own_length_));
if (address_ != (int64_t)(intptr_t)spite_temp_139) Memory_Heap_free(heap_, address_);
SpiteString spite_temp_148 = SpiteString___retain(result_);
SpiteString___release(result_);
Memory_Heap___release(heap_);
return spite_temp_148;
}
SpiteString SpiteString_to_string(SpiteString self) {
SpiteString spite_temp_149 = SpiteString___retain(self);
return spite_temp_149;
}
Nullable_Long SpiteString_to_long(SpiteString self) {
int32_t index_ = SpiteString_first_non_space(self);
bool negative_ = false;
if ((((index_ < SpiteString_length(self)))) && (((((SpiteString_code_at(self, index_) == 45)) || ((SpiteString_code_at(self, index_) == 43)))))) {
negative_ = (SpiteString_code_at(self, index_) == 45);
index_ = (index_ + 1);
}
int32_t digits_start_ = index_;
int64_t value_ = SpiteInteger_to_long(0);
while (((((((index_ < SpiteString_length(self))) && ((SpiteString_code_at(self, index_) >= 48)))) && ((SpiteString_code_at(self, index_) <= 57))))) {
int64_t digit_ = SpiteInteger_to_long(({ int32_t spite_temp_150 = SpiteString_code_at(self, index_); int32_t spite_temp_151 = 48; int32_t spite_temp_152; if (__builtin_expect(__builtin_sub_overflow(spite_temp_150, spite_temp_151, &spite_temp_152), 0)) spite_overflowed("code_at(index) - 48", "an Integer", "-", (int64_t)spite_temp_150, (int64_t)spite_temp_151, spite_site_23()); spite_temp_152; }));
if (!(((value_ >= (({ int64_t spite_temp_153 = SpiteLong_smallest(); int64_t spite_temp_154 = digit_; int64_t spite_temp_155; if (__builtin_expect(__builtin_add_overflow(spite_temp_153, spite_temp_154, &spite_temp_155), 0)) spite_overflowed("Long.smallest + digit", "a Long", "+", (int64_t)spite_temp_153, (int64_t)spite_temp_154, spite_site_24()); spite_temp_155; }) / SpiteInteger_to_long(10)))))) {
return ((Nullable_Long){ .has_value = false, .value = 0 });
}
value_ = ({ int64_t spite_temp_156 = ({ int64_t spite_temp_157 = value_; int64_t spite_temp_158 = SpiteInteger_to_long(10); int64_t spite_temp_159; if (__builtin_expect(__builtin_mul_overflow(spite_temp_157, spite_temp_158, &spite_temp_159), 0)) spite_overflowed("value * 10", "a Long", "*", (int64_t)spite_temp_157, (int64_t)spite_temp_158, spite_site_25()); spite_temp_159; }); int64_t spite_temp_160 = digit_; int64_t spite_temp_161; if (__builtin_expect(__builtin_sub_overflow(spite_temp_156, spite_temp_160, &spite_temp_161), 0)) spite_overflowed("value * 10 - digit", "a Long", "-", (int64_t)spite_temp_156, (int64_t)spite_temp_160, spite_site_25()); spite_temp_161; });
index_ = (index_ + 1);
}
if (!(((index_ > digits_start_)))) {
return ((Nullable_Long){ .has_value = false, .value = 0 });
}
if (!((SpiteString_only_spaces_from(self, index_)))) {
return ((Nullable_Long){ .has_value = false, .value = 0 });
}
if ((negative_)) {
Nullable_Long spite_temp_162 = ((Nullable_Long){ .has_value = true, .value = value_ });
return spite_temp_162;
}
if (!(((value_ != SpiteLong_smallest())))) {
return ((Nullable_Long){ .has_value = false, .value = 0 });
}
Nullable_Long spite_temp_163 = ((Nullable_Long){ .has_value = true, .value = ({ int64_t spite_temp_164 = value_; int64_t spite_temp_165; if (__builtin_expect(__builtin_sub_overflow((int64_t)0, spite_temp_164, &spite_temp_165), 0)) spite_overflowed("-value", "a Long", "-", (int64_t)0, (int64_t)spite_temp_164, spite_site_26()); spite_temp_165; }) });
return spite_temp_163;
}
Nullable_Integer SpiteString_to_integer(SpiteString self) {
Nullable_Long value_ = SpiteString_to_long(self);
if (!((value_).has_value)) {
return ((Nullable_Integer){ .has_value = false, .value = 0 });
}
if (!((((value_).value >= SpiteInteger_to_long(SpiteInteger_smallest()))))) {
return ((Nullable_Integer){ .has_value = false, .value = 0 });
}
if (!((((value_).value <= SpiteInteger_to_long(SpiteInteger_largest()))))) {
return ((Nullable_Integer){ .has_value = false, .value = 0 });
}
Nullable_Integer spite_temp_166 = ((Nullable_Integer){ .has_value = true, .value = ({ int64_t spite_temp_167 = (value_).value; if (__builtin_expect(spite_temp_167 < INT32_MIN || spite_temp_167 > INT32_MAX, 0)) spite_narrowed((int64_t)spite_temp_167, "a Long", "an Integer", spite_site_27()); (int32_t)spite_temp_167; }) });
return spite_temp_166;
}
int32_t SpiteString_first_non_space(SpiteString self) {
int32_t index_ = 0;
while (((((index_ < SpiteString_length(self))) && (SpiteString_is_space_at(self, index_))))) {
index_ = (index_ + 1);
}
int32_t spite_temp_168 = index_;
return spite_temp_168;
}
bool SpiteString_only_spaces_from(SpiteString self, int32_t start_) {
int32_t index_ = start_;
while (((((index_ < SpiteString_length(self))) && (SpiteString_is_space_at(self, index_))))) {
index_ = (index_ + 1);
}
bool spite_temp_169 = (index_ == SpiteString_length(self));
return spite_temp_169;
}
SpiteString SpiteMemory_Address_text(int64_t self, int64_t length_) {
return spite_string_from_bytes((const char*)(intptr_t)self, length_);
}
SpiteString SpiteMemory_Address_terminated_text(int64_t self) {
int64_t length_ = SpiteInteger_to_long(0);
while (((SpiteMemory_Address_read_byte(self, length_) != SpiteInteger_to_byte(0)))) {
length_ = ({ int64_t spite_temp_170 = length_; int64_t spite_temp_171 = SpiteInteger_to_long(1); int64_t spite_temp_172; if (__builtin_expect(__builtin_add_overflow(spite_temp_170, spite_temp_171, &spite_temp_172), 0)) spite_overflowed("length + 1", "a Long", "+", (int64_t)spite_temp_170, (int64_t)spite_temp_171, spite_site_28()); spite_temp_172; });
}
SpiteString spite_temp_173 = SpiteMemory_Address_text(self, length_);
return spite_temp_173;
}
int64_t Memory_Arena_allocate(Memory_Arena* self, int64_t bytes_) {
int64_t rounded_ = ({ int64_t spite_temp_174 = (({ int64_t spite_temp_175 = bytes_; int64_t spite_temp_176 = SpiteInteger_to_long(15); int64_t spite_temp_177; if (__builtin_expect(__builtin_add_overflow(spite_temp_175, spite_temp_176, &spite_temp_177), 0)) spite_overflowed("bytes + 15", "a Long", "+", (int64_t)spite_temp_175, (int64_t)spite_temp_176, spite_site_29()); spite_temp_177; }) / SpiteInteger_to_long(16)); int64_t spite_temp_178 = SpiteInteger_to_long(16); int64_t spite_temp_179; if (__builtin_expect(__builtin_mul_overflow(spite_temp_174, spite_temp_178, &spite_temp_179), 0)) spite_overflowed("(bytes + 15) / 16 * 16", "a Long", "*", (int64_t)spite_temp_174, (int64_t)spite_temp_178, spite_site_29()); spite_temp_179; });
if (((((self->_block_ == ((int64_t)(0)))) || ((({ int64_t spite_temp_180 = self->_used_; int64_t spite_temp_181 = rounded_; int64_t spite_temp_182; if (__builtin_expect(__builtin_add_overflow(spite_temp_180, spite_temp_181, &spite_temp_182), 0)) spite_overflowed("_used + rounded", "a Long", "+", (int64_t)spite_temp_180, (int64_t)spite_temp_181, spite_site_30()); spite_temp_182; }) > self->_end_))))) {
Memory_Arena_start_block(self, rounded_);
}
int64_t address_ = (self->_block_ + ((int64_t)(self->_used_)));
self->_used_ = ({ int64_t spite_temp_183 = self->_used_; int64_t spite_temp_184 = rounded_; int64_t spite_temp_185; if (__builtin_expect(__builtin_add_overflow(spite_temp_183, spite_temp_184, &spite_temp_185), 0)) spite_overflowed("_used + rounded", "a Long", "+", (int64_t)spite_temp_183, (int64_t)spite_temp_184, spite_site_31()); spite_temp_185; });
int64_t spite_temp_186 = address_;
return spite_temp_186;
}
void Memory_Arena_free(Memory_Arena* self, int64_t _address_) {
}
void Memory_Arena_start_block(Memory_Arena* self, int64_t at_least_) {
int64_t size_ = self->_block_bytes_;
if (((({ int64_t spite_temp_187 = at_least_; int64_t spite_temp_188 = SpiteInteger_to_long(16); int64_t spite_temp_189; if (__builtin_expect(__builtin_add_overflow(spite_temp_187, spite_temp_188, &spite_temp_189), 0)) spite_overflowed("at_least + 16", "a Long", "+", (int64_t)spite_temp_187, (int64_t)spite_temp_188, spite_site_32()); spite_temp_189; }) > size_))) {
size_ = ({ int64_t spite_temp_190 = at_least_; int64_t spite_temp_191 = SpiteInteger_to_long(16); int64_t spite_temp_192; if (__builtin_expect(__builtin_add_overflow(spite_temp_190, spite_temp_191, &spite_temp_192), 0)) spite_overflowed("at_least + 16", "a Long", "+", (int64_t)spite_temp_190, (int64_t)spite_temp_191, spite_site_33()); spite_temp_192; });
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
void Naive_Naive(Naive* self) {
int64_t start_ = Clock_elapsed_nanoseconds(self->clock_);
SpiteString source_ = Naive_written(self, (self->environment_)->pieces_);
List_Token* tokens_ = Naive_tokenized(self, SpiteString___retain(source_));
int32_t identifiers_ = List_Token_count_identifier(tokens_);
int32_t numbers_ = List_Token_count_number(tokens_);
int32_t symbol_count_ = List_Token_count_symbol(tokens_);
int64_t number_total_ = Naive_total_of_numbers___held_0(self, tokens_);
int64_t checksums_ = List_Token_sum_checksum(tokens_);
int64_t microseconds_ = (({ int64_t spite_temp_193 = Clock_elapsed_nanoseconds(self->clock_); int64_t spite_temp_194 = start_; int64_t spite_temp_195; if (__builtin_expect(__builtin_sub_overflow(spite_temp_193, spite_temp_194, &spite_temp_195), 0)) spite_overflowed("clock.elapsed_nanoseconds() - start", "a Long", "-", (int64_t)spite_temp_193, (int64_t)spite_temp_194, spite_site_34()); spite_temp_195; }) / SpiteInteger_to_long(1000));
List_Console_Printable spite_framed_3; Console_Printable spite_framed_3_items[10]; int32_t spite_framed_3_count = 0;
Console_print(self->console_, ({ spite_framed_3_items[0] = spite_tagged_object(0, ((void*)&spite_lit_38_box)); spite_framed_3_items[1] = spite_tagged_SpiteInteger(identifiers_); spite_framed_3_items[2] = spite_tagged_object(0, ((void*)&spite_lit_39_box)); spite_framed_3_items[3] = spite_tagged_SpiteInteger(numbers_); spite_framed_3_items[4] = spite_tagged_object(0, ((void*)&spite_lit_40_box)); spite_framed_3_items[5] = spite_tagged_SpiteInteger(symbol_count_); spite_framed_3_items[6] = spite_tagged_object(0, ((void*)&spite_lit_41_box)); spite_framed_3_items[7] = spite_tagged_SpiteLong(number_total_); spite_framed_3_items[8] = spite_tagged_object(0, ((void*)&spite_lit_42_box)); spite_framed_3_items[9] = spite_tagged_SpiteLong(checksums_); spite_framed_3_count = 10; List_Console_Printable___framed(&spite_framed_3, (int64_t)(intptr_t)spite_framed_3_items, 10); }));
for (int32_t spite_index = 0; spite_index < spite_framed_3_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_3_items[spite_index]); }
List_Console_Printable spite_framed_4; Console_Printable spite_framed_4_items[1]; int32_t spite_framed_4_count = 0;
Console_error(self->console_, ({ spite_framed_4_items[0] = spite_tagged_object(0, spite_box_SpiteString(({ char spite_temp_196_digits[24]; SpiteString spite_temp_196 = SPITE_STATIC_STRING(spite_temp_196_digits, spite_long_digits(spite_temp_196_digits, (int64_t)(microseconds_))); SpiteString spite_temp_197[] = {spite_lit_43, spite_temp_196}; SpiteString spite_temp_198 = spite_string_join(2, spite_temp_197); spite_temp_198; }))); spite_framed_4_count = 1; List_Console_Printable___framed(&spite_framed_4, (int64_t)(intptr_t)spite_framed_4_items, 1); }));
for (int32_t spite_index = 0; spite_index < spite_framed_4_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_4_items[spite_index]); }
List_Token___release(tokens_);
SpiteString___release(source_);
}
SpiteString Naive_written(Naive* self, int32_t pieces_) {
SpiteString source_ = spite_lit_44;
int64_t seed_ = SpiteInteger_to_long(7);
int32_t index_ = 0;
while (((index_ < pieces_))) {
seed_ = ((seed_ * SpiteInteger_to_long(48271)) % SpiteInteger_to_long(2147483647));
int64_t choice_ = (seed_ % SpiteInteger_to_long(3));
int32_t picked_ = ({ int64_t spite_temp_199 = ((seed_ / SpiteInteger_to_long(3)) % SpiteInteger_to_long(8)); if (__builtin_expect(spite_temp_199 < INT32_MIN || spite_temp_199 > INT32_MAX, 0)) spite_narrowed((int64_t)spite_temp_199, "a Long", "an Integer", spite_site_35()); (int32_t)spite_temp_199; });
SpiteString piece_ = Naive_piece_of(self, choice_, picked_, seed_);
SpiteString spite_temp_200 = piece_;
source_ = SpiteString_append(source_, spite_temp_200);
SpiteString spite_temp_201 = spite_lit_45;
source_ = SpiteString_append(source_, spite_temp_201);
SpiteString___release(spite_temp_201);
if ((((index_ % 12) == 11))) {
SpiteString spite_temp_202 = spite_lit_46;
source_ = SpiteString_append(source_, spite_temp_202);
SpiteString___release(spite_temp_202);
}
index_ = (index_ + 1);
SpiteString___release(piece_);
}
SpiteString spite_temp_203 = SpiteString___retain(source_);
SpiteString___release(source_);
return spite_temp_203;
}
SpiteString Naive_piece_of(Naive* self, int64_t choice_, int32_t picked_, int64_t seed_) {
if (((choice_ == SpiteInteger_to_long(0)))) {
if (!(({ List_String* spite_temp_204 = self->words_; int32_t spite_temp_205 = picked_; (spite_temp_205 >= 0 && spite_temp_205 < (spite_temp_204)->item_count_) && (!SPITE_STRING_IS_NULL(((SpiteString*)(intptr_t)(spite_temp_204)->items_)[spite_temp_205])); }))) {
spite_failed_3(picked_, self, choice_, seed_);
}
SpiteString spite_temp_206 = List_String_get_at(self->words_, picked_);
return spite_temp_206;
}
if (((choice_ == SpiteInteger_to_long(1)))) {
int64_t number_ = ((seed_ / SpiteInteger_to_long(24)) % SpiteInteger_to_long(100000));
SpiteString spite_temp_207 = SpiteLong_to_string(number_);
return spite_temp_207;
}
int32_t symbol_index_ = (picked_ % 6);
if (!(({ List_String* spite_temp_208 = self->symbols_; int32_t spite_temp_209 = symbol_index_; (spite_temp_209 >= 0 && spite_temp_209 < (spite_temp_208)->item_count_) && (!SPITE_STRING_IS_NULL(((SpiteString*)(intptr_t)(spite_temp_208)->items_)[spite_temp_209])); }))) {
spite_failed_4(symbol_index_, self, choice_, picked_, seed_);
}
SpiteString spite_temp_210 = List_String_get_at(self->symbols_, symbol_index_);
return spite_temp_210;
}
static SPITE_CRASH_REPORT void spite_failed_3(int32_t picked_, Naive* self, int64_t choice_, int64_t seed_) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_36(), stderr);
{
fputs("\twords[picked] is missing: index ", stderr);
{ SpiteString spite_temp_211 = SpiteInteger_to_string(picked_); fwrite(spite_string_bytes(&spite_temp_211), 1, (size_t)spite_string_length(spite_temp_211), stderr); SpiteString___release(spite_temp_211); }
fputs(", count ", stderr);
{ SpiteString spite_temp_212 = SpiteInteger_to_string(((self->words_)->item_count_)); fwrite(spite_string_bytes(&spite_temp_212), 1, (size_t)spite_string_length(spite_temp_212), stderr); SpiteString___release(spite_temp_212); }
}
fputs("\tchoice=", stderr);
{ SpiteString spite_temp_213 = SpiteLong_to_string(choice_); spite_crash_text(spite_string_bytes(&spite_temp_213), spite_string_length(spite_temp_213)); SpiteString___release(spite_temp_213); }
fputs("\tseed=", stderr);
{ SpiteString spite_temp_214 = SpiteLong_to_string(seed_); spite_crash_text(spite_string_bytes(&spite_temp_214), spite_string_length(spite_temp_214)); SpiteString___release(spite_temp_214); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
static SPITE_CRASH_REPORT void spite_failed_4(int32_t symbol_index_, Naive* self, int64_t choice_, int32_t picked_, int64_t seed_) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_37(), stderr);
{
fputs("\tsymbols[symbol_index] is missing: index ", stderr);
{ SpiteString spite_temp_215 = SpiteInteger_to_string(symbol_index_); fwrite(spite_string_bytes(&spite_temp_215), 1, (size_t)spite_string_length(spite_temp_215), stderr); SpiteString___release(spite_temp_215); }
fputs(", count ", stderr);
{ SpiteString spite_temp_216 = SpiteInteger_to_string(((self->symbols_)->item_count_)); fwrite(spite_string_bytes(&spite_temp_216), 1, (size_t)spite_string_length(spite_temp_216), stderr); SpiteString___release(spite_temp_216); }
}
fputs("\tchoice=", stderr);
{ SpiteString spite_temp_217 = SpiteLong_to_string(choice_); spite_crash_text(spite_string_bytes(&spite_temp_217), spite_string_length(spite_temp_217)); SpiteString___release(spite_temp_217); }
fputs("\tpicked=", stderr);
{ SpiteString spite_temp_218 = SpiteInteger_to_string(picked_); spite_crash_text(spite_string_bytes(&spite_temp_218), spite_string_length(spite_temp_218)); SpiteString___release(spite_temp_218); }
fputs("\tseed=", stderr);
{ SpiteString spite_temp_219 = SpiteLong_to_string(seed_); spite_crash_text(spite_string_bytes(&spite_temp_219), spite_string_length(spite_temp_219)); SpiteString___release(spite_temp_219); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
List_Token* Naive_tokenized(Naive* self, SpiteString source_) {
List_Token* tokens_ = List_Token___make();
int32_t length_ = SpiteString_length(source_);
int32_t line_ = 1;
int32_t position_ = 0;
while (((position_ < length_))) {
int32_t code_ = SpiteString_code_at(source_, position_);
int32_t first_ = position_;
if (((code_ == 10))) {
line_ = ({ int32_t spite_temp_220 = line_; int32_t spite_temp_221 = 1; int32_t spite_temp_222; if (__builtin_expect(__builtin_add_overflow(spite_temp_220, spite_temp_221, &spite_temp_222), 0)) spite_overflowed("line + 1", "an Integer", "+", (int64_t)spite_temp_220, (int64_t)spite_temp_221, spite_site_38()); spite_temp_222; });
position_ = (position_ + 1);
}
else {
if (((code_ == 32))) {
position_ = ({ int32_t spite_temp_223 = position_; int32_t spite_temp_224 = 1; int32_t spite_temp_225; if (__builtin_expect(__builtin_add_overflow(spite_temp_223, spite_temp_224, &spite_temp_225), 0)) spite_overflowed("position + 1", "an Integer", "+", (int64_t)spite_temp_223, (int64_t)spite_temp_224, spite_site_39()); spite_temp_225; });
}
else {
if ((((code_ >= 97))) && (((code_ <= 122)))) {
int64_t hash_ = SpiteInteger_to_long(0);
while (((((((position_ < length_)) && ((SpiteString_code_at(source_, position_) >= 97)))) && ((SpiteString_code_at(source_, position_) <= 122))))) {
hash_ = (((hash_ * SpiteInteger_to_long(31)) + SpiteInteger_to_long(SpiteString_code_at(source_, position_))) % SpiteInteger_to_long(1000000007));
position_ = (position_ + 1);
}
Token* identifier_ = Token___make(Token_TokenKind_identifier, first_, ({ int32_t spite_temp_226 = position_; int32_t spite_temp_227 = first_; int32_t spite_temp_228; if (__builtin_expect(__builtin_sub_overflow(spite_temp_226, spite_temp_227, &spite_temp_228), 0)) spite_overflowed("position - first", "an Integer", "-", (int64_t)spite_temp_226, (int64_t)spite_temp_227, spite_site_40()); spite_temp_228; }), line_, hash_);
List_Token_append(tokens_, Token___retain(identifier_));
Token___release(identifier_);
}
else {
if ((((code_ >= 48))) && (((code_ <= 57)))) {
int64_t number_ = SpiteInteger_to_long(0);
while (((((((position_ < length_)) && ((SpiteString_code_at(source_, position_) >= 48)))) && ((SpiteString_code_at(source_, position_) <= 57))))) {
number_ = ({ int64_t spite_temp_229 = ({ int64_t spite_temp_230 = ({ int64_t spite_temp_231 = number_; int64_t spite_temp_232 = SpiteInteger_to_long(10); int64_t spite_temp_233; if (__builtin_expect(__builtin_mul_overflow(spite_temp_231, spite_temp_232, &spite_temp_233), 0)) spite_overflowed("number * 10", "a Long", "*", (int64_t)spite_temp_231, (int64_t)spite_temp_232, spite_site_41()); spite_temp_233; }); int64_t spite_temp_234 = SpiteInteger_to_long(SpiteString_code_at(source_, position_)); int64_t spite_temp_235; if (__builtin_expect(__builtin_add_overflow(spite_temp_230, spite_temp_234, &spite_temp_235), 0)) spite_overflowed("number * 10 + source.code_at(position)", "a Long", "+", (int64_t)spite_temp_230, (int64_t)spite_temp_234, spite_site_41()); spite_temp_235; }); int64_t spite_temp_236 = SpiteInteger_to_long(48); int64_t spite_temp_237; if (__builtin_expect(__builtin_sub_overflow(spite_temp_229, spite_temp_236, &spite_temp_237), 0)) spite_overflowed("number * 10 + source.code_at(position) - 48", "a Long", "-", (int64_t)spite_temp_229, (int64_t)spite_temp_236, spite_site_41()); spite_temp_237; });
position_ = (position_ + 1);
}
Token* digits_ = Token___make(Token_TokenKind_number, first_, ({ int32_t spite_temp_238 = position_; int32_t spite_temp_239 = first_; int32_t spite_temp_240; if (__builtin_expect(__builtin_sub_overflow(spite_temp_238, spite_temp_239, &spite_temp_240), 0)) spite_overflowed("position - first", "an Integer", "-", (int64_t)spite_temp_238, (int64_t)spite_temp_239, spite_site_42()); spite_temp_240; }), line_, number_);
List_Token_append(tokens_, Token___retain(digits_));
Token___release(digits_);
}
else {
position_ = ({ int32_t spite_temp_241 = position_; int32_t spite_temp_242 = 1; int32_t spite_temp_243; if (__builtin_expect(__builtin_add_overflow(spite_temp_241, spite_temp_242, &spite_temp_243), 0)) spite_overflowed("position + 1", "an Integer", "+", (int64_t)spite_temp_241, (int64_t)spite_temp_242, spite_site_43()); spite_temp_243; });
Token* symbol_ = Token___make(Token_TokenKind_symbol, first_, 1, line_, SpiteInteger_to_long(code_));
List_Token_append(tokens_, Token___retain(symbol_));
Token___release(symbol_);
}
}
}
}
}
List_Token* spite_temp_244 = List_Token___retain(tokens_);
List_Token___release(tokens_);
SpiteString___release(source_);
return spite_temp_244;
}
int64_t Naive_total_of_numbers___held_0(Naive* self, List_Token* tokens_) {
int64_t total_ = SpiteInteger_to_long(0);
int32_t index_ = 0;
while (((index_ < spite_folded_List_Token_count(tokens_)))) {
Token* token_ = ({ List_Token* spite_temp_245 = tokens_; int32_t spite_temp_246 = index_; if (__builtin_expect(spite_temp_246 < 0 || spite_temp_246 >= (spite_temp_245)->item_count_, 0)) spite_outside_list("tokens[index]", spite_site_44()); ((Token**)(intptr_t)(spite_temp_245)->items_)[spite_temp_246]; });
if ((((token_)->kind_ == Token_TokenKind_number))) {
total_ = ({ int64_t spite_temp_247 = total_; int64_t spite_temp_248 = (token_)->value_; int64_t spite_temp_249; if (__builtin_expect(__builtin_add_overflow(spite_temp_247, spite_temp_248, &spite_temp_249), 0)) spite_overflowed("total + token.value", "a Long", "+", (int64_t)spite_temp_247, (int64_t)spite_temp_248, spite_site_45()); spite_temp_249; });
}
index_ = (index_ + 1);
}
int64_t spite_temp_250 = total_;
return spite_temp_250;
}
void Token_Token(Token* self, Token_TokenKind made_kind_, int32_t made_start_, int32_t made_length_, int32_t made_line_, int64_t made_value_) {
self->kind_ = made_kind_;
self->start_ = made_start_;
self->length_ = made_length_;
self->line_ = made_line_;
self->value_ = made_value_;
}
int64_t Token_checksum(Token* self) {
int64_t total_ = SpiteInteger_to_long(self->start_);
total_ = ((total_ + SpiteInteger_to_long(({ int32_t spite_temp_251 = self->length_; int32_t spite_temp_252 = 7; int32_t spite_temp_253; if (__builtin_expect(__builtin_mul_overflow(spite_temp_251, spite_temp_252, &spite_temp_253), 0)) spite_overflowed("length * 7", "an Integer", "*", (int64_t)spite_temp_251, (int64_t)spite_temp_252, spite_site_46()); spite_temp_253; }))) + SpiteInteger_to_long(({ int32_t spite_temp_254 = self->line_; int32_t spite_temp_255 = 13; int32_t spite_temp_256; if (__builtin_expect(__builtin_mul_overflow(spite_temp_254, spite_temp_255, &spite_temp_256), 0)) spite_overflowed("line * 13", "an Integer", "*", (int64_t)spite_temp_254, (int64_t)spite_temp_255, spite_site_46()); spite_temp_256; })));
total_ = (total_ + (self->value_ % SpiteInteger_to_long(1000)));
int64_t spite_temp_257 = total_;
return spite_temp_257;
}
int32_t List_String_count(List_String* self) {
int32_t spite_temp_258 = self->item_count_;
return spite_temp_258;
}
void List_String_append(List_String* self, SpiteString value_) {
List_String_make_room(self);
TypedMemory__String_write_value(self->values_, self->items_, self->item_count_, SpiteString___retain(value_));
self->item_count_ = ({ int32_t spite_temp_259 = self->item_count_; int32_t spite_temp_260 = 1; int32_t spite_temp_261; if (__builtin_expect(__builtin_add_overflow(spite_temp_259, spite_temp_260, &spite_temp_261), 0)) spite_overflowed("item_count + 1", "an Integer", "+", (int64_t)spite_temp_259, (int64_t)spite_temp_260, spite_site_47()); spite_temp_261; });
SpiteString___release(value_);
}
SpiteString List_String_get_at(List_String* self, int32_t index_) {
if ((((index_ >= 0))) && (((index_ < self->item_count_)))) {
SpiteString spite_temp_262 = TypedMemory__String_read_value(self->values_, self->items_, index_);
return spite_temp_262;
}
SpiteString spite_temp_263 = SPITE_STRING_NULL;
return spite_temp_263;
}
void List_String_clear(List_String* self) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
TypedMemory__String_release_value(self->values_, self->items_, index_);
index_ = (index_ + 1);
}
self->item_count_ = 0;
}
bool List_String_contains(List_String* self, SpiteString value_) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
if ((({ SpiteString spite_temp_264 = TypedMemory__String_read_value(self->values_, self->items_, index_); SpiteString spite_temp_265 = value_; bool spite_temp_266 = SpiteString_equals(spite_temp_264, SpiteString___retain(spite_temp_265)); SpiteString___release(spite_temp_264); spite_temp_266; }))) {
bool spite_temp_267 = true;
SpiteString___release(value_);
return spite_temp_267;
}
index_ = (index_ + 1);
}
bool spite_temp_268 = false;
SpiteString___release(value_);
return spite_temp_268;
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
total_ = ({ int64_t spite_temp_269 = total_; int64_t spite_temp_270 = SpiteInteger_to_long(({ int32_t spite_temp_271 = SpiteString_length(separator_); int32_t spite_temp_272 = ({ int32_t spite_temp_273 = self->item_count_; int32_t spite_temp_274 = 1; int32_t spite_temp_275; if (__builtin_expect(__builtin_sub_overflow(spite_temp_273, spite_temp_274, &spite_temp_275), 0)) spite_overflowed("item_count - 1", "an Integer", "-", (int64_t)spite_temp_273, (int64_t)spite_temp_274, spite_site_48()); spite_temp_275; }); int32_t spite_temp_276; if (__builtin_expect(__builtin_mul_overflow(spite_temp_271, spite_temp_272, &spite_temp_276), 0)) spite_overflowed("separator.length() * (item_count - 1)", "an Integer", "*", (int64_t)spite_temp_271, (int64_t)spite_temp_272, spite_site_48()); spite_temp_276; })); int64_t spite_temp_277; if (__builtin_expect(__builtin_add_overflow(spite_temp_269, spite_temp_270, &spite_temp_277), 0)) spite_overflowed("total + separator.length() * (item_count - 1)", "a Long", "+", (int64_t)spite_temp_269, (int64_t)spite_temp_270, spite_site_48()); spite_temp_277; });
}
int64_t spite_temp_278[32];
int64_t spite_temp_279 = ({ int64_t spite_temp_280 = total_; int64_t spite_temp_281 = SpiteInteger_to_long(1); int64_t spite_temp_282; if (__builtin_expect(__builtin_add_overflow(spite_temp_280, spite_temp_281, &spite_temp_282), 0)) spite_overflowed("total + 1", "a Long", "+", (int64_t)spite_temp_280, (int64_t)spite_temp_281, spite_site_49()); spite_temp_282; });
int64_t address_ = spite_temp_279 <= 256 ? (int64_t)(intptr_t)spite_temp_278 : Memory_Heap_allocate(self->heap_, spite_temp_279);
int64_t position_ = SpiteInteger_to_long(0);
index_ = 0;
while (((index_ < List_String_count(pieces_)))) {
if (((index_ > 0))) {
position_ = List_String_write_text(self, SpiteString___retain(separator_), address_, position_);
}
SpiteString next_piece_ = ({ SpiteString spite_temp_283 = List_String_get_at(pieces_, index_); if (__builtin_expect(!((!SPITE_STRING_IS_NULL(spite_temp_283))), 0)) spite_outside_list("pieces[index]", spite_site_50()); spite_temp_283; });
position_ = List_String_write_text(self, SpiteString___retain(next_piece_), address_, position_);
index_ = (index_ + 1);
SpiteString___release(next_piece_);
}
SpiteString joined_ = SpiteMemory_Address_text(address_, total_);
if (address_ != (int64_t)(intptr_t)spite_temp_278) Memory_Heap_free(self->heap_, address_);
SpiteString spite_temp_284 = SpiteString___retain(joined_);
SpiteString___release(joined_);
List_String___release(pieces_);
SpiteString___release(separator_);
return spite_temp_284;
}
int64_t List_String_write_text(List_String* self, SpiteString text_, int64_t address_, int64_t position_) {
int32_t index_ = 0;
while (((index_ < SpiteString_length(text_)))) {
int32_t code_ = SpiteString_code_at(text_, index_);
SpiteMemory_Address_write_byte(address_, ({ int64_t spite_temp_285 = position_; int64_t spite_temp_286 = SpiteInteger_to_long(index_); int64_t spite_temp_287; if (__builtin_expect(__builtin_add_overflow(spite_temp_285, spite_temp_286, &spite_temp_287), 0)) spite_overflowed("position + index", "a Long", "+", (int64_t)spite_temp_285, (int64_t)spite_temp_286, spite_site_51()); spite_temp_287; }), ({ int32_t spite_temp_288 = code_; if (__builtin_expect(spite_temp_288 < 0 || spite_temp_288 > UINT8_MAX, 0)) spite_narrowed((int64_t)spite_temp_288, "an Integer", "a Byte", spite_site_51()); (uint8_t)spite_temp_288; }));
index_ = (index_ + 1);
}
int64_t spite_temp_289 = ({ int64_t spite_temp_290 = position_; int64_t spite_temp_291 = SpiteInteger_to_long(SpiteString_length(text_)); int64_t spite_temp_292; if (__builtin_expect(__builtin_add_overflow(spite_temp_290, spite_temp_291, &spite_temp_292), 0)) spite_overflowed("position + text.length()", "a Long", "+", (int64_t)spite_temp_290, (int64_t)spite_temp_291, spite_site_52()); spite_temp_292; });
SpiteString___release(text_);
return spite_temp_289;
}
void List_String_drop(List_String* self) {
List_String_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_293 = SPITE_ALLOCATOR_List_String(self, spite_singleton_Memory_Heap); int64_t spite_temp_294 = self->items_; if (((SpiteHeader*)(spite_temp_293))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_293), spite_temp_294); } else if (((SpiteHeader*)(spite_temp_293))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_293), spite_temp_294); } });
}
}
void List_String_make_room(List_String* self) {
if (((self->item_count_ == self->capacity_))) {
List_String__grow(self);
}
}
void List_String__grow(List_String* self) {
int32_t grown_ = ({ int32_t spite_temp_295 = self->capacity_; int32_t spite_temp_296 = 2; int32_t spite_temp_297; if (__builtin_expect(__builtin_mul_overflow(spite_temp_295, spite_temp_296, &spite_temp_297), 0)) spite_overflowed("capacity * 2", "an Integer", "*", (int64_t)spite_temp_295, (int64_t)spite_temp_296, spite_site_53()); spite_temp_297; });
if (((self->capacity_ == 0))) {
grown_ = 4;
}
int64_t bytes_ = TypedMemory__String_value_bytes(self->values_);
self->items_ = List_String__resized(self, ({ int64_t spite_temp_298 = bytes_; int64_t spite_temp_299 = SpiteInteger_to_long(self->capacity_); int64_t spite_temp_300; if (__builtin_expect(__builtin_mul_overflow(spite_temp_298, spite_temp_299, &spite_temp_300), 0)) spite_overflowed("bytes * capacity", "a Long", "*", (int64_t)spite_temp_298, (int64_t)spite_temp_299, spite_site_54()); spite_temp_300; }), ({ int64_t spite_temp_301 = bytes_; int64_t spite_temp_302 = SpiteInteger_to_long(grown_); int64_t spite_temp_303; if (__builtin_expect(__builtin_mul_overflow(spite_temp_301, spite_temp_302, &spite_temp_303), 0)) spite_overflowed("bytes * grown", "a Long", "*", (int64_t)spite_temp_301, (int64_t)spite_temp_302, spite_site_54()); spite_temp_303; }));
self->capacity_ = grown_;
}
int64_t List_String__resized(List_String* self, int64_t old_bytes_, int64_t new_bytes_) {
if ((({ Spite_Allocator spite_temp_304 = SPITE_ALLOCATOR_List_String(self, spite_singleton_Memory_Heap); bool spite_temp_305 = (((SpiteHeader*)(spite_temp_304))->class_id == 94); spite_temp_305; }))) {
int64_t spite_temp_306 = Memory_Heap_resize(self->heap_, self->items_, new_bytes_);
return spite_temp_306;
}
int64_t moved_ = ({ Spite_Allocator spite_temp_307 = SPITE_ALLOCATOR_List_String(self, spite_singleton_Memory_Heap); int64_t spite_temp_308 = new_bytes_; int64_t spite_temp_309 = 0; if (((SpiteHeader*)(spite_temp_307))->class_id == 93) { spite_temp_309 = Memory_Arena_allocate(((Memory_Arena*)spite_temp_307), spite_temp_308); } else if (((SpiteHeader*)(spite_temp_307))->class_id == 94) { spite_temp_309 = Memory_Heap_allocate(((Memory_Heap*)spite_temp_307), spite_temp_308); } spite_temp_309; });
if (((self->items_ != ((int64_t)(0))))) {
SpiteMemory_Address_copy_to(self->items_, moved_, old_bytes_);
({ Spite_Allocator spite_temp_310 = SPITE_ALLOCATOR_List_String(self, spite_singleton_Memory_Heap); int64_t spite_temp_311 = self->items_; if (((SpiteHeader*)(spite_temp_310))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_310), spite_temp_311); } else if (((SpiteHeader*)(spite_temp_310))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_310), spite_temp_311); } });
}
int64_t spite_temp_312 = moved_;
return spite_temp_312;
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
Console_Printable List_Console_Printable_get_at(List_Console_Printable* self, int32_t index_) {
if ((((index_ >= 0))) && (((index_ < self->item_count_)))) {
Console_Printable spite_temp_313 = TypedMemory__Console_Printable_read_value(self->values_, self->items_, index_);
return spite_temp_313;
}
Console_Printable spite_temp_314 = SPITE_TAGGED_NULL;
return spite_temp_314;
}
void List_Console_Printable_drop(List_Console_Printable* self) {
List_Console_Printable_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_315 = SPITE_ALLOCATOR_List_Console_Printable(self, spite_singleton_Memory_Heap); int64_t spite_temp_316 = self->items_; if (((SpiteHeader*)(spite_temp_315))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_315), spite_temp_316); } else if (((SpiteHeader*)(spite_temp_315))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_315), spite_temp_316); } });
}
}
Console_Printable TypedMemory__Console_Printable_read_value(TypedMemory__Console_Printable* self, int64_t address_, int32_t index_) {
return Console_Printable___retain(((Console_Printable*)(intptr_t)address_)[index_]);
}
void List_Token_append(List_Token* self, Token* value_) {
List_Token_make_room(self);
TypedMemory__Token_write_value(self->values_, self->items_, self->item_count_, Token___retain(value_));
self->item_count_ = ({ int32_t spite_temp_317 = self->item_count_; int32_t spite_temp_318 = 1; int32_t spite_temp_319; if (__builtin_expect(__builtin_add_overflow(spite_temp_317, spite_temp_318, &spite_temp_319), 0)) spite_overflowed("item_count + 1", "an Integer", "+", (int64_t)spite_temp_317, (int64_t)spite_temp_318, spite_site_47()); spite_temp_319; });
Token___release(value_);
}
void List_Token_drop(List_Token* self) {
List_Token_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_320 = SPITE_ALLOCATOR_List_Token(self, spite_singleton_Memory_Heap); int64_t spite_temp_321 = self->items_; if (((SpiteHeader*)(spite_temp_320))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_320), spite_temp_321); } else if (((SpiteHeader*)(spite_temp_320))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_320), spite_temp_321); } });
}
}
void List_Token_make_room(List_Token* self) {
if (((self->item_count_ == self->capacity_))) {
List_Token__grow(self);
}
}
void List_Token__grow(List_Token* self) {
int32_t grown_ = ({ int32_t spite_temp_322 = self->capacity_; int32_t spite_temp_323 = 2; int32_t spite_temp_324; if (__builtin_expect(__builtin_mul_overflow(spite_temp_322, spite_temp_323, &spite_temp_324), 0)) spite_overflowed("capacity * 2", "an Integer", "*", (int64_t)spite_temp_322, (int64_t)spite_temp_323, spite_site_53()); spite_temp_324; });
if (((self->capacity_ == 0))) {
grown_ = 4;
}
int64_t bytes_ = TypedMemory__Token_value_bytes(self->values_);
self->items_ = List_Token__resized(self, ({ int64_t spite_temp_325 = bytes_; int64_t spite_temp_326 = SpiteInteger_to_long(self->capacity_); int64_t spite_temp_327; if (__builtin_expect(__builtin_mul_overflow(spite_temp_325, spite_temp_326, &spite_temp_327), 0)) spite_overflowed("bytes * capacity", "a Long", "*", (int64_t)spite_temp_325, (int64_t)spite_temp_326, spite_site_54()); spite_temp_327; }), ({ int64_t spite_temp_328 = bytes_; int64_t spite_temp_329 = SpiteInteger_to_long(grown_); int64_t spite_temp_330; if (__builtin_expect(__builtin_mul_overflow(spite_temp_328, spite_temp_329, &spite_temp_330), 0)) spite_overflowed("bytes * grown", "a Long", "*", (int64_t)spite_temp_328, (int64_t)spite_temp_329, spite_site_54()); spite_temp_330; }));
self->capacity_ = grown_;
}
int64_t List_Token__resized(List_Token* self, int64_t old_bytes_, int64_t new_bytes_) {
if ((({ Spite_Allocator spite_temp_331 = SPITE_ALLOCATOR_List_Token(self, spite_singleton_Memory_Heap); bool spite_temp_332 = (((SpiteHeader*)(spite_temp_331))->class_id == 94); spite_temp_332; }))) {
int64_t spite_temp_333 = Memory_Heap_resize(self->heap_, self->items_, new_bytes_);
return spite_temp_333;
}
int64_t moved_ = ({ Spite_Allocator spite_temp_334 = SPITE_ALLOCATOR_List_Token(self, spite_singleton_Memory_Heap); int64_t spite_temp_335 = new_bytes_; int64_t spite_temp_336 = 0; if (((SpiteHeader*)(spite_temp_334))->class_id == 93) { spite_temp_336 = Memory_Arena_allocate(((Memory_Arena*)spite_temp_334), spite_temp_335); } else if (((SpiteHeader*)(spite_temp_334))->class_id == 94) { spite_temp_336 = Memory_Heap_allocate(((Memory_Heap*)spite_temp_334), spite_temp_335); } spite_temp_336; });
if (((self->items_ != ((int64_t)(0))))) {
SpiteMemory_Address_copy_to(self->items_, moved_, old_bytes_);
({ Spite_Allocator spite_temp_337 = SPITE_ALLOCATOR_List_Token(self, spite_singleton_Memory_Heap); int64_t spite_temp_338 = self->items_; if (((SpiteHeader*)(spite_temp_337))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_337), spite_temp_338); } else if (((SpiteHeader*)(spite_temp_337))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_337), spite_temp_338); } });
}
int64_t spite_temp_339 = moved_;
return spite_temp_339;
}
int32_t List_Token_count_identifier(List_Token* self) {
int32_t counted_ = 0;
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
Token* item_ = ((Token**)(intptr_t)self->items_)[index_];
if ((((item_)->kind_ == Token_TokenKind_identifier))) {
counted_ = (counted_ + 1);
}
index_ = (index_ + 1);
}
int32_t spite_temp_340 = counted_;
return spite_temp_340;
}
int32_t List_Token_count_number(List_Token* self) {
int32_t counted_ = 0;
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
Token* item_ = ((Token**)(intptr_t)self->items_)[index_];
if ((((item_)->kind_ == Token_TokenKind_number))) {
counted_ = (counted_ + 1);
}
index_ = (index_ + 1);
}
int32_t spite_temp_341 = counted_;
return spite_temp_341;
}
int32_t List_Token_count_symbol(List_Token* self) {
int32_t counted_ = 0;
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
Token* item_ = ((Token**)(intptr_t)self->items_)[index_];
if ((((item_)->kind_ == Token_TokenKind_symbol))) {
counted_ = (counted_ + 1);
}
index_ = (index_ + 1);
}
int32_t spite_temp_342 = counted_;
return spite_temp_342;
}
int64_t List_Token_sum_checksum(List_Token* self) {
int64_t total_ = SpiteInteger_to_long(0);
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
Token* item_ = ((Token**)(intptr_t)self->items_)[index_];
total_ = ({ int64_t spite_temp_343 = total_; int64_t spite_temp_344 = Token_checksum(item_); int64_t spite_temp_345; if (__builtin_expect(__builtin_add_overflow(spite_temp_343, spite_temp_344, &spite_temp_345), 0)) spite_overflowed("total + item.attributes[member]", "a Long", "+", (int64_t)spite_temp_343, (int64_t)spite_temp_344, spite_site_55()); spite_temp_345; });
index_ = (index_ + 1);
}
int64_t spite_temp_346 = total_;
return spite_temp_346;
}
void TypedMemory__Token_write_value(TypedMemory__Token* self, int64_t address_, int32_t index_, Token* value_) {
((Token**)(intptr_t)address_)[index_] = value_;
}
int64_t TypedMemory__Token_value_bytes(TypedMemory__Token* self) {
return (int64_t)sizeof(Token*);
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
void List_Token_clear(List_Token* self) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
TypedMemory__Token_release_value(self->values_, self->items_, index_);
index_ = (index_ + 1);
}
self->item_count_ = 0;
}
void TypedMemory__Token_release_value(TypedMemory__Token* self, int64_t address_, int32_t index_) {
Token___release(((Token**)(intptr_t)address_)[index_]);
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
{(const void*)&Token___pool_grow, "-\t-", "Token___pool_grow", 0},
{(const void*)&Token___pool_take, "-\t-", "Token___pool_take", 0},
{(const void*)&Token___pool_give, "-\t-", "Token___pool_give", 0},
{(const void*)&spite_singleton_Memory_Heap, "-\t-", "spite_singleton_Memory_Heap", 0},
{(const void*)&spite_singleton_TypedMemory__String, "-\t-", "spite_singleton_TypedMemory__String", 0},
{(const void*)&SpiteArguments_count, "-\t-", "SpiteArguments_count", 0},
{(const void*)&SpiteArguments_get, "-\t-", "SpiteArguments_get", 0},
{(const void*)&Console_Printable___retain, "-\t-", "Console_Printable___retain", 0},
{(const void*)&spite_singleton_Build, "-\t-", "spite_singleton_Build", 0},
{(const void*)&spite_singleton_Console_teardown, "-\t-", "spite_singleton_Console_teardown", 0},
{(const void*)&spite_singleton_Console, "-\t-", "spite_singleton_Console", 0},
{(const void*)&spite_singleton_Program_teardown, "-\t-", "spite_singleton_Program_teardown", 0},
{(const void*)&spite_singleton_Program, "-\t-", "spite_singleton_Program", 0},
{(const void*)&spite_singleton_Clock_teardown, "-\t-", "spite_singleton_Clock_teardown", 0},
{(const void*)&spite_singleton_Clock, "-\t-", "spite_singleton_Clock", 0},
{(const void*)&spite_singleton_Environment_teardown, "-\t-", "spite_singleton_Environment_teardown", 0},
{(const void*)&spite_singleton_Environment, "-\t-", "spite_singleton_Environment", 0},
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
{(const void*)&List_String___framed, "-\t-", "List_String___framed", 0},
{(const void*)&Console___init, "-\t-", "Console___init", 0},
{(const void*)&Console___allocate, "-\t-", "Console___allocate", 0},
{(const void*)&Console___make, "-\t-", "Console___make", 0},
{(const void*)&Console___destroy, "-\t-", "Console___destroy", 0},
{(const void*)&Console___discard, "-\t-", "Console___discard", 0},
{(const void*)&DynamicLibrary___init, "-\t-", "DynamicLibrary___init", 0},
{(const void*)&DynamicLibrary___allocate, "-\t-", "DynamicLibrary___allocate", 0},
{(const void*)&DynamicLibrary___make, "-\t-", "DynamicLibrary___make", 0},
{(const void*)&DynamicLibrary___destroy, "-\t-", "DynamicLibrary___destroy", 0},
{(const void*)&DynamicLibrary___discard, "-\t-", "DynamicLibrary___discard", 0},
{(const void*)&Environment___init, "-\t-", "Environment___init", 0},
{(const void*)&Environment___allocate, "-\t-", "Environment___allocate", 0},
{(const void*)&Environment___make, "-\t-", "Environment___make", 0},
{(const void*)&Environment___destroy, "-\t-", "Environment___destroy", 0},
{(const void*)&Environment___discard, "-\t-", "Environment___discard", 0},
{(const void*)&Program___init, "-\t-", "Program___init", 0},
{(const void*)&Program___allocate, "-\t-", "Program___allocate", 0},
{(const void*)&Program___make, "-\t-", "Program___make", 0},
{(const void*)&Program___destroy, "-\t-", "Program___destroy", 0},
{(const void*)&Program___discard, "-\t-", "Program___discard", 0},
{(const void*)&spite_long_digits, "-\t-", "spite_long_digits", 0},
{(const void*)&spite_string_block, "-\t-", "spite_string_block", 0},
{(const void*)&spite_string_held, "-\t-", "spite_string_held", 0},
{(const void*)&SpiteString___retain, "-\t-", "SpiteString___retain", 0},
{(const void*)&SpiteString___release, "-\t-", "SpiteString___release", 0},
{(const void*)&spite_string_from_bytes, "-\t-", "spite_string_from_bytes", 0},
{(const void*)&spite_string_from_cstring_owned, "-\t-", "spite_string_from_cstring_owned", 0},
{(const void*)&spite_string_join, "-\t-", "spite_string_join", 0},
{(const void*)&SpiteString_append, "-\t-", "SpiteString_append", 0},
{(const void*)&spite_singleton_TypedMemory__Token, "-\t-", "spite_singleton_TypedMemory__Token", 0},
{(const void*)&Naive___init, "-\t-", "Naive___init", 0},
{(const void*)&Naive___allocate, "-\t-", "Naive___allocate", 0},
{(const void*)&Naive___release, "-\t-", "Naive___release", 0},
{(const void*)&Naive___free, "-\t-", "Naive___free", 0},
{(const void*)&spite_tagged_SpiteInteger, "-\t-", "spite_tagged_SpiteInteger", 0},
{(const void*)&Token___init, "-\t-", "Token___init", 0},
{(const void*)&Token___allocate, "-\t-", "Token___allocate", 0},
{(const void*)&Token___make, "-\t-", "Token___make", 0},
{(const void*)&Token___retain, "-\t-", "Token___retain", 0},
{(const void*)&Token___release, "-\t-", "Token___release", 0},
{(const void*)&Token___free, "-\t-", "Token___free", 0},
{(const void*)&List_String___init, "-\t-", "List_String___init", 0},
{(const void*)&List_String___allocate, "-\t-", "List_String___allocate", 0},
{(const void*)&List_String___make, "-\t-", "List_String___make", 0},
{(const void*)&List_String___retain, "-\t-", "List_String___retain", 0},
{(const void*)&List_String___release, "-\t-", "List_String___release", 0},
{(const void*)&List_String___free, "-\t-", "List_String___free", 0},
{(const void*)&List_Console_Printable___init, "-\t-", "List_Console_Printable___init", 0},
{(const void*)&List_Console_Printable___retain, "-\t-", "List_Console_Printable___retain", 0},
{(const void*)&List_Console_Printable___release, "-\t-", "List_Console_Printable___release", 0},
{(const void*)&List_Console_Printable___free, "-\t-", "List_Console_Printable___free", 0},
{(const void*)&List_Token___init, "-\t-", "List_Token___init", 0},
{(const void*)&List_Token___allocate, "-\t-", "List_Token___allocate", 0},
{(const void*)&List_Token___make, "-\t-", "List_Token___make", 0},
{(const void*)&List_Token___retain, "-\t-", "List_Token___retain", 0},
{(const void*)&List_Token___release, "-\t-", "List_Token___release", 0},
{(const void*)&List_Token___free, "-\t-", "List_Token___free", 0},
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
{(const void*)&Environment_setting, "library/environment.spite\tEnvironment", "setting", 6},
{(const void*)&spite_failed_1, "-\t-", "spite_failed_1", 0},
{(const void*)&Environment_reject_misspelled_settings, "library/environment.spite\tEnvironment", "reject_misspelled_settings", 29},
{(const void*)&Environment_reject_misspelled, "library/environment.spite\tEnvironment", "reject_misspelled", 39},
{(const void*)&Environment_setting_written, "library/environment.spite\tEnvironment", "setting_written", 51},
{(const void*)&Environment_integer_setting, "library/environment.spite\tEnvironment", "integer_setting", 69},
{(const void*)&spite_failed_2, "-\t-", "spite_failed_2", 0},
{(const void*)&Environment_Environment, "library/environment.spite\tEnvironment", "Environment", 0},
{(const void*)&SpiteInteger_to_string, "library/integer.spite\tInteger", "to_string", 3},
{(const void*)&SpiteLong_to_string, "library/long.spite\tLong", "to_string", 3},
{(const void*)&Program_exit, "library/program.spite\tProgram", "exit", 5},
{(const void*)&Program_environment, "library/program.spite\tProgram", "environment", 10},
{(const void*)&Program_exit_process, "library/windows/program.spite\tProgram", "exit_process", 8},
{(const void*)&Program_environment_address, "library/windows/program.spite\tProgram", "environment_address", 12},
{(const void*)&Program__flush_output, "bootstrap/source/generation/prelude.spite\tProgram", "_flush_output", 1},
{(const void*)&SpiteString_length, "library/string.spite\tString", "length", 4},
{(const void*)&SpiteString_slice, "library/string.spite\tString", "slice", 10},
{(const void*)&SpiteString__clamped, "library/string.spite\tString", "_clamped", 20},
{(const void*)&SpiteString_equals, "library/string.spite\tString", "equals", 32},
{(const void*)&SpiteString_is_empty, "library/string.spite\tString", "is_empty", 52},
{(const void*)&SpiteString_character_at, "library/string.spite\tString", "character_at", 56},
{(const void*)&SpiteString_matches_at, "library/string.spite\tString", "matches_at", 60},
{(const void*)&SpiteString_index_of, "library/string.spite\tString", "index_of", 74},
{(const void*)&SpiteString_contains, "library/string.spite\tString", "contains", 85},
{(const void*)&SpiteString_starts_with, "library/string.spite\tString", "starts_with", 89},
{(const void*)&SpiteString_split, "library/string.spite\tString", "split", 99},
{(const void*)&SpiteString_replace, "library/string.spite\tString", "replace", 132},
{(const void*)&SpiteString_is_space_at, "library/string.spite\tString", "is_space_at", 140},
{(const void*)&SpiteString_upper_case, "library/string.spite\tString", "upper_case", 157},
{(const void*)&SpiteString_shifted_case, "library/string.spite\tString", "shifted_case", 165},
{(const void*)&SpiteString_to_string, "library/string.spite\tString", "to_string", 184},
{(const void*)&SpiteString_to_long, "library/string.spite\tString", "to_long", 202},
{(const void*)&SpiteString_to_integer, "library/string.spite\tString", "to_integer", 225},
{(const void*)&SpiteString_first_non_space, "library/string.spite\tString", "first_non_space", 278},
{(const void*)&SpiteString_only_spaces_from, "library/string.spite\tString", "only_spaces_from", 286},
{(const void*)&SpiteMemory_Address_text, "library/memory/address.spite\tMemory.Address", "text", 3},
{(const void*)&SpiteMemory_Address_terminated_text, "library/memory/address.spite\tMemory.Address", "terminated_text", 5},
{(const void*)&Memory_Arena_allocate, "library/memory/arena.spite\tMemory.Arena", "allocate", 11},
{(const void*)&Memory_Arena_free, "library/memory/arena.spite\tMemory.Arena", "free", 21},
{(const void*)&Memory_Arena_start_block, "library/memory/arena.spite\tMemory.Arena", "start_block", 23},
{(const void*)&Memory_Heap_allocate, "bootstrap/source/generation/prelude.spite\tMemory.Heap", "allocate", 1},
{(const void*)&Memory_Heap_resize, "bootstrap/source/generation/prelude.spite\tMemory.Heap", "resize", 2},
{(const void*)&Memory_Heap_free, "bootstrap/source/generation/prelude.spite\tMemory.Heap", "free", 3},
{(const void*)&Naive_Naive, "benchmarks/tokens_as_columns/naive/naive.spite\tNaive", "Naive", 7},
{(const void*)&Naive_written, "benchmarks/tokens_as_columns/naive/naive.spite\tNaive", "written", 32},
{(const void*)&Naive_piece_of, "benchmarks/tokens_as_columns/naive/naive.spite\tNaive", "piece_of", 50},
{(const void*)&spite_failed_3, "-\t-", "spite_failed_3", 0},
{(const void*)&spite_failed_4, "-\t-", "spite_failed_4", 0},
{(const void*)&Naive_tokenized, "benchmarks/tokens_as_columns/naive/naive.spite\tNaive", "tokenized", 64},
{(const void*)&Naive_total_of_numbers___held_0, "benchmarks/tokens_as_columns/naive/naive.spite\tNaive", "total_of_numbers", 102},
{(const void*)&Token_Token, "benchmarks/tokens_as_columns/naive/token.spite\tToken", "Token", 13},
{(const void*)&Token_checksum, "benchmarks/tokens_as_columns/naive/token.spite\tToken", "checksum", 21},
{(const void*)&List_String_count, "library/list.spite\tList", "count", 9},
{(const void*)&List_String_append, "library/list.spite\tList", "append", 17},
{(const void*)&List_String_get_at, "library/list.spite\tList", "get_at", 41},
{(const void*)&List_String_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&List_String_contains, "library/list.spite\tList", "contains", 152},
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
{(const void*)&List_Console_Printable_get_at, "library/list.spite\tList", "get_at", 41},
{(const void*)&List_Console_Printable_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&TypedMemory__Console_Printable_read_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "read_value", 1},
{(const void*)&List_Token_append, "library/list.spite\tList", "append", 17},
{(const void*)&List_Token_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_Token_make_room, "library/list.spite\tList", "make_room", 849},
{(const void*)&List_Token__grow, "library/list.spite\tList", "_grow", 855},
{(const void*)&List_Token__resized, "library/list.spite\tList", "_resized", 865},
{(const void*)&List_Token_count_identifier, "library/list.spite\tList", "count_identifier", 0},
{(const void*)&List_Token_count_number, "library/list.spite\tList", "count_number", 0},
{(const void*)&List_Token_count_symbol, "library/list.spite\tList", "count_symbol", 0},
{(const void*)&List_Token_sum_checksum, "library/list.spite\tList", "sum_checksum", 0},
{(const void*)&TypedMemory__Token_write_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "write_value", 2},
{(const void*)&TypedMemory__Token_value_bytes, "bootstrap/source/generation/prelude.spite\tTypedMemory", "value_bytes", 4},
{(const void*)&List_Console_Printable_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&TypedMemory__Console_Printable_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&List_Token_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&TypedMemory__Token_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
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








return 0;
}
