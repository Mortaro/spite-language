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
typedef struct TimeText TimeText;
typedef struct Memory_Arena Memory_Arena;
typedef struct Memory_Heap Memory_Heap;
typedef struct Spite_Argument Spite_Argument;
typedef struct Spite_AttributeDeclaration Spite_AttributeDeclaration;
typedef struct Spite_Class Spite_Class;
typedef struct Spite_Function Spite_Function;
typedef struct Spite_Namespace Spite_Namespace;
typedef struct Naive Naive;
typedef struct Entity Entity;
typedef struct Mover Mover;
typedef struct Position Position;
typedef struct SparseSet SparseSet;
typedef struct Velocity Velocity;
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
typedef void* Mover_Moving;
static Console* spite_singleton_Console_cache = 0;
static bool spite_singleton_Console_destroyed = false;
static int32_t spite_singleton_Console_lock = 0;
typedef struct List_Integer List_Integer;
typedef struct TypedMemory__Integer TypedMemory__Integer;
static DynamicLibrary* spite_foreign_library_1_cache = 0;
static bool spite_foreign_library_1_tracked = false;
static DynamicLibrary* spite_foreign_library_2_cache = 0;
static bool spite_foreign_library_2_tracked = false;
static SpiteString spite_lit_1 = SPITE_STATIC_STRING("", 0);
static Clock* spite_singleton_Clock_cache = 0;
static bool spite_singleton_Clock_destroyed = false;
static int32_t spite_singleton_Clock_lock = 0;
typedef struct { bool has_value; int32_t value; } Nullable_Integer;
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
typedef struct Runner__Mover_Moving Runner__Mover_Moving;
typedef struct Column__Position Column__Position;
typedef struct Vector__Position Vector__Position;
typedef struct InlineMemory__Position InlineMemory__Position;
static Column__Position* spite_singleton_Column__Position_cache = 0;
static bool spite_singleton_Column__Position_destroyed = false;
static int32_t spite_singleton_Column__Position_lock = 0;
typedef struct Column__Velocity Column__Velocity;
typedef struct Vector__Velocity Vector__Velocity;
typedef struct InlineMemory__Velocity InlineMemory__Velocity;
static Column__Velocity* spite_singleton_Column__Velocity_cache = 0;
static bool spite_singleton_Column__Velocity_destroyed = false;
static int32_t spite_singleton_Column__Velocity_lock = 0;
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
static SpiteString spite_lit_10 = SPITE_STATIC_STRING("0", 1);
#define spite_site_6() "library/long.spite:17 in Long.to_string"
#define spite_site_7() "library/long.spite:18 in Long.to_string"
#define spite_site_8() "library/long.spite:22 in Long.to_string"
#define spite_site_9() "library/long.spite:26 in Long.to_string"
#define SpiteLong_to_unsigned_integer(self) ((uint32_t)(self))
#define SpiteLong_to_unsigned_long(self) ((uint64_t)(self))
#define SpiteShort_to_integer(self) ((int32_t)(self))
#define SpiteShort_to_long(self) ((int64_t)(self))
#define SpiteString_code_at(self, index) spite_string_code_at(&(self), (index))
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
#define spite_site_10() "library/memory/arena.spite:12 in Memory.Arena.allocate"
#define spite_site_11() "library/memory/arena.spite:13 in Memory.Arena.allocate"
#define spite_site_12() "library/memory/arena.spite:17 in Memory.Arena.allocate"
#define spite_site_13() "library/memory/arena.spite:25 in Memory.Arena.start_block"
#define spite_site_14() "library/memory/arena.spite:26 in Memory.Arena.start_block"
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
Mover* mover_;
Runner__Mover_Moving* runner_;
Column__Position* positions_;
Column__Velocity* velocities_;
int32_t entity_count_;
};
static SpiteString spite_symbol_3 = { (int64_t)0x6e615f6e77617073ULL, (int64_t)0x010065766f6d5f64ULL };
static Spite_Class* spite_class_object_Long_cache = 0;
static bool spite_class_object_Long_ready = false;
typedef struct Benchmark__Long Benchmark__Long;
static SpiteBox_SpiteString spite_lit_11_box = { { 0, -1 }, SPITE_STATIC_STRING("places", 6) };
static SpiteBox_SpiteString spite_lit_12_box = { { 0, -1 }, SPITE_STATIC_STRING("marked", 6) };
typedef struct SpiteBox_SpiteInteger { SpiteHeader header; int32_t value; } SpiteBox_SpiteInteger;
static SpiteString spite_lit_13 = SPITE_STATIC_STRING("microseconds ", 13);
#define spite_site_15() "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/naive.spite:28 in Naive.spawn_all"
#define spite_site_16() "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/naive.spite:31 in Naive.spawn_all"
struct Entity {
SpiteHeader header;
int32_t id_;
};
struct Mover {
SpiteHeader header;
int32_t marked_;
};
#define spite_site_17() "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/mover.spite:10 in Mover.update_each"
#define spite_site_18() "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/mover.spite:11 in Mover.update_each"
#define spite_site_19() "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/mover.spite:13 in Mover.update_each"
struct Position {
SpiteHeader header;
int32_t left_;
int32_t top_;
};
struct SparseSet {
SpiteHeader header;
List_Integer* dense_of_;
List_Integer* entities_;
};
#define spite_site_20() "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/sparse_set.spite:16 in SparseSet.dense_index"
struct Velocity {
SpiteHeader header;
int32_t across_;
int32_t down_;
};
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
struct List_Integer {
SpiteHeader header;
Memory_Heap* heap_;
TypedMemory__Integer* values_;
int64_t items_;
int32_t item_count_;
int32_t capacity_;
};
struct TypedMemory__Integer {
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
struct Runner__Mover_Moving {
SpiteHeader header;
List_Integer* found_;
bool missing_;
};
struct Column__Position {
SpiteHeader header;
SparseSet* set_;
Vector__Position* values_;
};
struct Vector__Position {
SpiteHeader header;
Memory_Heap* heap_;
InlineMemory__Position* values_;
int64_t items_;
int32_t item_count_;
int32_t capacity_;
};
struct InlineMemory__Position {
SpiteHeader header;
};
struct Column__Velocity {
SpiteHeader header;
SparseSet* set_;
Vector__Velocity* values_;
};
struct Vector__Velocity {
SpiteHeader header;
Memory_Heap* heap_;
InlineMemory__Velocity* values_;
int64_t items_;
int32_t item_count_;
int32_t capacity_;
};
struct InlineMemory__Velocity {
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
struct Benchmark__Long {
SpiteHeader header;
Clock* _clock_;
int64_t answer_;
Duration* duration_;
};
#define SpiteByte_to_unsigned_integer(self) ((uint32_t)(self))
#define SpiteDouble_to_float(self) ((float)(self))
#define SpiteInteger_to_short(self) ((int16_t)(self))
#define SpiteLong_to_double(self) ((double)(self))
#define spite_site_21() "library/list.spite:20 in List.append"
#define spite_site_22() "library/list.spite:856 in List._grow"
#define spite_site_23() "library/list.spite:861 in List._grow"
#define spite_site_24() "spite.crash\t08d4aa22"
#define spite_site_25() "spite.crash\t36a6df9c"
#define spite_site_26() "spite.crash\t61966d00"
#define spite_site_27() "spite.crash\t20f5471a"
#define spite_site_28() "spite.crash\t69b8176b"
#define spite_site_29() "spite.crash\t2cd9342b"
typedef struct Object_entity_Entity_position_Position_velocity_Velocity Object_entity_Entity_position_Position_velocity_Velocity;
struct Object_entity_Entity_position_Position_velocity_Velocity {
SpiteHeader header;
Entity* entity_;
Position* position_;
Velocity* velocity_;
};
#define spite_site_30() "library/vector.spite:20 in Vector.append"
#define spite_site_31() "library/vector.spite:318 in Vector._grow"
#define spite_site_32() "library/vector.spite:168 in Vector.sum_place"
#define spite_site_33() "library/benchmark.spite:11 in Benchmark.Benchmark"
static SpiteString spite_symbol_4 = { (int64_t)0x00000000676e6f4cULL, (int64_t)0x0b00000000000000ULL };
static SpiteString spite_symbol_5 = { (int64_t)0x797469746e656469ULL, (int64_t)0x0700000000000000ULL };
Memory_Heap* spite_singleton_Memory_Heap(void);
Console_Printable Console_Printable___retain(Console_Printable self);
void Console_Printable___release(Console_Printable self);
Build* spite_singleton_Build(void);
Console* spite_singleton_Console(void);
TypedMemory__Integer* spite_singleton_TypedMemory__Integer(void);
DynamicLibrary* spite_foreign_library_1(void);
DynamicLibrary* spite_foreign_library_2(void);
TimeText* spite_singleton_TimeText(void);
Clock* spite_singleton_Clock(void);
TypedMemory__Spite_AttributeDeclaration* spite_singleton_TypedMemory__Spite_AttributeDeclaration(void);
TypedMemory__Spite_Function* spite_singleton_TypedMemory__Spite_Function(void);
TypedMemory__Spite_Argument* spite_singleton_TypedMemory__Spite_Argument(void);
InlineMemory__Position* spite_singleton_InlineMemory__Position(void);
Column__Position* spite_singleton_Column__Position(void);
InlineMemory__Velocity* spite_singleton_InlineMemory__Velocity(void);
Column__Velocity* spite_singleton_Column__Velocity(void);
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
SpiteString SpiteInteger_to_string(int32_t self);
SpiteString SpiteLong_to_string(int64_t self);
SpiteString SpiteString_to_string(SpiteString self);
SpiteString SpiteString___retain(SpiteString self);
void SpiteString___release(SpiteString self);
SpiteString spite_string_from_bytes(const char* bytes, int64_t length);
SpiteString spite_string_join(int32_t count, const SpiteString* pieces);
static int64_t spite_long_digits(char* digits, int64_t value);
static SpiteStringBlock* spite_string_block(int64_t length);
static SpiteString spite_string_held(SpiteStringBlock* block, int64_t length);
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
int64_t Naive_spawn_and_move(Naive* self);
void Naive_spawn_all(Naive* self);
void Naive_push(Naive* self, int32_t entity_);
void Naive_run_ticks(Naive* self, int32_t count_);
Spite_Class* spite_class_object_Long(void);
void Naive_spawn_and_move___dropping_call(void* owner);
Spite_Function* spite_function_value_Naive_spawn_and_move(Naive* owner);
static inline SpiteTagged spite_tagged_SpiteInteger(int32_t value);
int64_t Vector__Position_sum_place(Vector__Position* self);
static Position* Position___framed(Position* self);
static Position* Position___make_into(Position* self);
void Column__Position_insert___held_1(Column__Position* self, int32_t entity_, Position* value_);
static Velocity* Velocity___framed(Velocity* self);
static Velocity* Velocity___make_into(Velocity* self, int32_t new_across_, int32_t new_down_);
void Column__Velocity_insert___held_1(Column__Velocity* self, int32_t entity_, Velocity* value_);
void Runner__Mover_Moving_run___held_0(Runner__Mover_Moving* self, Mover* system_, int32_t entity_count_);
void Entity___init(Entity* self);
void Entity_Entity(Entity* self, int32_t new_id_);
void Mover___init(Mover* self);
Mover* Mover___allocate(void);
Mover* Mover___make(void);
static inline void Mover___release(Mover* self);
void Mover___free(Mover* self);
Position* Mover_Moving___peek_position(Mover_Moving self);
Velocity* Mover_Moving___peek_velocity(Mover_Moving self);
Entity* Mover_Moving___peek_entity(Mover_Moving self);
void Position___init(Position* self);
static inline Position* Position___retain(Position* self);
static inline void Position___release(Position* self);
void Position___free(Position* self);
int64_t Position_get_place(Position* self);
void SparseSet___init(SparseSet* self);
SparseSet* SparseSet___allocate(void);
SparseSet* SparseSet___make(void);
static inline void SparseSet___release(SparseSet* self);
void SparseSet___free(SparseSet* self);
void SparseSet_place(SparseSet* self, int32_t entity_);
int32_t SparseSet_dense_index(SparseSet* self, int32_t entity_);
void Velocity___init(Velocity* self);
static inline Velocity* Velocity___retain(Velocity* self);
static inline void Velocity___release(Velocity* self);
void Velocity___free(Velocity* self);
void Velocity_Velocity(Velocity* self, int32_t new_across_, int32_t new_down_);
static inline void List_String___release(List_String* self);
void List_String___free(List_String* self);
void List_String_drop(List_String* self);
void List_String_clear(List_String* self);
void List_String_drop(List_String* self);
void TypedMemory__String___release(TypedMemory__String* self);
void TypedMemory__String_release_value(TypedMemory__String* self, int64_t address_, int32_t index_);
void List_Integer___init(List_Integer* self);
List_Integer* List_Integer___allocate(void);
List_Integer* List_Integer___make(void);
static inline void List_Integer___release(List_Integer* self);
void List_Integer___free(List_Integer* self);
void List_Integer_drop(List_Integer* self);
int32_t List_Integer_count(List_Integer* self);
void List_Integer_append(List_Integer* self, int32_t value_);
Nullable_Integer List_Integer_get_at(List_Integer* self, int32_t index_);
void List_Integer_set_at(List_Integer* self, int32_t index_, int32_t value_);
void List_Integer_clear(List_Integer* self);
void List_Integer_drop(List_Integer* self);
void List_Integer_make_room(List_Integer* self);
void List_Integer__grow(List_Integer* self);
int64_t List_Integer__resized(List_Integer* self, int64_t old_bytes_, int64_t new_bytes_);
void TypedMemory__Integer___release(TypedMemory__Integer* self);
int32_t TypedMemory__Integer_read_value(TypedMemory__Integer* self, int64_t address_, int32_t index_);
void TypedMemory__Integer_write_value(TypedMemory__Integer* self, int64_t address_, int32_t index_, int32_t value_);
void TypedMemory__Integer_release_value(TypedMemory__Integer* self, int64_t address_, int32_t index_);
int64_t TypedMemory__Integer_value_bytes(TypedMemory__Integer* self);
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
void Runner__Mover_Moving___init(Runner__Mover_Moving* self);
Runner__Mover_Moving* Runner__Mover_Moving___allocate(void);
Runner__Mover_Moving* Runner__Mover_Moving___make(void);
static inline void Runner__Mover_Moving___release(Runner__Mover_Moving* self);
void Runner__Mover_Moving___free(Runner__Mover_Moving* self);
void Runner__Mover_Moving_note(Runner__Mover_Moving* self, int32_t dense_);
void Runner__Mover_Moving_run___held_0(Runner__Mover_Moving* self, Mover* system_, int32_t entity_count_);
void Column__Position___init(Column__Position* self);
Column__Position* Column__Position___allocate(void);
Column__Position* Column__Position___make(void);
void Column__Position___release(Column__Position* self);
void Column__Position_insert___held_1(Column__Position* self, int32_t entity_, Position* value_);
void Column__Position___destroy(Column__Position* self);
void Column__Position___discard(Column__Position* self);
void Vector__Position___init(Vector__Position* self);
Vector__Position* Vector__Position___allocate(void);
Vector__Position* Vector__Position___make(void);
static inline void Vector__Position___release(Vector__Position* self);
void Vector__Position___free(Vector__Position* self);
void Vector__Position_drop(Vector__Position* self);
void Vector__Position_append(Vector__Position* self, Position* value_);
Position* Vector__Position_get_at(Vector__Position* self, int32_t index_);
void Vector__Position_clear(Vector__Position* self);
void Vector__Position_drop(Vector__Position* self);
void Vector__Position_make_room(Vector__Position* self);
void Vector__Position__grow(Vector__Position* self);
int64_t Vector__Position__resized(Vector__Position* self, int64_t old_bytes_, int64_t new_bytes_);
int64_t Vector__Position_sum_place(Vector__Position* self);
void InlineMemory__Position___release(InlineMemory__Position* self);
Position* InlineMemory__Position_item_at(InlineMemory__Position* self, int64_t address_, int32_t index_);
void InlineMemory__Position_write_item(InlineMemory__Position* self, int64_t address_, int32_t index_, Position* value_);
void InlineMemory__Position_release_item(InlineMemory__Position* self, int64_t address_, int32_t index_);
int64_t InlineMemory__Position_block_bytes(InlineMemory__Position* self, int32_t item_count_);
void Column__Velocity___init(Column__Velocity* self);
Column__Velocity* Column__Velocity___allocate(void);
Column__Velocity* Column__Velocity___make(void);
void Column__Velocity___release(Column__Velocity* self);
void Column__Velocity_insert___held_1(Column__Velocity* self, int32_t entity_, Velocity* value_);
void Column__Velocity___destroy(Column__Velocity* self);
void Column__Velocity___discard(Column__Velocity* self);
void Vector__Velocity___init(Vector__Velocity* self);
Vector__Velocity* Vector__Velocity___allocate(void);
Vector__Velocity* Vector__Velocity___make(void);
static inline void Vector__Velocity___release(Vector__Velocity* self);
void Vector__Velocity___free(Vector__Velocity* self);
void Vector__Velocity_drop(Vector__Velocity* self);
void Vector__Velocity_append(Vector__Velocity* self, Velocity* value_);
Velocity* Vector__Velocity_get_at(Vector__Velocity* self, int32_t index_);
void Vector__Velocity_clear(Vector__Velocity* self);
void Vector__Velocity_drop(Vector__Velocity* self);
void Vector__Velocity_make_room(Vector__Velocity* self);
void Vector__Velocity__grow(Vector__Velocity* self);
int64_t Vector__Velocity__resized(Vector__Velocity* self, int64_t old_bytes_, int64_t new_bytes_);
void InlineMemory__Velocity___release(InlineMemory__Velocity* self);
Velocity* InlineMemory__Velocity_item_at(InlineMemory__Velocity* self, int64_t address_, int32_t index_);
void InlineMemory__Velocity_write_item(InlineMemory__Velocity* self, int64_t address_, int32_t index_, Velocity* value_);
void InlineMemory__Velocity_release_item(InlineMemory__Velocity* self, int64_t address_, int32_t index_);
int64_t InlineMemory__Velocity_block_bytes(InlineMemory__Velocity* self, int32_t item_count_);
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
void Benchmark__Long___init(Benchmark__Long* self);
Benchmark__Long* Benchmark__Long___allocate(void);
Benchmark__Long* Benchmark__Long___make(Spite_Function* work_);
static inline void Benchmark__Long___release(Benchmark__Long* self);
void Benchmark__Long___free(Benchmark__Long* self);
void Benchmark__Long_Benchmark(Benchmark__Long* self, Spite_Function* work_);
static SPITE_CRASH_REPORT void spite_failed_1(int32_t index_, int32_t value_, List_Integer* self);
static SPITE_CRASH_REPORT void spite_failed_2(int32_t index_, List_Integer* self, int32_t value_);
void Runner__Mover_Moving_find_attributes(Runner__Mover_Moving* self, int32_t entity_);
static SPITE_CRASH_REPORT void spite_failed_3(int32_t entity_count_, int32_t entity_, Runner__Mover_Moving* self);
static SPITE_CRASH_REPORT void spite_failed_4(int32_t entity_count_, int32_t entity_, Runner__Mover_Moving* self);
static SPITE_CRASH_REPORT void spite_failed_5(int32_t entity_count_, int32_t entity_, Runner__Mover_Moving* self);
static SPITE_CRASH_REPORT void spite_failed_6(int32_t entity_count_, int32_t entity_, Runner__Mover_Moving* self);
void Mover_update_each___lent_0(Mover* self, Mover_Moving moving_);
void Runner__Mover_Moving_find_entity(Runner__Mover_Moving* self, int32_t entity_);
void Runner__Mover_Moving_find_position(Runner__Mover_Moving* self, int32_t entity_);
void Runner__Mover_Moving_find_velocity(Runner__Mover_Moving* self, int32_t entity_);
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
#define SPITE_ALLOCATOR_Vector__Position(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_Vector__Velocity(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Console_Printable(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Console_Debuggable(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Directory_Entry(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_File(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Symbol(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Spite_Access(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Directory(object, heap) ((void)(object), ((Spite_Allocator)heap()))
static __typeof__(&Memory_Heap___release) spite_folded_Memory_Heap___release = ((__typeof__(&Memory_Heap___release))&TimeText___release);
static __typeof__(&TypedMemory__String___release) spite_folded_TypedMemory__String___release = ((__typeof__(&TypedMemory__String___release))&TimeText___release);
static __typeof__(&TypedMemory__Integer___release) spite_folded_TypedMemory__Integer___release = ((__typeof__(&TypedMemory__Integer___release))&TimeText___release);
static __typeof__(&TypedMemory__Spite_AttributeDeclaration___release) spite_folded_TypedMemory__Spite_AttributeDeclaration___release = ((__typeof__(&TypedMemory__Spite_AttributeDeclaration___release))&TimeText___release);
static __typeof__(&TypedMemory__Spite_Function___release) spite_folded_TypedMemory__Spite_Function___release = ((__typeof__(&TypedMemory__Spite_Function___release))&TimeText___release);
static __typeof__(&TypedMemory__Spite_Argument___release) spite_folded_TypedMemory__Spite_Argument___release = ((__typeof__(&TypedMemory__Spite_Argument___release))&TimeText___release);
static __typeof__(&TypedMemory__Spite_Class___release) spite_folded_TypedMemory__Spite_Class___release = ((__typeof__(&TypedMemory__Spite_Class___release))&TimeText___release);
static __typeof__(&TypedMemory__Spite_Namespace___release) spite_folded_TypedMemory__Spite_Namespace___release = ((__typeof__(&TypedMemory__Spite_Namespace___release))&TimeText___release);
static __typeof__(&InlineMemory__Position___release) spite_folded_InlineMemory__Position___release = ((__typeof__(&InlineMemory__Position___release))&TimeText___release);
static __typeof__(&Column__Velocity___release) spite_folded_Column__Velocity___release = ((__typeof__(&Column__Velocity___release))&Column__Position___release);
static __typeof__(&InlineMemory__Velocity___release) spite_folded_InlineMemory__Velocity___release = ((__typeof__(&InlineMemory__Velocity___release))&TimeText___release);
static __typeof__(&TypedMemory__Console_Printable___release) spite_folded_TypedMemory__Console_Printable___release = ((__typeof__(&TypedMemory__Console_Printable___release))&TimeText___release);
static __typeof__(&TypedMemory__Symbol___release) spite_folded_TypedMemory__Symbol___release = ((__typeof__(&TypedMemory__Symbol___release))&TimeText___release);
static __typeof__(&List_Console_Printable_count) spite_folded_List_Console_Printable_count = ((__typeof__(&List_Console_Printable_count))&List_Integer_count);
static __typeof__(&List_Symbol_clear) spite_folded_List_Symbol_clear = ((__typeof__(&List_Symbol_clear))&List_String_clear);
static __typeof__(&TypedMemory__Symbol_release_value) spite_folded_TypedMemory__Symbol_release_value = ((__typeof__(&TypedMemory__Symbol_release_value))&TypedMemory__String_release_value);
static Launcher* Launcher___pool_free = 0;
static char* Launcher___pool_next = 0;
static char* Launcher___pool_end = 0;
static size_t Launcher___pool_count = 0;
static void Launcher___pool_grow(void) {
if (Launcher___pool_count == 0) { Launcher___pool_count = 16; } else if (Launcher___pool_count * sizeof(Launcher) < 262144) { Launcher___pool_count = Launcher___pool_count * 2; }
char* chunk = (char*)SPITE_MALLOC(Launcher___pool_count * sizeof(Launcher) + 63);
if (chunk == 0) { fflush(stdout); fputs("spite: out of memory making an object\n", stderr); exit(1); }
Launcher___pool_next = (char*)(((uintptr_t)chunk + 63) & ~(uintptr_t)63);
Launcher___pool_end = Launcher___pool_next + Launcher___pool_count * sizeof(Launcher);
}
static inline Launcher* Launcher___pool_take(void) {
Launcher* self = Launcher___pool_free;
if (self != 0) { Launcher___pool_free = *(Launcher**)self; return self; }
if (Launcher___pool_next == Launcher___pool_end) Launcher___pool_grow();
self = (Launcher*)Launcher___pool_next;
Launcher___pool_next = Launcher___pool_next + sizeof(Launcher);
return self;
}
static inline void Launcher___pool_give(Launcher* self) {
*(Launcher**)self = Launcher___pool_free;
Launcher___pool_free = self;
}
static Duration* Duration___pool_free = 0;
static char* Duration___pool_next = 0;
static char* Duration___pool_end = 0;
static size_t Duration___pool_count = 0;
static void Duration___pool_grow(void) {
if (Duration___pool_count == 0) { Duration___pool_count = 16; } else if (Duration___pool_count * sizeof(Duration) < 262144) { Duration___pool_count = Duration___pool_count * 2; }
char* chunk = (char*)SPITE_MALLOC(Duration___pool_count * sizeof(Duration) + 63);
if (chunk == 0) { fflush(stdout); fputs("spite: out of memory making an object\n", stderr); exit(1); }
Duration___pool_next = (char*)(((uintptr_t)chunk + 63) & ~(uintptr_t)63);
Duration___pool_end = Duration___pool_next + Duration___pool_count * sizeof(Duration);
}
static inline Duration* Duration___pool_take(void) {
Duration* self = Duration___pool_free;
if (self != 0) { Duration___pool_free = *(Duration**)self; return self; }
if (Duration___pool_next == Duration___pool_end) Duration___pool_grow();
self = (Duration*)Duration___pool_next;
Duration___pool_next = Duration___pool_next + sizeof(Duration);
return self;
}
static inline void Duration___pool_give(Duration* self) {
*(Duration**)self = Duration___pool_free;
Duration___pool_free = self;
}
static Spite_Class* Spite_Class___pool_free = 0;
static char* Spite_Class___pool_next = 0;
static char* Spite_Class___pool_end = 0;
static size_t Spite_Class___pool_count = 0;
static void Spite_Class___pool_grow(void) {
if (Spite_Class___pool_count == 0) { Spite_Class___pool_count = 16; } else if (Spite_Class___pool_count * sizeof(Spite_Class) < 262144) { Spite_Class___pool_count = Spite_Class___pool_count * 2; }
char* chunk = (char*)SPITE_MALLOC(Spite_Class___pool_count * sizeof(Spite_Class) + 63);
if (chunk == 0) { fflush(stdout); fputs("spite: out of memory making an object\n", stderr); exit(1); }
Spite_Class___pool_next = (char*)(((uintptr_t)chunk + 63) & ~(uintptr_t)63);
Spite_Class___pool_end = Spite_Class___pool_next + Spite_Class___pool_count * sizeof(Spite_Class);
}
static inline Spite_Class* Spite_Class___pool_take(void) {
Spite_Class* self = Spite_Class___pool_free;
if (self != 0) { Spite_Class___pool_free = *(Spite_Class**)self; return self; }
if (Spite_Class___pool_next == Spite_Class___pool_end) Spite_Class___pool_grow();
self = (Spite_Class*)Spite_Class___pool_next;
Spite_Class___pool_next = Spite_Class___pool_next + sizeof(Spite_Class);
return self;
}
static inline void Spite_Class___pool_give(Spite_Class* self) {
*(Spite_Class**)self = Spite_Class___pool_free;
Spite_Class___pool_free = self;
}
static Spite_Function* Spite_Function___pool_free = 0;
static char* Spite_Function___pool_next = 0;
static char* Spite_Function___pool_end = 0;
static size_t Spite_Function___pool_count = 0;
static void Spite_Function___pool_grow(void) {
if (Spite_Function___pool_count == 0) { Spite_Function___pool_count = 16; } else if (Spite_Function___pool_count * sizeof(Spite_Function) < 262144) { Spite_Function___pool_count = Spite_Function___pool_count * 2; }
char* chunk = (char*)SPITE_MALLOC(Spite_Function___pool_count * sizeof(Spite_Function) + 63);
if (chunk == 0) { fflush(stdout); fputs("spite: out of memory making an object\n", stderr); exit(1); }
Spite_Function___pool_next = (char*)(((uintptr_t)chunk + 63) & ~(uintptr_t)63);
Spite_Function___pool_end = Spite_Function___pool_next + Spite_Function___pool_count * sizeof(Spite_Function);
}
static inline Spite_Function* Spite_Function___pool_take(void) {
Spite_Function* self = Spite_Function___pool_free;
if (self != 0) { Spite_Function___pool_free = *(Spite_Function**)self; return self; }
if (Spite_Function___pool_next == Spite_Function___pool_end) Spite_Function___pool_grow();
self = (Spite_Function*)Spite_Function___pool_next;
Spite_Function___pool_next = Spite_Function___pool_next + sizeof(Spite_Function);
return self;
}
static inline void Spite_Function___pool_give(Spite_Function* self) {
*(Spite_Function**)self = Spite_Function___pool_free;
Spite_Function___pool_free = self;
}
static Naive* Naive___pool_free = 0;
static char* Naive___pool_next = 0;
static char* Naive___pool_end = 0;
static size_t Naive___pool_count = 0;
static void Naive___pool_grow(void) {
if (Naive___pool_count == 0) { Naive___pool_count = 16; } else if (Naive___pool_count * sizeof(Naive) < 262144) { Naive___pool_count = Naive___pool_count * 2; }
char* chunk = (char*)SPITE_MALLOC(Naive___pool_count * sizeof(Naive) + 63);
if (chunk == 0) { fflush(stdout); fputs("spite: out of memory making an object\n", stderr); exit(1); }
Naive___pool_next = (char*)(((uintptr_t)chunk + 63) & ~(uintptr_t)63);
Naive___pool_end = Naive___pool_next + Naive___pool_count * sizeof(Naive);
}
static inline Naive* Naive___pool_take(void) {
Naive* self = Naive___pool_free;
if (self != 0) { Naive___pool_free = *(Naive**)self; return self; }
if (Naive___pool_next == Naive___pool_end) Naive___pool_grow();
self = (Naive*)Naive___pool_next;
Naive___pool_next = Naive___pool_next + sizeof(Naive);
return self;
}
static inline void Naive___pool_give(Naive* self) {
*(Naive**)self = Naive___pool_free;
Naive___pool_free = self;
}
static Mover* Mover___pool_free = 0;
static char* Mover___pool_next = 0;
static char* Mover___pool_end = 0;
static size_t Mover___pool_count = 0;
static void Mover___pool_grow(void) {
if (Mover___pool_count == 0) { Mover___pool_count = 16; } else if (Mover___pool_count * sizeof(Mover) < 262144) { Mover___pool_count = Mover___pool_count * 2; }
char* chunk = (char*)SPITE_MALLOC(Mover___pool_count * sizeof(Mover) + 63);
if (chunk == 0) { fflush(stdout); fputs("spite: out of memory making an object\n", stderr); exit(1); }
Mover___pool_next = (char*)(((uintptr_t)chunk + 63) & ~(uintptr_t)63);
Mover___pool_end = Mover___pool_next + Mover___pool_count * sizeof(Mover);
}
static inline Mover* Mover___pool_take(void) {
Mover* self = Mover___pool_free;
if (self != 0) { Mover___pool_free = *(Mover**)self; return self; }
if (Mover___pool_next == Mover___pool_end) Mover___pool_grow();
self = (Mover*)Mover___pool_next;
Mover___pool_next = Mover___pool_next + sizeof(Mover);
return self;
}
static inline void Mover___pool_give(Mover* self) {
*(Mover**)self = Mover___pool_free;
Mover___pool_free = self;
}
static SparseSet* SparseSet___pool_free = 0;
static char* SparseSet___pool_next = 0;
static char* SparseSet___pool_end = 0;
static size_t SparseSet___pool_count = 0;
static void SparseSet___pool_grow(void) {
if (SparseSet___pool_count == 0) { SparseSet___pool_count = 16; } else if (SparseSet___pool_count * sizeof(SparseSet) < 262144) { SparseSet___pool_count = SparseSet___pool_count * 2; }
char* chunk = (char*)SPITE_MALLOC(SparseSet___pool_count * sizeof(SparseSet) + 63);
if (chunk == 0) { fflush(stdout); fputs("spite: out of memory making an object\n", stderr); exit(1); }
SparseSet___pool_next = (char*)(((uintptr_t)chunk + 63) & ~(uintptr_t)63);
SparseSet___pool_end = SparseSet___pool_next + SparseSet___pool_count * sizeof(SparseSet);
}
static inline SparseSet* SparseSet___pool_take(void) {
SparseSet* self = SparseSet___pool_free;
if (self != 0) { SparseSet___pool_free = *(SparseSet**)self; return self; }
if (SparseSet___pool_next == SparseSet___pool_end) SparseSet___pool_grow();
self = (SparseSet*)SparseSet___pool_next;
SparseSet___pool_next = SparseSet___pool_next + sizeof(SparseSet);
return self;
}
static inline void SparseSet___pool_give(SparseSet* self) {
*(SparseSet**)self = SparseSet___pool_free;
SparseSet___pool_free = self;
}
static Runner__Mover_Moving* Runner__Mover_Moving___pool_free = 0;
static char* Runner__Mover_Moving___pool_next = 0;
static char* Runner__Mover_Moving___pool_end = 0;
static size_t Runner__Mover_Moving___pool_count = 0;
static void Runner__Mover_Moving___pool_grow(void) {
if (Runner__Mover_Moving___pool_count == 0) { Runner__Mover_Moving___pool_count = 16; } else if (Runner__Mover_Moving___pool_count * sizeof(Runner__Mover_Moving) < 262144) { Runner__Mover_Moving___pool_count = Runner__Mover_Moving___pool_count * 2; }
char* chunk = (char*)SPITE_MALLOC(Runner__Mover_Moving___pool_count * sizeof(Runner__Mover_Moving) + 63);
if (chunk == 0) { fflush(stdout); fputs("spite: out of memory making an object\n", stderr); exit(1); }
Runner__Mover_Moving___pool_next = (char*)(((uintptr_t)chunk + 63) & ~(uintptr_t)63);
Runner__Mover_Moving___pool_end = Runner__Mover_Moving___pool_next + Runner__Mover_Moving___pool_count * sizeof(Runner__Mover_Moving);
}
static inline Runner__Mover_Moving* Runner__Mover_Moving___pool_take(void) {
Runner__Mover_Moving* self = Runner__Mover_Moving___pool_free;
if (self != 0) { Runner__Mover_Moving___pool_free = *(Runner__Mover_Moving**)self; return self; }
if (Runner__Mover_Moving___pool_next == Runner__Mover_Moving___pool_end) Runner__Mover_Moving___pool_grow();
self = (Runner__Mover_Moving*)Runner__Mover_Moving___pool_next;
Runner__Mover_Moving___pool_next = Runner__Mover_Moving___pool_next + sizeof(Runner__Mover_Moving);
return self;
}
static inline void Runner__Mover_Moving___pool_give(Runner__Mover_Moving* self) {
*(Runner__Mover_Moving**)self = Runner__Mover_Moving___pool_free;
Runner__Mover_Moving___pool_free = self;
}
static Benchmark__Long* Benchmark__Long___pool_free = 0;
static char* Benchmark__Long___pool_next = 0;
static char* Benchmark__Long___pool_end = 0;
static size_t Benchmark__Long___pool_count = 0;
static void Benchmark__Long___pool_grow(void) {
if (Benchmark__Long___pool_count == 0) { Benchmark__Long___pool_count = 16; } else if (Benchmark__Long___pool_count * sizeof(Benchmark__Long) < 262144) { Benchmark__Long___pool_count = Benchmark__Long___pool_count * 2; }
char* chunk = (char*)SPITE_MALLOC(Benchmark__Long___pool_count * sizeof(Benchmark__Long) + 63);
if (chunk == 0) { fflush(stdout); fputs("spite: out of memory making an object\n", stderr); exit(1); }
Benchmark__Long___pool_next = (char*)(((uintptr_t)chunk + 63) & ~(uintptr_t)63);
Benchmark__Long___pool_end = Benchmark__Long___pool_next + Benchmark__Long___pool_count * sizeof(Benchmark__Long);
}
static inline Benchmark__Long* Benchmark__Long___pool_take(void) {
Benchmark__Long* self = Benchmark__Long___pool_free;
if (self != 0) { Benchmark__Long___pool_free = *(Benchmark__Long**)self; return self; }
if (Benchmark__Long___pool_next == Benchmark__Long___pool_end) Benchmark__Long___pool_grow();
self = (Benchmark__Long*)Benchmark__Long___pool_next;
Benchmark__Long___pool_next = Benchmark__Long___pool_next + sizeof(Benchmark__Long);
return self;
}
static inline void Benchmark__Long___pool_give(Benchmark__Long* self) {
*(Benchmark__Long**)self = Benchmark__Long___pool_free;
Benchmark__Long___pool_free = self;
}
Memory_Heap* spite_singleton_Memory_Heap(void) {
static Memory_Heap spite_object = { { 1, 95 } };
return &spite_object;
}


Console_Printable Console_Printable___retain(Console_Printable self) {
if (self.plain == 0 && self.value.object != 0) SPITE_COUNT_UP(((SpiteHeader*)self.value.object)->ref_count);
return self;
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
TypedMemory__Integer* spite_singleton_TypedMemory__Integer(void) {
static TypedMemory__Integer spite_object = { { 1, 129 } };
return &spite_object;
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
TypedMemory__Spite_AttributeDeclaration* spite_singleton_TypedMemory__Spite_AttributeDeclaration(void) {
static TypedMemory__Spite_AttributeDeclaration spite_object = { { 1, 147 } };
return &spite_object;
}
TypedMemory__Spite_Function* spite_singleton_TypedMemory__Spite_Function(void) {
static TypedMemory__Spite_Function spite_object = { { 1, 149 } };
return &spite_object;
}
TypedMemory__Spite_Argument* spite_singleton_TypedMemory__Spite_Argument(void) {
static TypedMemory__Spite_Argument spite_object = { { 1, 151 } };
return &spite_object;
}
InlineMemory__Position* spite_singleton_InlineMemory__Position(void) {
static InlineMemory__Position spite_object = { { 1, 159 } };
return &spite_object;
}
static void spite_singleton_Column__Position_teardown(void) {
Column__Position* object = spite_singleton_Column__Position_cache;
spite_singleton_Column__Position_cache = 0;
spite_singleton_Column__Position_destroyed = true;
Column__Position___destroy(object);
}
Column__Position* spite_singleton_Column__Position(void) {
Column__Position* found = SPITE_SINGLETON_FOUND(spite_singleton_Column__Position_cache);
if (found != 0) return found;
spite_singleton_check_circle("Column<Position>");
SPITE_LOCK(spite_singleton_Column__Position_lock);
if (spite_singleton_Column__Position_cache == 0) {
if (spite_singleton_Column__Position_destroyed) spite_singleton_used_after_exit("Column");
spite_singleton_making("Column<Position>");
Column__Position* made = Column__Position___make();
spite_singleton_made();
spite_singleton_created(spite_singleton_Column__Position_teardown);
SPITE_SINGLETON_PUBLISH(spite_singleton_Column__Position_cache, made);
}
SPITE_UNLOCK(spite_singleton_Column__Position_lock);
return spite_singleton_Column__Position_cache;
}
InlineMemory__Velocity* spite_singleton_InlineMemory__Velocity(void) {
static InlineMemory__Velocity spite_object = { { 1, 162 } };
return &spite_object;
}
static void spite_singleton_Column__Velocity_teardown(void) {
Column__Velocity* object = spite_singleton_Column__Velocity_cache;
spite_singleton_Column__Velocity_cache = 0;
spite_singleton_Column__Velocity_destroyed = true;
Column__Velocity___destroy(object);
}
Column__Velocity* spite_singleton_Column__Velocity(void) {
Column__Velocity* found = SPITE_SINGLETON_FOUND(spite_singleton_Column__Velocity_cache);
if (found != 0) return found;
spite_singleton_check_circle("Column<Velocity>");
SPITE_LOCK(spite_singleton_Column__Velocity_lock);
if (spite_singleton_Column__Velocity_cache == 0) {
if (spite_singleton_Column__Velocity_destroyed) spite_singleton_used_after_exit("Column");
spite_singleton_making("Column<Velocity>");
Column__Velocity* made = Column__Velocity___make();
spite_singleton_made();
spite_singleton_created(spite_singleton_Column__Velocity_teardown);
SPITE_SINGLETON_PUBLISH(spite_singleton_Column__Velocity_cache, made);
}
SPITE_UNLOCK(spite_singleton_Column__Velocity_lock);
return spite_singleton_Column__Velocity_cache;
}
void Launcher___init(Launcher* self) {
self->build_ = spite_singleton_Build();
}
Launcher* Launcher___allocate(void) {
Launcher* self = Launcher___pool_take();
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
Launcher___pool_give(self);
}
static void spite_overflowed(const char* operation, const char* type, const char* symbol, int64_t left, int64_t right, const char* where) {
fflush(stdout);
fprintf(stderr, "spite: '%s' does not fit in %s (%lld %s %lld), at %s\n", operation, type, (long long)left, symbol, (long long)right, where);
exit(1);
}
TypedMemory__Console_Printable* spite_singleton_TypedMemory__Console_Printable(void) {
static TypedMemory__Console_Printable spite_object = { { 1, 164 } };
return &spite_object;
}
static List_Console_Printable* List_Console_Printable___framed(List_Console_Printable* self, int64_t items, int32_t count) {
List_Console_Printable___init(self);
self->header.ref_count = 2;
self->header.class_id = 163;
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
tagged.tag = 165;
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
Duration* self = Duration___pool_take();
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
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
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
Duration___pool_give(self);
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
void TimeText___release(TimeText* self) { (void)self; }
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
Spite_Class* self = Spite_Class___pool_take();
self->header.ref_count = 1;
self->header.class_id = 101;
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
Spite_Class___pool_give(self);
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
Spite_Function* self = Spite_Function___pool_take();
self->header.ref_count = 1;
self->header.class_id = 105;
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
Spite_Function___pool_give(self);
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
self->mover_ = Mover___make();
self->runner_ = Runner__Mover_Moving___make();
self->positions_ = spite_singleton_Column__Position();
self->velocities_ = spite_singleton_Column__Velocity();
self->entity_count_ = 100000;
}
Naive* Naive___allocate(void) {
Naive* self = Naive___pool_take();
self->header.ref_count = 1;
self->header.class_id = 110;
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
Mover___release(self->mover_);
Runner__Mover_Moving___release(self->runner_);
Column__Position___release(self->positions_);
spite_folded_Column__Velocity___release(self->velocities_);
#ifdef SPITE_TRACKS_Naive
spite_untrack_Naive(self);
#endif
#ifdef SPITE_WEAK_Naive
spite_weak_object_freed(self);
#endif
Naive___pool_give(self);
}
void Naive_spawn_and_move___dropping_call(void* owner) {
(void)Naive_spawn_and_move((Naive*)owner);
}
Spite_Function* spite_function_value_Naive_spawn_and_move(Naive* owner) {
Spite_Function* described = Spite_Function___make(spite_symbol_3, spite_class_object_Long());
described->spite_owner = Naive___retain(owner);
described->spite_release_owner = (void (*)(void*))Naive___release;
described->spite_call = (void (*)(void*))Naive_spawn_and_move___dropping_call;
described->spite_typed_call = (void*)Naive_spawn_and_move;
return described;
}
static inline SpiteTagged spite_tagged_SpiteInteger(int32_t value) {
SpiteTagged tagged;
tagged.tag = 181;
tagged.plain = 1;
tagged.value.bits = 0;
memcpy(&tagged.value, &value, sizeof(value));
return tagged;
}
static Position* Position___framed(Position* self) {
self->header.ref_count = SPITE_FRAMED_COUNT;
self->header.class_id = 114;
return self;
}
static Position* Position___make_into(Position* self) {
Position___framed(self);
Position___init(self);
return self;
}
static Velocity* Velocity___framed(Velocity* self) {
self->header.ref_count = SPITE_FRAMED_COUNT;
self->header.class_id = 117;
return self;
}
static Velocity* Velocity___make_into(Velocity* self, int32_t new_across_, int32_t new_down_) {
Velocity___framed(self);
Velocity___init(self);
Velocity_Velocity(self, new_across_, new_down_);
return self;
}
void Entity___init(Entity* self) {
self->id_ = 0;
}
void Mover___init(Mover* self) {
self->marked_ = 0;
}
Mover* Mover___allocate(void) {
Mover* self = Mover___pool_take();
self->header.ref_count = 1;
self->header.class_id = 113;
Mover___init(self);
#ifdef SPITE_TRACKS_Mover
spite_track_Mover(self);
#endif
return self;
}
Mover* Mover___make(void) {
Mover* self = Mover___allocate();
return self;
}
static inline void Mover___release(Mover* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Mover___free(self);
}
void Mover___free(Mover* self) {
#ifdef SPITE_TRACKS_Mover
spite_untrack_Mover(self);
#endif
#ifdef SPITE_WEAK_Mover
spite_weak_object_freed(self);
#endif
Mover___pool_give(self);
}

void Position___init(Position* self) {
self->left_ = 0;
self->top_ = 0;
}
static inline Position* Position___retain(Position* self) {
if (self != 0) SPITE_COUNT_UP(self->header.ref_count);
return self;
}
static inline void Position___release(Position* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Position___free(self);
}
void Position___free(Position* self) {
#ifdef SPITE_TRACKS_Position
spite_untrack_Position(self);
#endif
#ifdef SPITE_WEAK_Position
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void SparseSet___init(SparseSet* self) {
self->dense_of_ = List_Integer___make();
self->entities_ = List_Integer___make();
}
SparseSet* SparseSet___allocate(void) {
SparseSet* self = SparseSet___pool_take();
self->header.ref_count = 1;
self->header.class_id = 116;
SparseSet___init(self);
#ifdef SPITE_TRACKS_SparseSet
spite_track_SparseSet(self);
#endif
return self;
}
SparseSet* SparseSet___make(void) {
SparseSet* self = SparseSet___allocate();
return self;
}
static inline void SparseSet___release(SparseSet* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
SparseSet___free(self);
}
void SparseSet___free(SparseSet* self) {
List_Integer___release(self->dense_of_);
List_Integer___release(self->entities_);
#ifdef SPITE_TRACKS_SparseSet
spite_untrack_SparseSet(self);
#endif
#ifdef SPITE_WEAK_SparseSet
spite_weak_object_freed(self);
#endif
SparseSet___pool_give(self);
}
void Velocity___init(Velocity* self) {
self->across_ = 0;
self->down_ = 0;
}
static inline Velocity* Velocity___retain(Velocity* self) {
if (self != 0) SPITE_COUNT_UP(self->header.ref_count);
return self;
}
static inline void Velocity___release(Velocity* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Velocity___free(self);
}
void Velocity___free(Velocity* self) {
#ifdef SPITE_TRACKS_Velocity
spite_untrack_Velocity(self);
#endif
#ifdef SPITE_WEAK_Velocity
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
void List_Integer___init(List_Integer* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->values_ = spite_singleton_TypedMemory__Integer();
self->items_ = ((int64_t)(0));
self->item_count_ = 0;
self->capacity_ = 0;
}
List_Integer* List_Integer___allocate(void) {
List_Integer* self = (List_Integer*)SPITE_MALLOC(sizeof(List_Integer));
self->header.ref_count = 1;
self->header.class_id = 128;
List_Integer___init(self);
#ifdef SPITE_TRACKS_List_Integer
spite_track_List_Integer(self);
#endif
return self;
}
List_Integer* List_Integer___make(void) {
List_Integer* self = List_Integer___allocate();
return self;
}
static inline void List_Integer___release(List_Integer* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
List_Integer___free(self);
}
void List_Integer___free(List_Integer* self) {
List_Integer_drop(self);
spite_folded_Memory_Heap___release(self->heap_);
spite_folded_TypedMemory__Integer___release(self->values_);
#ifdef SPITE_TRACKS_List_Integer
spite_untrack_List_Integer(self);
#endif
#ifdef SPITE_WEAK_List_Integer
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
self->header.class_id = 146;
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
self->header.class_id = 148;
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
self->header.class_id = 150;
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
void Runner__Mover_Moving___init(Runner__Mover_Moving* self) {
self->found_ = List_Integer___make();
self->missing_ = false;
}
Runner__Mover_Moving* Runner__Mover_Moving___allocate(void) {
Runner__Mover_Moving* self = Runner__Mover_Moving___pool_take();
self->header.ref_count = 1;
self->header.class_id = 156;
Runner__Mover_Moving___init(self);
#ifdef SPITE_TRACKS_Runner__Mover_Moving
spite_track_Runner__Mover_Moving(self);
#endif
return self;
}
Runner__Mover_Moving* Runner__Mover_Moving___make(void) {
Runner__Mover_Moving* self = Runner__Mover_Moving___allocate();
return self;
}
static inline void Runner__Mover_Moving___release(Runner__Mover_Moving* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Runner__Mover_Moving___free(self);
}
void Runner__Mover_Moving___free(Runner__Mover_Moving* self) {
List_Integer___release(self->found_);
#ifdef SPITE_TRACKS_Runner__Mover_Moving
spite_untrack_Runner__Mover_Moving(self);
#endif
#ifdef SPITE_WEAK_Runner__Mover_Moving
spite_weak_object_freed(self);
#endif
Runner__Mover_Moving___pool_give(self);
}
void Column__Position___init(Column__Position* self) {
self->set_ = SparseSet___make();
self->values_ = Vector__Position___make();
}
Column__Position* Column__Position___allocate(void) {
Column__Position* self = (Column__Position*)SPITE_MALLOC(sizeof(Column__Position));
self->header.ref_count = 1;
self->header.class_id = 157;
Column__Position___init(self);
#ifdef SPITE_TRACKS_Column__Position
spite_track_Column__Position(self);
#endif
return self;
}
Column__Position* Column__Position___make(void) {
Column__Position* self = Column__Position___allocate();
return self;
}
void Column__Position___release(Column__Position* self) { (void)self; }
void Column__Position___destroy(Column__Position* self) {
if (self == 0) return;
Column__Position___discard(self);
}
void Column__Position___discard(Column__Position* self) {
if (self == 0) return;
SparseSet___release(self->set_);
Vector__Position___release(self->values_);
#ifdef SPITE_TRACKS_Column__Position
spite_untrack_Column__Position(self);
#endif
#ifdef SPITE_WEAK_Column__Position
spite_weak_object_freed(self);
#endif
spite_singleton_free_later(self);
}
void Vector__Position___init(Vector__Position* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->values_ = spite_singleton_InlineMemory__Position();
self->items_ = ((int64_t)(0));
self->item_count_ = 0;
self->capacity_ = 0;
}
Vector__Position* Vector__Position___allocate(void) {
Vector__Position* self = (Vector__Position*)SPITE_MALLOC(sizeof(Vector__Position));
self->header.ref_count = 1;
self->header.class_id = 158;
Vector__Position___init(self);
#ifdef SPITE_TRACKS_Vector__Position
spite_track_Vector__Position(self);
#endif
return self;
}
Vector__Position* Vector__Position___make(void) {
Vector__Position* self = Vector__Position___allocate();
return self;
}
static inline void Vector__Position___release(Vector__Position* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Vector__Position___free(self);
}
void Vector__Position___free(Vector__Position* self) {
Vector__Position_drop(self);
spite_folded_Memory_Heap___release(self->heap_);
spite_folded_InlineMemory__Position___release(self->values_);
#ifdef SPITE_TRACKS_Vector__Position
spite_untrack_Vector__Position(self);
#endif
#ifdef SPITE_WEAK_Vector__Position
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void Column__Velocity___init(Column__Velocity* self) {
self->set_ = SparseSet___make();
self->values_ = Vector__Velocity___make();
}
Column__Velocity* Column__Velocity___allocate(void) {
Column__Velocity* self = (Column__Velocity*)SPITE_MALLOC(sizeof(Column__Velocity));
self->header.ref_count = 1;
self->header.class_id = 160;
Column__Velocity___init(self);
#ifdef SPITE_TRACKS_Column__Velocity
spite_track_Column__Velocity(self);
#endif
return self;
}
Column__Velocity* Column__Velocity___make(void) {
Column__Velocity* self = Column__Velocity___allocate();
return self;
}
void Column__Velocity___destroy(Column__Velocity* self) {
if (self == 0) return;
Column__Velocity___discard(self);
}
void Column__Velocity___discard(Column__Velocity* self) {
if (self == 0) return;
SparseSet___release(self->set_);
Vector__Velocity___release(self->values_);
#ifdef SPITE_TRACKS_Column__Velocity
spite_untrack_Column__Velocity(self);
#endif
#ifdef SPITE_WEAK_Column__Velocity
spite_weak_object_freed(self);
#endif
spite_singleton_free_later(self);
}
void Vector__Velocity___init(Vector__Velocity* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->values_ = spite_singleton_InlineMemory__Velocity();
self->items_ = ((int64_t)(0));
self->item_count_ = 0;
self->capacity_ = 0;
}
Vector__Velocity* Vector__Velocity___allocate(void) {
Vector__Velocity* self = (Vector__Velocity*)SPITE_MALLOC(sizeof(Vector__Velocity));
self->header.ref_count = 1;
self->header.class_id = 161;
Vector__Velocity___init(self);
#ifdef SPITE_TRACKS_Vector__Velocity
spite_track_Vector__Velocity(self);
#endif
return self;
}
Vector__Velocity* Vector__Velocity___make(void) {
Vector__Velocity* self = Vector__Velocity___allocate();
return self;
}
static inline void Vector__Velocity___release(Vector__Velocity* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Vector__Velocity___free(self);
}
void Vector__Velocity___free(Vector__Velocity* self) {
Vector__Velocity_drop(self);
spite_folded_Memory_Heap___release(self->heap_);
spite_folded_InlineMemory__Velocity___release(self->values_);
#ifdef SPITE_TRACKS_Vector__Velocity
spite_untrack_Vector__Velocity(self);
#endif
#ifdef SPITE_WEAK_Vector__Velocity
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
void Benchmark__Long___init(Benchmark__Long* self) {
self->_clock_ = spite_singleton_Clock();
self->answer_ = 0;
self->duration_ = Duration___default();
}
Benchmark__Long* Benchmark__Long___allocate(void) {
Benchmark__Long* self = Benchmark__Long___pool_take();
self->header.ref_count = 1;
self->header.class_id = 180;
Benchmark__Long___init(self);
#ifdef SPITE_TRACKS_Benchmark__Long
spite_track_Benchmark__Long(self);
#endif
return self;
}
Benchmark__Long* Benchmark__Long___make(Spite_Function* work_) {
Benchmark__Long* self = Benchmark__Long___allocate();
Benchmark__Long_Benchmark(self, work_);
return self;
}
static inline void Benchmark__Long___release(Benchmark__Long* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Benchmark__Long___free(self);
}
void Benchmark__Long___free(Benchmark__Long* self) {
Clock___release(self->_clock_);
Duration___release(self->duration_);
#ifdef SPITE_TRACKS_Benchmark__Long
spite_untrack_Benchmark__Long(self);
#endif
#ifdef SPITE_WEAK_Benchmark__Long
spite_weak_object_freed(self);
#endif
Benchmark__Long___pool_give(self);
}
Spite_Class* spite_class_object_Long(void) {
if (SPITE_SINGLETON_FOUND(spite_class_object_Long_ready)) return Spite_Class___retain(spite_class_object_Long_cache);
spite_described_enter();
if (spite_class_object_Long_cache == 0) {
spite_class_object_Long_cache = Spite_Class___make(spite_symbol_4);
}
spite_described_leave(&spite_class_object_Long_ready);
return Spite_Class___retain(spite_class_object_Long_cache);
}
void Console_Printable___release(Console_Printable self) {
if (self.plain != 0 || self.value.object == 0) return;
if (((self).tag == 0) && ((self).plain == 0)) { spite_string_box_release(self.value.object); return; }
}
SpiteString Console_Printable___call_to_string(Console_Printable self) {
if (((self).tag == 0) && ((self).plain == 0)) return SpiteString_to_string((((SpiteBox_SpiteString*)(self).value.object)->value));
if ((self).tag == 165) return SpiteLong_to_string(SPITE_TAGGED_VALUE(self, int64_t));
if ((self).tag == 181) return SpiteInteger_to_string(SPITE_TAGGED_VALUE(self, int32_t));
fputs("spite.crash\tPrintable.to_string was called on a value of a class it was not compiled for\n", stderr);
abort();
}
Position* Mover_Moving___peek_position(Mover_Moving self) {
return ((Object_entity_Entity_position_Position_velocity_Velocity*)self)->position_;
return 0;
}
Velocity* Mover_Moving___peek_velocity(Mover_Moving self) {
return ((Object_entity_Entity_position_Position_velocity_Velocity*)self)->velocity_;
return 0;
}
Entity* Mover_Moving___peek_entity(Mover_Moving self) {
return ((Object_entity_Entity_position_Position_velocity_Velocity*)self)->entity_;
return 0;
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
SpiteString SpiteMemory_Address_text(int64_t self, int64_t length_) {
return spite_string_from_bytes((const char*)(intptr_t)self, length_);
}
SpiteString SpiteMemory_Address_to_string(int64_t self) {
int64_t number_ = SpiteMemory_Address_to_long(self);
SpiteString spite_temp_85 = SpiteLong_to_string(number_);
return spite_temp_85;
}
int64_t Memory_Arena_allocate(Memory_Arena* self, int64_t bytes_) {
int64_t rounded_ = ({ int64_t spite_temp_86 = (({ int64_t spite_temp_87 = bytes_; int64_t spite_temp_88 = SpiteInteger_to_long(15); int64_t spite_temp_89; if (__builtin_expect(__builtin_add_overflow(spite_temp_87, spite_temp_88, &spite_temp_89), 0)) spite_overflowed("bytes + 15", "a Long", "+", (int64_t)spite_temp_87, (int64_t)spite_temp_88, spite_site_10()); spite_temp_89; }) / SpiteInteger_to_long(16)); int64_t spite_temp_90 = SpiteInteger_to_long(16); int64_t spite_temp_91; if (__builtin_expect(__builtin_mul_overflow(spite_temp_86, spite_temp_90, &spite_temp_91), 0)) spite_overflowed("(bytes + 15) / 16 * 16", "a Long", "*", (int64_t)spite_temp_86, (int64_t)spite_temp_90, spite_site_10()); spite_temp_91; });
if (((((self->_block_ == ((int64_t)(0)))) || ((({ int64_t spite_temp_92 = self->_used_; int64_t spite_temp_93 = rounded_; int64_t spite_temp_94; if (__builtin_expect(__builtin_add_overflow(spite_temp_92, spite_temp_93, &spite_temp_94), 0)) spite_overflowed("_used + rounded", "a Long", "+", (int64_t)spite_temp_92, (int64_t)spite_temp_93, spite_site_11()); spite_temp_94; }) > self->_end_))))) {
Memory_Arena_start_block(self, rounded_);
}
int64_t address_ = (self->_block_ + ((int64_t)(self->_used_)));
self->_used_ = ({ int64_t spite_temp_95 = self->_used_; int64_t spite_temp_96 = rounded_; int64_t spite_temp_97; if (__builtin_expect(__builtin_add_overflow(spite_temp_95, spite_temp_96, &spite_temp_97), 0)) spite_overflowed("_used + rounded", "a Long", "+", (int64_t)spite_temp_95, (int64_t)spite_temp_96, spite_site_12()); spite_temp_97; });
int64_t spite_temp_98 = address_;
return spite_temp_98;
}
void Memory_Arena_free(Memory_Arena* self, int64_t _address_) {
}
void Memory_Arena_start_block(Memory_Arena* self, int64_t at_least_) {
int64_t size_ = self->_block_bytes_;
if (((({ int64_t spite_temp_99 = at_least_; int64_t spite_temp_100 = SpiteInteger_to_long(16); int64_t spite_temp_101; if (__builtin_expect(__builtin_add_overflow(spite_temp_99, spite_temp_100, &spite_temp_101), 0)) spite_overflowed("at_least + 16", "a Long", "+", (int64_t)spite_temp_99, (int64_t)spite_temp_100, spite_site_13()); spite_temp_101; }) > size_))) {
size_ = ({ int64_t spite_temp_102 = at_least_; int64_t spite_temp_103 = SpiteInteger_to_long(16); int64_t spite_temp_104; if (__builtin_expect(__builtin_add_overflow(spite_temp_102, spite_temp_103, &spite_temp_104), 0)) spite_overflowed("at_least + 16", "a Long", "+", (int64_t)spite_temp_102, (int64_t)spite_temp_103, spite_site_14()); spite_temp_104; });
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
SpiteString spite_temp_105 = SpiteString___retain(starting_name_);
SpiteString___release(self->_name_);
self->_name_ = spite_temp_105;
SpiteString___release(starting_name_);
}
void Spite_Function_Function(Spite_Function* self, SpiteString starting_name_, Spite_Class* starting_returns_) {
SpiteString spite_temp_106 = SpiteString___retain(starting_name_);
SpiteString___release(self->_name_);
self->_name_ = spite_temp_106;
Spite_Class* spite_temp_107 = Spite_Class___retain(starting_returns_);
Spite_Class___release(self->_returns_);
self->_returns_ = spite_temp_107;
Spite_Class___release(starting_returns_);
SpiteString___release(starting_name_);
}
void Naive_Naive(Naive* self) {
Benchmark__Long* benchmark_ = Benchmark__Long___make(spite_function_value_Naive_spawn_and_move(self));
List_Console_Printable spite_framed_1; Console_Printable spite_framed_1_items[4]; int32_t spite_framed_1_count = 0;
Console_print(self->console_, ({ spite_framed_1_items[0] = spite_tagged_object(0, ((void*)&spite_lit_11_box)); spite_framed_1_items[1] = spite_tagged_SpiteLong((benchmark_)->answer_); spite_framed_1_items[2] = spite_tagged_object(0, ((void*)&spite_lit_12_box)); spite_framed_1_items[3] = spite_tagged_SpiteInteger((self->mover_)->marked_); spite_framed_1_count = 4; List_Console_Printable___framed(&spite_framed_1, (int64_t)(intptr_t)spite_framed_1_items, 4); }));
for (int32_t spite_index = 0; spite_index < spite_framed_1_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_1_items[spite_index]); }
int64_t microseconds_ = Duration_total((benchmark_)->duration_, Duration_Unit_microseconds);
List_Console_Printable spite_framed_2; Console_Printable spite_framed_2_items[1]; int32_t spite_framed_2_count = 0;
Console_error(self->console_, ({ spite_framed_2_items[0] = spite_tagged_object(0, spite_box_SpiteString(({ char spite_temp_108_digits[24]; SpiteString spite_temp_108 = SPITE_STATIC_STRING(spite_temp_108_digits, spite_long_digits(spite_temp_108_digits, (int64_t)(microseconds_))); SpiteString spite_temp_109[] = {spite_lit_13, spite_temp_108}; SpiteString spite_temp_110 = spite_string_join(2, spite_temp_109); spite_temp_110; }))); spite_framed_2_count = 1; List_Console_Printable___framed(&spite_framed_2, (int64_t)(intptr_t)spite_framed_2_items, 1); }));
for (int32_t spite_index = 0; spite_index < spite_framed_2_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_2_items[spite_index]); }
Benchmark__Long___release(benchmark_);
}
int64_t Naive_spawn_and_move(Naive* self) {
Naive_spawn_all(self);
Naive_run_ticks(self, 50);
int64_t spite_temp_111 = Vector__Position_sum_place((self->positions_)->values_);
return spite_temp_111;
}
void Naive_spawn_all(Naive* self) {
int32_t entity_ = 0;
while (((entity_ < self->entity_count_))) {
Position spite_slot_1;
Position* position_ = Position___make_into(&spite_slot_1);
Column__Position_insert___held_1(self->positions_, entity_, position_);
entity_ = (entity_ + 1);
}
int32_t backwards_ = ({ int32_t spite_temp_112 = self->entity_count_; int32_t spite_temp_113 = 1; int32_t spite_temp_114; if (__builtin_expect(__builtin_sub_overflow(spite_temp_112, spite_temp_113, &spite_temp_114), 0)) spite_overflowed("entity_count - 1", "an Integer", "-", (int64_t)spite_temp_112, (int64_t)spite_temp_113, spite_site_15()); spite_temp_114; });
while (((backwards_ >= 0))) {
Naive_push(self, backwards_);
backwards_ = ({ int32_t spite_temp_115 = backwards_; int32_t spite_temp_116 = 1; int32_t spite_temp_117; if (__builtin_expect(__builtin_sub_overflow(spite_temp_115, spite_temp_116, &spite_temp_117), 0)) spite_overflowed("backwards - 1", "an Integer", "-", (int64_t)spite_temp_115, (int64_t)spite_temp_116, spite_site_16()); spite_temp_117; });
}
}
void Naive_push(Naive* self, int32_t entity_) {
if ((((entity_ % 3) != 0))) {
Velocity spite_slot_2;
Velocity* velocity_ = Velocity___make_into(&spite_slot_2, ((entity_ % 13) - 6), ((entity_ % 7) - 3));
Column__Velocity_insert___held_1(self->velocities_, entity_, velocity_);
}
}
void Naive_run_ticks(Naive* self, int32_t count_) {
int32_t tick_ = 0;
while (((tick_ < count_))) {
Runner__Mover_Moving_run___held_0(self->runner_, self->mover_, self->entity_count_);
tick_ = (tick_ + 1);
}
}
void Entity_Entity(Entity* self, int32_t new_id_) {
self->id_ = new_id_;
}
int64_t Position_get_place(Position* self) {
int64_t wide_ = SpiteInteger_to_long(self->left_);
int64_t spite_temp_118 = (wide_ + SpiteInteger_to_long(self->top_));
return spite_temp_118;
}
void SparseSet_place(SparseSet* self, int32_t entity_) {
while (((List_Integer_count(self->dense_of_) <= entity_))) {
List_Integer_append(self->dense_of_, 0);
}
List_Integer_append(self->entities_, entity_);
List_Integer_set_at(self->dense_of_, entity_, List_Integer_count(self->entities_));
}
int32_t SparseSet_dense_index(SparseSet* self, int32_t entity_) {
if (((entity_ >= List_Integer_count(self->dense_of_)))) {
int32_t spite_temp_119 = (-(1));
return spite_temp_119;
}
int32_t spite_temp_120 = ({ int32_t spite_temp_121 = ({ Nullable_Integer spite_temp_122 = List_Integer_get_at(self->dense_of_, entity_); if (__builtin_expect(!spite_temp_122.has_value, 0)) spite_outside_list("dense_of[entity]", spite_site_20()); spite_temp_122.value; }); int32_t spite_temp_123 = 1; int32_t spite_temp_124; if (__builtin_expect(__builtin_sub_overflow(spite_temp_121, spite_temp_123, &spite_temp_124), 0)) spite_overflowed("dense_of[entity] - 1", "an Integer", "-", (int64_t)spite_temp_121, (int64_t)spite_temp_123, spite_site_20()); spite_temp_124; });
return spite_temp_120;
}
void Velocity_Velocity(Velocity* self, int32_t new_across_, int32_t new_down_) {
self->across_ = new_across_;
self->down_ = new_down_;
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
({ Spite_Allocator spite_temp_125 = SPITE_ALLOCATOR_List_String(self, spite_singleton_Memory_Heap); int64_t spite_temp_126 = self->items_; if (((SpiteHeader*)(spite_temp_125))->class_id == 94) { Memory_Arena_free(((Memory_Arena*)spite_temp_125), spite_temp_126); } else if (((SpiteHeader*)(spite_temp_125))->class_id == 95) { Memory_Heap_free(((Memory_Heap*)spite_temp_125), spite_temp_126); } });
}
}
void TypedMemory__String_release_value(TypedMemory__String* self, int64_t address_, int32_t index_) {
SpiteString___release(((SpiteString*)(intptr_t)address_)[index_]);
}
int32_t List_Integer_count(List_Integer* self) {
int32_t spite_temp_127 = self->item_count_;
return spite_temp_127;
}
void List_Integer_append(List_Integer* self, int32_t value_) {
List_Integer_make_room(self);
TypedMemory__Integer_write_value(self->values_, self->items_, self->item_count_, value_);
self->item_count_ = ({ int32_t spite_temp_128 = self->item_count_; int32_t spite_temp_129 = 1; int32_t spite_temp_130; if (__builtin_expect(__builtin_add_overflow(spite_temp_128, spite_temp_129, &spite_temp_130), 0)) spite_overflowed("item_count + 1", "an Integer", "+", (int64_t)spite_temp_128, (int64_t)spite_temp_129, spite_site_21()); spite_temp_130; });
}
Nullable_Integer List_Integer_get_at(List_Integer* self, int32_t index_) {
if ((((index_ >= 0))) && (((index_ < self->item_count_)))) {
Nullable_Integer spite_temp_131 = ((Nullable_Integer){ .has_value = true, .value = TypedMemory__Integer_read_value(self->values_, self->items_, index_) });
return spite_temp_131;
}
Nullable_Integer spite_temp_132 = ((Nullable_Integer){ .has_value = false, .value = 0 });
return spite_temp_132;
}
void List_Integer_set_at(List_Integer* self, int32_t index_, int32_t value_) {
if (!(((index_ >= 0)))) {
spite_failed_1(index_, value_, self);
}
if (!(((index_ < self->item_count_)))) {
spite_failed_2(index_, self, value_);
}
TypedMemory__Integer_release_value(self->values_, self->items_, index_);
TypedMemory__Integer_write_value(self->values_, self->items_, index_, value_);
}
static SPITE_CRASH_REPORT void spite_failed_1(int32_t index_, int32_t value_, List_Integer* self) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_24(), stderr);
fputs("\tindex=", stderr);
{ SpiteString spite_temp_133 = SpiteInteger_to_string(index_); fwrite(spite_string_bytes(&spite_temp_133), 1, (size_t)spite_string_length(spite_temp_133), stderr); SpiteString___release(spite_temp_133); }
fputs("\tvalue=", stderr);
{ SpiteString spite_temp_134 = SpiteInteger_to_string(value_); spite_crash_text(spite_string_bytes(&spite_temp_134), spite_string_length(spite_temp_134)); SpiteString___release(spite_temp_134); }
fputs("\titems=", stderr);
{ SpiteString spite_temp_135 = SpiteMemory_Address_to_string(self->items_); spite_crash_text(spite_string_bytes(&spite_temp_135), spite_string_length(spite_temp_135)); SpiteString___release(spite_temp_135); }
fputs("\titem_count=", stderr);
{ SpiteString spite_temp_136 = SpiteInteger_to_string(self->item_count_); spite_crash_text(spite_string_bytes(&spite_temp_136), spite_string_length(spite_temp_136)); SpiteString___release(spite_temp_136); }
fputs("\tcapacity=", stderr);
{ SpiteString spite_temp_137 = SpiteInteger_to_string(self->capacity_); spite_crash_text(spite_string_bytes(&spite_temp_137), spite_string_length(spite_temp_137)); SpiteString___release(spite_temp_137); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
static SPITE_CRASH_REPORT void spite_failed_2(int32_t index_, List_Integer* self, int32_t value_) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_25(), stderr);
fputs("\tindex=", stderr);
{ SpiteString spite_temp_138 = SpiteInteger_to_string(index_); fwrite(spite_string_bytes(&spite_temp_138), 1, (size_t)spite_string_length(spite_temp_138), stderr); SpiteString___release(spite_temp_138); }
fputs("\titem_count=", stderr);
{ SpiteString spite_temp_139 = SpiteInteger_to_string(self->item_count_); fwrite(spite_string_bytes(&spite_temp_139), 1, (size_t)spite_string_length(spite_temp_139), stderr); SpiteString___release(spite_temp_139); }
fputs("\tvalue=", stderr);
{ SpiteString spite_temp_140 = SpiteInteger_to_string(value_); spite_crash_text(spite_string_bytes(&spite_temp_140), spite_string_length(spite_temp_140)); SpiteString___release(spite_temp_140); }
fputs("\titems=", stderr);
{ SpiteString spite_temp_141 = SpiteMemory_Address_to_string(self->items_); spite_crash_text(spite_string_bytes(&spite_temp_141), spite_string_length(spite_temp_141)); SpiteString___release(spite_temp_141); }
fputs("\tcapacity=", stderr);
{ SpiteString spite_temp_142 = SpiteInteger_to_string(self->capacity_); spite_crash_text(spite_string_bytes(&spite_temp_142), spite_string_length(spite_temp_142)); SpiteString___release(spite_temp_142); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
void List_Integer_clear(List_Integer* self) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
TypedMemory__Integer_release_value(self->values_, self->items_, index_);
index_ = (index_ + 1);
}
self->item_count_ = 0;
}
void List_Integer_drop(List_Integer* self) {
List_Integer_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_143 = SPITE_ALLOCATOR_List_Integer(self, spite_singleton_Memory_Heap); int64_t spite_temp_144 = self->items_; if (((SpiteHeader*)(spite_temp_143))->class_id == 94) { Memory_Arena_free(((Memory_Arena*)spite_temp_143), spite_temp_144); } else if (((SpiteHeader*)(spite_temp_143))->class_id == 95) { Memory_Heap_free(((Memory_Heap*)spite_temp_143), spite_temp_144); } });
}
}
void List_Integer_make_room(List_Integer* self) {
if (((self->item_count_ == self->capacity_))) {
List_Integer__grow(self);
}
}
void List_Integer__grow(List_Integer* self) {
int32_t grown_ = ({ int32_t spite_temp_145 = self->capacity_; int32_t spite_temp_146 = 2; int32_t spite_temp_147; if (__builtin_expect(__builtin_mul_overflow(spite_temp_145, spite_temp_146, &spite_temp_147), 0)) spite_overflowed("capacity * 2", "an Integer", "*", (int64_t)spite_temp_145, (int64_t)spite_temp_146, spite_site_22()); spite_temp_147; });
if (((self->capacity_ == 0))) {
grown_ = 4;
}
int64_t bytes_ = TypedMemory__Integer_value_bytes(self->values_);
self->items_ = List_Integer__resized(self, ({ int64_t spite_temp_148 = bytes_; int64_t spite_temp_149 = SpiteInteger_to_long(self->capacity_); int64_t spite_temp_150; if (__builtin_expect(__builtin_mul_overflow(spite_temp_148, spite_temp_149, &spite_temp_150), 0)) spite_overflowed("bytes * capacity", "a Long", "*", (int64_t)spite_temp_148, (int64_t)spite_temp_149, spite_site_23()); spite_temp_150; }), ({ int64_t spite_temp_151 = bytes_; int64_t spite_temp_152 = SpiteInteger_to_long(grown_); int64_t spite_temp_153; if (__builtin_expect(__builtin_mul_overflow(spite_temp_151, spite_temp_152, &spite_temp_153), 0)) spite_overflowed("bytes * grown", "a Long", "*", (int64_t)spite_temp_151, (int64_t)spite_temp_152, spite_site_23()); spite_temp_153; }));
self->capacity_ = grown_;
}
int64_t List_Integer__resized(List_Integer* self, int64_t old_bytes_, int64_t new_bytes_) {
if ((({ Spite_Allocator spite_temp_154 = SPITE_ALLOCATOR_List_Integer(self, spite_singleton_Memory_Heap); bool spite_temp_155 = (((SpiteHeader*)(spite_temp_154))->class_id == 95); spite_temp_155; }))) {
int64_t spite_temp_156 = Memory_Heap_resize(self->heap_, self->items_, new_bytes_);
return spite_temp_156;
}
int64_t moved_ = ({ Spite_Allocator spite_temp_157 = SPITE_ALLOCATOR_List_Integer(self, spite_singleton_Memory_Heap); int64_t spite_temp_158 = new_bytes_; int64_t spite_temp_159 = 0; if (((SpiteHeader*)(spite_temp_157))->class_id == 94) { spite_temp_159 = Memory_Arena_allocate(((Memory_Arena*)spite_temp_157), spite_temp_158); } else if (((SpiteHeader*)(spite_temp_157))->class_id == 95) { spite_temp_159 = Memory_Heap_allocate(((Memory_Heap*)spite_temp_157), spite_temp_158); } spite_temp_159; });
if (((self->items_ != ((int64_t)(0))))) {
SpiteMemory_Address_copy_to(self->items_, moved_, old_bytes_);
({ Spite_Allocator spite_temp_160 = SPITE_ALLOCATOR_List_Integer(self, spite_singleton_Memory_Heap); int64_t spite_temp_161 = self->items_; if (((SpiteHeader*)(spite_temp_160))->class_id == 94) { Memory_Arena_free(((Memory_Arena*)spite_temp_160), spite_temp_161); } else if (((SpiteHeader*)(spite_temp_160))->class_id == 95) { Memory_Heap_free(((Memory_Heap*)spite_temp_160), spite_temp_161); } });
}
int64_t spite_temp_162 = moved_;
return spite_temp_162;
}
int32_t TypedMemory__Integer_read_value(TypedMemory__Integer* self, int64_t address_, int32_t index_) {
return ((int32_t*)(intptr_t)address_)[index_];
}
void TypedMemory__Integer_write_value(TypedMemory__Integer* self, int64_t address_, int32_t index_, int32_t value_) {
((int32_t*)(intptr_t)address_)[index_] = value_;
}
void TypedMemory__Integer_release_value(TypedMemory__Integer* self, int64_t address_, int32_t index_) {

}
int64_t TypedMemory__Integer_value_bytes(TypedMemory__Integer* self) {
return (int64_t)sizeof(int32_t);
}
void List_Spite_AttributeDeclaration_drop(List_Spite_AttributeDeclaration* self) {
List_Spite_AttributeDeclaration_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_163 = SPITE_ALLOCATOR_List_Spite_AttributeDeclaration(self, spite_singleton_Memory_Heap); int64_t spite_temp_164 = self->items_; if (((SpiteHeader*)(spite_temp_163))->class_id == 94) { Memory_Arena_free(((Memory_Arena*)spite_temp_163), spite_temp_164); } else if (((SpiteHeader*)(spite_temp_163))->class_id == 95) { Memory_Heap_free(((Memory_Heap*)spite_temp_163), spite_temp_164); } });
}
}
void List_Spite_Function_drop(List_Spite_Function* self) {
List_Spite_Function_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_165 = SPITE_ALLOCATOR_List_Spite_Function(self, spite_singleton_Memory_Heap); int64_t spite_temp_166 = self->items_; if (((SpiteHeader*)(spite_temp_165))->class_id == 94) { Memory_Arena_free(((Memory_Arena*)spite_temp_165), spite_temp_166); } else if (((SpiteHeader*)(spite_temp_165))->class_id == 95) { Memory_Heap_free(((Memory_Heap*)spite_temp_165), spite_temp_166); } });
}
}
void List_Spite_Argument_drop(List_Spite_Argument* self) {
List_Spite_Argument_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_167 = SPITE_ALLOCATOR_List_Spite_Argument(self, spite_singleton_Memory_Heap); int64_t spite_temp_168 = self->items_; if (((SpiteHeader*)(spite_temp_167))->class_id == 94) { Memory_Arena_free(((Memory_Arena*)spite_temp_167), spite_temp_168); } else if (((SpiteHeader*)(spite_temp_167))->class_id == 95) { Memory_Heap_free(((Memory_Heap*)spite_temp_167), spite_temp_168); } });
}
}
void List_Spite_Class_drop(List_Spite_Class* self) {
List_Spite_Class_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_169 = SPITE_ALLOCATOR_List_Spite_Class(self, spite_singleton_Memory_Heap); int64_t spite_temp_170 = self->items_; if (((SpiteHeader*)(spite_temp_169))->class_id == 94) { Memory_Arena_free(((Memory_Arena*)spite_temp_169), spite_temp_170); } else if (((SpiteHeader*)(spite_temp_169))->class_id == 95) { Memory_Heap_free(((Memory_Heap*)spite_temp_169), spite_temp_170); } });
}
}
void List_Spite_Namespace_drop(List_Spite_Namespace* self) {
List_Spite_Namespace_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_171 = SPITE_ALLOCATOR_List_Spite_Namespace(self, spite_singleton_Memory_Heap); int64_t spite_temp_172 = self->items_; if (((SpiteHeader*)(spite_temp_171))->class_id == 94) { Memory_Arena_free(((Memory_Arena*)spite_temp_171), spite_temp_172); } else if (((SpiteHeader*)(spite_temp_171))->class_id == 95) { Memory_Heap_free(((Memory_Heap*)spite_temp_171), spite_temp_172); } });
}
}
void Runner__Mover_Moving_run___held_0(Runner__Mover_Moving* self, Mover* system_, int32_t entity_count_) {
int32_t entity_ = 0;
while (((entity_ < entity_count_))) {
List_Integer_clear(self->found_);
self->missing_ = false;
Runner__Mover_Moving_find_attributes(self, entity_);
if (((!(self->missing_)))) {
Nullable_Integer spite_temp_173 = List_Integer_get_at(self->found_, 1);
if (!(spite_temp_173.has_value)) {
spite_failed_3(entity_count_, entity_, self);
}
Position* spite_temp_174 = Vector__Position_get_at((spite_singleton_Column__Position())->values_, spite_temp_173.value);
if (!(((spite_temp_174) != 0))) {
spite_failed_4(entity_count_, entity_, self);
}
Nullable_Integer spite_temp_175 = List_Integer_get_at(self->found_, 2);
if (!(spite_temp_175.has_value)) {
spite_failed_5(entity_count_, entity_, self);
}
Velocity* spite_temp_176 = Vector__Velocity_get_at((spite_singleton_Column__Velocity())->values_, spite_temp_175.value);
if (!(((spite_temp_176) != 0))) {
spite_failed_6(entity_count_, entity_, self);
}
Entity spite_temp_177 = { { 1, 112 } };
Entity___init(&spite_temp_177);
Entity_Entity(&spite_temp_177, entity_);
Object_entity_Entity_position_Position_velocity_Velocity spite_temp_178 = { { 1, 182 } };
spite_temp_178.entity_ = (&spite_temp_177);
spite_temp_178.position_ = spite_temp_174;
spite_temp_178.velocity_ = spite_temp_176;
Mover_Moving row_ = ((Mover_Moving)((&spite_temp_178)));


Mover_update_each___lent_0(system_, row_);
}
entity_ = (entity_ + 1);
}
}
static SPITE_CRASH_REPORT void spite_failed_3(int32_t entity_count_, int32_t entity_, Runner__Mover_Moving* self) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_26(), stderr);
fputs("\tentity_count=", stderr);
{ SpiteString spite_temp_179 = SpiteInteger_to_string(entity_count_); spite_crash_text(spite_string_bytes(&spite_temp_179), spite_string_length(spite_temp_179)); SpiteString___release(spite_temp_179); }
fputs("\tentity=", stderr);
{ SpiteString spite_temp_180 = SpiteInteger_to_string(entity_); spite_crash_text(spite_string_bytes(&spite_temp_180), spite_string_length(spite_temp_180)); SpiteString___release(spite_temp_180); }
fputs("\tmissing=", stderr);
{ SpiteString spite_temp_181 = SpiteBoolean_to_string(self->missing_); spite_crash_text(spite_string_bytes(&spite_temp_181), spite_string_length(spite_temp_181)); SpiteString___release(spite_temp_181); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
static SPITE_CRASH_REPORT void spite_failed_4(int32_t entity_count_, int32_t entity_, Runner__Mover_Moving* self) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_27(), stderr);
fputs("\tentity_count=", stderr);
{ SpiteString spite_temp_182 = SpiteInteger_to_string(entity_count_); spite_crash_text(spite_string_bytes(&spite_temp_182), spite_string_length(spite_temp_182)); SpiteString___release(spite_temp_182); }
fputs("\tentity=", stderr);
{ SpiteString spite_temp_183 = SpiteInteger_to_string(entity_); spite_crash_text(spite_string_bytes(&spite_temp_183), spite_string_length(spite_temp_183)); SpiteString___release(spite_temp_183); }
fputs("\tmissing=", stderr);
{ SpiteString spite_temp_184 = SpiteBoolean_to_string(self->missing_); spite_crash_text(spite_string_bytes(&spite_temp_184), spite_string_length(spite_temp_184)); SpiteString___release(spite_temp_184); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
static SPITE_CRASH_REPORT void spite_failed_5(int32_t entity_count_, int32_t entity_, Runner__Mover_Moving* self) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_28(), stderr);
fputs("\tentity_count=", stderr);
{ SpiteString spite_temp_185 = SpiteInteger_to_string(entity_count_); spite_crash_text(spite_string_bytes(&spite_temp_185), spite_string_length(spite_temp_185)); SpiteString___release(spite_temp_185); }
fputs("\tentity=", stderr);
{ SpiteString spite_temp_186 = SpiteInteger_to_string(entity_); spite_crash_text(spite_string_bytes(&spite_temp_186), spite_string_length(spite_temp_186)); SpiteString___release(spite_temp_186); }
fputs("\tmissing=", stderr);
{ SpiteString spite_temp_187 = SpiteBoolean_to_string(self->missing_); spite_crash_text(spite_string_bytes(&spite_temp_187), spite_string_length(spite_temp_187)); SpiteString___release(spite_temp_187); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
static SPITE_CRASH_REPORT void spite_failed_6(int32_t entity_count_, int32_t entity_, Runner__Mover_Moving* self) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_29(), stderr);
fputs("\tentity_count=", stderr);
{ SpiteString spite_temp_188 = SpiteInteger_to_string(entity_count_); spite_crash_text(spite_string_bytes(&spite_temp_188), spite_string_length(spite_temp_188)); SpiteString___release(spite_temp_188); }
fputs("\tentity=", stderr);
{ SpiteString spite_temp_189 = SpiteInteger_to_string(entity_); spite_crash_text(spite_string_bytes(&spite_temp_189), spite_string_length(spite_temp_189)); SpiteString___release(spite_temp_189); }
fputs("\tmissing=", stderr);
{ SpiteString spite_temp_190 = SpiteBoolean_to_string(self->missing_); spite_crash_text(spite_string_bytes(&spite_temp_190), spite_string_length(spite_temp_190)); SpiteString___release(spite_temp_190); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
void Runner__Mover_Moving_find_attributes(Runner__Mover_Moving* self, int32_t entity_) {
Runner__Mover_Moving_find_entity(self, entity_);
Runner__Mover_Moving_find_position(self, entity_);
Runner__Mover_Moving_find_velocity(self, entity_);
}
void Runner__Mover_Moving_find_entity(Runner__Mover_Moving* self, int32_t entity_) {
{
Runner__Mover_Moving_note(self, entity_);
}
}
void Runner__Mover_Moving_find_position(Runner__Mover_Moving* self, int32_t entity_) {
{
Column__Position* column_ = spite_singleton_Column__Position();
int32_t dense_ = SparseSet_dense_index((column_)->set_, entity_);
Runner__Mover_Moving_note(self, dense_);
Column__Position___release(column_);
}
}
void Runner__Mover_Moving_find_velocity(Runner__Mover_Moving* self, int32_t entity_) {
{
Column__Velocity* column_ = spite_singleton_Column__Velocity();
int32_t dense_ = SparseSet_dense_index((column_)->set_, entity_);
Runner__Mover_Moving_note(self, dense_);
spite_folded_Column__Velocity___release(column_);
}
}
void Column__Position_insert___held_1(Column__Position* self, int32_t entity_, Position* value_) {
SparseSet_place(self->set_, entity_);
Vector__Position_append(self->values_, Position___retain(value_));
}
void Vector__Position_append(Vector__Position* self, Position* value_) {
Vector__Position_make_room(self);
InlineMemory__Position_write_item(self->values_, self->items_, self->item_count_, Position___retain(value_));
self->item_count_ = ({ int32_t spite_temp_191 = self->item_count_; int32_t spite_temp_192 = 1; int32_t spite_temp_193; if (__builtin_expect(__builtin_add_overflow(spite_temp_191, spite_temp_192, &spite_temp_193), 0)) spite_overflowed("item_count + 1", "an Integer", "+", (int64_t)spite_temp_191, (int64_t)spite_temp_192, spite_site_30()); spite_temp_193; });
Position___release(value_);
}
Position* Vector__Position_get_at(Vector__Position* self, int32_t index_) {
if ((((index_ >= 0))) && (((index_ < self->item_count_)))) {
Position* spite_temp_194 = InlineMemory__Position_item_at(self->values_, self->items_, index_);
return spite_temp_194;
}
Position* spite_temp_195 = 0;
return spite_temp_195;
}
void Vector__Position_drop(Vector__Position* self) {
Vector__Position_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_196 = SPITE_ALLOCATOR_Vector__Position(self, spite_singleton_Memory_Heap); int64_t spite_temp_197 = self->items_; if (((SpiteHeader*)(spite_temp_196))->class_id == 94) { Memory_Arena_free(((Memory_Arena*)spite_temp_196), spite_temp_197); } else if (((SpiteHeader*)(spite_temp_196))->class_id == 95) { Memory_Heap_free(((Memory_Heap*)spite_temp_196), spite_temp_197); } });
}
}
void Vector__Position_make_room(Vector__Position* self) {
if (((self->item_count_ == self->capacity_))) {
Vector__Position__grow(self);
}
}
void Vector__Position__grow(Vector__Position* self) {
int32_t grown_ = ({ int32_t spite_temp_198 = self->capacity_; int32_t spite_temp_199 = 2; int32_t spite_temp_200; if (__builtin_expect(__builtin_mul_overflow(spite_temp_198, spite_temp_199, &spite_temp_200), 0)) spite_overflowed("capacity * 2", "an Integer", "*", (int64_t)spite_temp_198, (int64_t)spite_temp_199, spite_site_31()); spite_temp_200; });
if (((self->capacity_ == 0))) {
grown_ = 4;
}
int64_t old_bytes_ = InlineMemory__Position_block_bytes(self->values_, self->capacity_);
int64_t bytes_ = InlineMemory__Position_block_bytes(self->values_, grown_);
self->items_ = Vector__Position__resized(self, old_bytes_, bytes_);
self->capacity_ = grown_;
}
int64_t Vector__Position__resized(Vector__Position* self, int64_t old_bytes_, int64_t new_bytes_) {
if ((({ Spite_Allocator spite_temp_201 = SPITE_ALLOCATOR_Vector__Position(self, spite_singleton_Memory_Heap); bool spite_temp_202 = (((SpiteHeader*)(spite_temp_201))->class_id == 95); spite_temp_202; }))) {
int64_t spite_temp_203 = Memory_Heap_resize(self->heap_, self->items_, new_bytes_);
return spite_temp_203;
}
int64_t moved_ = ({ Spite_Allocator spite_temp_204 = SPITE_ALLOCATOR_Vector__Position(self, spite_singleton_Memory_Heap); int64_t spite_temp_205 = new_bytes_; int64_t spite_temp_206 = 0; if (((SpiteHeader*)(spite_temp_204))->class_id == 94) { spite_temp_206 = Memory_Arena_allocate(((Memory_Arena*)spite_temp_204), spite_temp_205); } else if (((SpiteHeader*)(spite_temp_204))->class_id == 95) { spite_temp_206 = Memory_Heap_allocate(((Memory_Heap*)spite_temp_204), spite_temp_205); } spite_temp_206; });
if (((self->items_ != ((int64_t)(0))))) {
SpiteMemory_Address_copy_to(self->items_, moved_, old_bytes_);
({ Spite_Allocator spite_temp_207 = SPITE_ALLOCATOR_Vector__Position(self, spite_singleton_Memory_Heap); int64_t spite_temp_208 = self->items_; if (((SpiteHeader*)(spite_temp_207))->class_id == 94) { Memory_Arena_free(((Memory_Arena*)spite_temp_207), spite_temp_208); } else if (((SpiteHeader*)(spite_temp_207))->class_id == 95) { Memory_Heap_free(((Memory_Heap*)spite_temp_207), spite_temp_208); } });
}
int64_t spite_temp_209 = moved_;
return spite_temp_209;
}
int64_t Vector__Position_sum_place(Vector__Position* self) {
int64_t total_ = SpiteInteger_to_long(0);
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
Position* item_ = InlineMemory__Position_item_at(self->values_, self->items_, index_);
total_ = ({ int64_t spite_temp_210 = total_; int64_t spite_temp_211 = Position_get_place(item_); int64_t spite_temp_212; if (__builtin_expect(__builtin_add_overflow(spite_temp_210, spite_temp_211, &spite_temp_212), 0)) spite_overflowed("total + item.attributes[member]", "a Long", "+", (int64_t)spite_temp_210, (int64_t)spite_temp_211, spite_site_32()); spite_temp_212; });
index_ = (index_ + 1);
}
int64_t spite_temp_213 = total_;
return spite_temp_213;
}
Position* InlineMemory__Position_item_at(InlineMemory__Position* self, int64_t address_, int32_t index_) {
return ((Position*)((char*)(intptr_t)address_ + (int64_t)index_ * ((int64_t)sizeof(Position) - (int64_t)sizeof(SpiteHeader))));
}
void InlineMemory__Position_write_item(InlineMemory__Position* self, int64_t address_, int32_t index_, Position* value_) {
Position* spite_slot = ((Position*)((char*)(intptr_t)address_ + (int64_t)index_ * ((int64_t)sizeof(Position) - (int64_t)sizeof(SpiteHeader))));
memcpy((char*)spite_slot + sizeof(SpiteHeader), (char*)value_ + sizeof(SpiteHeader), (size_t)((int64_t)sizeof(Position) - (int64_t)sizeof(SpiteHeader)));
Position___release(value_);
}
int64_t InlineMemory__Position_block_bytes(InlineMemory__Position* self, int32_t item_count_) {
return (int64_t)item_count_ * ((int64_t)sizeof(Position) - (int64_t)sizeof(SpiteHeader)) + (int64_t)sizeof(SpiteHeader);
}
void Column__Velocity_insert___held_1(Column__Velocity* self, int32_t entity_, Velocity* value_) {
SparseSet_place(self->set_, entity_);
Vector__Velocity_append(self->values_, Velocity___retain(value_));
}
void Vector__Velocity_append(Vector__Velocity* self, Velocity* value_) {
Vector__Velocity_make_room(self);
InlineMemory__Velocity_write_item(self->values_, self->items_, self->item_count_, Velocity___retain(value_));
self->item_count_ = ({ int32_t spite_temp_214 = self->item_count_; int32_t spite_temp_215 = 1; int32_t spite_temp_216; if (__builtin_expect(__builtin_add_overflow(spite_temp_214, spite_temp_215, &spite_temp_216), 0)) spite_overflowed("item_count + 1", "an Integer", "+", (int64_t)spite_temp_214, (int64_t)spite_temp_215, spite_site_30()); spite_temp_216; });
Velocity___release(value_);
}
Velocity* Vector__Velocity_get_at(Vector__Velocity* self, int32_t index_) {
if ((((index_ >= 0))) && (((index_ < self->item_count_)))) {
Velocity* spite_temp_217 = InlineMemory__Velocity_item_at(self->values_, self->items_, index_);
return spite_temp_217;
}
Velocity* spite_temp_218 = 0;
return spite_temp_218;
}
void Vector__Velocity_drop(Vector__Velocity* self) {
Vector__Velocity_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_219 = SPITE_ALLOCATOR_Vector__Velocity(self, spite_singleton_Memory_Heap); int64_t spite_temp_220 = self->items_; if (((SpiteHeader*)(spite_temp_219))->class_id == 94) { Memory_Arena_free(((Memory_Arena*)spite_temp_219), spite_temp_220); } else if (((SpiteHeader*)(spite_temp_219))->class_id == 95) { Memory_Heap_free(((Memory_Heap*)spite_temp_219), spite_temp_220); } });
}
}
void Vector__Velocity_make_room(Vector__Velocity* self) {
if (((self->item_count_ == self->capacity_))) {
Vector__Velocity__grow(self);
}
}
void Vector__Velocity__grow(Vector__Velocity* self) {
int32_t grown_ = ({ int32_t spite_temp_221 = self->capacity_; int32_t spite_temp_222 = 2; int32_t spite_temp_223; if (__builtin_expect(__builtin_mul_overflow(spite_temp_221, spite_temp_222, &spite_temp_223), 0)) spite_overflowed("capacity * 2", "an Integer", "*", (int64_t)spite_temp_221, (int64_t)spite_temp_222, spite_site_31()); spite_temp_223; });
if (((self->capacity_ == 0))) {
grown_ = 4;
}
int64_t old_bytes_ = InlineMemory__Velocity_block_bytes(self->values_, self->capacity_);
int64_t bytes_ = InlineMemory__Velocity_block_bytes(self->values_, grown_);
self->items_ = Vector__Velocity__resized(self, old_bytes_, bytes_);
self->capacity_ = grown_;
}
int64_t Vector__Velocity__resized(Vector__Velocity* self, int64_t old_bytes_, int64_t new_bytes_) {
if ((({ Spite_Allocator spite_temp_224 = SPITE_ALLOCATOR_Vector__Velocity(self, spite_singleton_Memory_Heap); bool spite_temp_225 = (((SpiteHeader*)(spite_temp_224))->class_id == 95); spite_temp_225; }))) {
int64_t spite_temp_226 = Memory_Heap_resize(self->heap_, self->items_, new_bytes_);
return spite_temp_226;
}
int64_t moved_ = ({ Spite_Allocator spite_temp_227 = SPITE_ALLOCATOR_Vector__Velocity(self, spite_singleton_Memory_Heap); int64_t spite_temp_228 = new_bytes_; int64_t spite_temp_229 = 0; if (((SpiteHeader*)(spite_temp_227))->class_id == 94) { spite_temp_229 = Memory_Arena_allocate(((Memory_Arena*)spite_temp_227), spite_temp_228); } else if (((SpiteHeader*)(spite_temp_227))->class_id == 95) { spite_temp_229 = Memory_Heap_allocate(((Memory_Heap*)spite_temp_227), spite_temp_228); } spite_temp_229; });
if (((self->items_ != ((int64_t)(0))))) {
SpiteMemory_Address_copy_to(self->items_, moved_, old_bytes_);
({ Spite_Allocator spite_temp_230 = SPITE_ALLOCATOR_Vector__Velocity(self, spite_singleton_Memory_Heap); int64_t spite_temp_231 = self->items_; if (((SpiteHeader*)(spite_temp_230))->class_id == 94) { Memory_Arena_free(((Memory_Arena*)spite_temp_230), spite_temp_231); } else if (((SpiteHeader*)(spite_temp_230))->class_id == 95) { Memory_Heap_free(((Memory_Heap*)spite_temp_230), spite_temp_231); } });
}
int64_t spite_temp_232 = moved_;
return spite_temp_232;
}
Velocity* InlineMemory__Velocity_item_at(InlineMemory__Velocity* self, int64_t address_, int32_t index_) {
return ((Velocity*)((char*)(intptr_t)address_ + (int64_t)index_ * ((int64_t)sizeof(Velocity) - (int64_t)sizeof(SpiteHeader))));
}
void InlineMemory__Velocity_write_item(InlineMemory__Velocity* self, int64_t address_, int32_t index_, Velocity* value_) {
Velocity* spite_slot = ((Velocity*)((char*)(intptr_t)address_ + (int64_t)index_ * ((int64_t)sizeof(Velocity) - (int64_t)sizeof(SpiteHeader))));
memcpy((char*)spite_slot + sizeof(SpiteHeader), (char*)value_ + sizeof(SpiteHeader), (size_t)((int64_t)sizeof(Velocity) - (int64_t)sizeof(SpiteHeader)));
Velocity___release(value_);
}
int64_t InlineMemory__Velocity_block_bytes(InlineMemory__Velocity* self, int32_t item_count_) {
return (int64_t)item_count_ * ((int64_t)sizeof(Velocity) - (int64_t)sizeof(SpiteHeader)) + (int64_t)sizeof(SpiteHeader);
}
Console_Printable List_Console_Printable_get_at(List_Console_Printable* self, int32_t index_) {
if ((((index_ >= 0))) && (((index_ < self->item_count_)))) {
Console_Printable spite_temp_233 = TypedMemory__Console_Printable_read_value(self->values_, self->items_, index_);
return spite_temp_233;
}
Console_Printable spite_temp_234 = SPITE_TAGGED_NULL;
return spite_temp_234;
}
void List_Console_Printable_drop(List_Console_Printable* self) {
List_Console_Printable_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_235 = SPITE_ALLOCATOR_List_Console_Printable(self, spite_singleton_Memory_Heap); int64_t spite_temp_236 = self->items_; if (((SpiteHeader*)(spite_temp_235))->class_id == 94) { Memory_Arena_free(((Memory_Arena*)spite_temp_235), spite_temp_236); } else if (((SpiteHeader*)(spite_temp_235))->class_id == 95) { Memory_Heap_free(((Memory_Heap*)spite_temp_235), spite_temp_236); } });
}
}
Console_Printable TypedMemory__Console_Printable_read_value(TypedMemory__Console_Printable* self, int64_t address_, int32_t index_) {
return Console_Printable___retain(((Console_Printable*)(intptr_t)address_)[index_]);
}
void List_Symbol_drop(List_Symbol* self) {
spite_folded_List_Symbol_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_237 = SPITE_ALLOCATOR_List_Symbol(self, spite_singleton_Memory_Heap); int64_t spite_temp_238 = self->items_; if (((SpiteHeader*)(spite_temp_237))->class_id == 94) { Memory_Arena_free(((Memory_Arena*)spite_temp_237), spite_temp_238); } else if (((SpiteHeader*)(spite_temp_237))->class_id == 95) { Memory_Heap_free(((Memory_Heap*)spite_temp_237), spite_temp_238); } });
}
}
void Benchmark__Long_Benchmark(Benchmark__Long* self, Spite_Function* work_) {
int64_t started_ = Clock_elapsed_nanoseconds(self->_clock_);
self->answer_ = ({ Spite_Function* spite_temp_239 = work_; int64_t spite_temp_240 = ((int64_t (*)(void*))spite_temp_239->spite_typed_call)(spite_temp_239->spite_owner); spite_temp_240; });
int64_t finished_ = Clock_elapsed_nanoseconds(self->_clock_);
Duration* spite_temp_241 = Duration___make(({ int64_t spite_temp_242 = finished_; int64_t spite_temp_243 = started_; int64_t spite_temp_244; if (__builtin_expect(__builtin_sub_overflow(spite_temp_242, spite_temp_243, &spite_temp_244), 0)) spite_overflowed("finished - started", "a Long", "-", (int64_t)spite_temp_242, (int64_t)spite_temp_243, spite_site_33()); spite_temp_244; }), Duration_Unit_nanoseconds);
Duration___release(self->duration_);
self->duration_ = spite_temp_241;
Spite_Function___release(work_);
}
void Mover_update_each___lent_0(Mover* self, Mover_Moving moving_) {
Position spite_temp_245_scratch;
Position* spite_temp_245 = Mover_Moving___peek_position(moving_);
if (spite_temp_245 == 0) spite_temp_245 = &spite_temp_245_scratch;
(spite_temp_245)->left_ = ({ int32_t spite_temp_246 = ({ Position* spite_temp_247 = Mover_Moving___peek_position(moving_); spite_temp_247 != 0 ? (spite_temp_247)->left_ : (0); }); int32_t spite_temp_248 = ({ Velocity* spite_temp_249 = Mover_Moving___peek_velocity(moving_); spite_temp_249 != 0 ? (spite_temp_249)->across_ : (0); }); int32_t spite_temp_250; if (__builtin_expect(__builtin_add_overflow(spite_temp_246, spite_temp_248, &spite_temp_250), 0)) spite_overflowed("moving.position.left + moving.velocity.across", "an Integer", "+", (int64_t)spite_temp_246, (int64_t)spite_temp_248, spite_site_17()); spite_temp_250; });
Position spite_temp_251_scratch;
Position* spite_temp_251 = Mover_Moving___peek_position(moving_);
if (spite_temp_251 == 0) spite_temp_251 = &spite_temp_251_scratch;
(spite_temp_251)->top_ = ({ int32_t spite_temp_252 = ({ Position* spite_temp_253 = Mover_Moving___peek_position(moving_); spite_temp_253 != 0 ? (spite_temp_253)->top_ : (0); }); int32_t spite_temp_254 = ({ Velocity* spite_temp_255 = Mover_Moving___peek_velocity(moving_); spite_temp_255 != 0 ? (spite_temp_255)->down_ : (0); }); int32_t spite_temp_256; if (__builtin_expect(__builtin_add_overflow(spite_temp_252, spite_temp_254, &spite_temp_256), 0)) spite_overflowed("moving.position.top + moving.velocity.down", "an Integer", "+", (int64_t)spite_temp_252, (int64_t)spite_temp_254, spite_site_18()); spite_temp_256; });
if ((((({ Entity* spite_temp_257 = Mover_Moving___peek_entity(moving_); spite_temp_257 != 0 ? (spite_temp_257)->id_ : (0); }) % 1000) == 0))) {
self->marked_ = ({ int32_t spite_temp_258 = self->marked_; int32_t spite_temp_259 = 1; int32_t spite_temp_260; if (__builtin_expect(__builtin_add_overflow(spite_temp_258, spite_temp_259, &spite_temp_260), 0)) spite_overflowed("marked + 1", "an Integer", "+", (int64_t)spite_temp_258, (int64_t)spite_temp_259, spite_site_19()); spite_temp_260; });
}
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
void Runner__Mover_Moving_note(Runner__Mover_Moving* self, int32_t dense_) {
if (((dense_ < 0))) {
self->missing_ = true;
}
List_Integer_append(self->found_, dense_);
}
void Vector__Position_clear(Vector__Position* self) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
InlineMemory__Position_release_item(self->values_, self->items_, index_);
index_ = (index_ + 1);
}
self->item_count_ = 0;
}
void InlineMemory__Position_release_item(InlineMemory__Position* self, int64_t address_, int32_t index_) {
Position* spite_slot = ((Position*)((char*)(intptr_t)address_ + (int64_t)index_ * ((int64_t)sizeof(Position) - (int64_t)sizeof(SpiteHeader))));
}
void Vector__Velocity_clear(Vector__Velocity* self) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
InlineMemory__Velocity_release_item(self->values_, self->items_, index_);
index_ = (index_ + 1);
}
self->item_count_ = 0;
}
void InlineMemory__Velocity_release_item(InlineMemory__Velocity* self, int64_t address_, int32_t index_) {
Velocity* spite_slot = ((Velocity*)((char*)(intptr_t)address_ + (int64_t)index_ * ((int64_t)sizeof(Velocity) - (int64_t)sizeof(SpiteHeader))));
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
{(const void*)&Launcher___pool_grow, "-\t-", "Launcher___pool_grow", 0},
{(const void*)&Launcher___pool_take, "-\t-", "Launcher___pool_take", 0},
{(const void*)&Launcher___pool_give, "-\t-", "Launcher___pool_give", 0},
{(const void*)&Duration___pool_grow, "-\t-", "Duration___pool_grow", 0},
{(const void*)&Duration___pool_take, "-\t-", "Duration___pool_take", 0},
{(const void*)&Duration___pool_give, "-\t-", "Duration___pool_give", 0},
{(const void*)&Spite_Class___pool_grow, "-\t-", "Spite_Class___pool_grow", 0},
{(const void*)&Spite_Class___pool_take, "-\t-", "Spite_Class___pool_take", 0},
{(const void*)&Spite_Class___pool_give, "-\t-", "Spite_Class___pool_give", 0},
{(const void*)&Spite_Function___pool_grow, "-\t-", "Spite_Function___pool_grow", 0},
{(const void*)&Spite_Function___pool_take, "-\t-", "Spite_Function___pool_take", 0},
{(const void*)&Spite_Function___pool_give, "-\t-", "Spite_Function___pool_give", 0},
{(const void*)&Naive___pool_grow, "-\t-", "Naive___pool_grow", 0},
{(const void*)&Naive___pool_take, "-\t-", "Naive___pool_take", 0},
{(const void*)&Naive___pool_give, "-\t-", "Naive___pool_give", 0},
{(const void*)&Mover___pool_grow, "-\t-", "Mover___pool_grow", 0},
{(const void*)&Mover___pool_take, "-\t-", "Mover___pool_take", 0},
{(const void*)&Mover___pool_give, "-\t-", "Mover___pool_give", 0},
{(const void*)&SparseSet___pool_grow, "-\t-", "SparseSet___pool_grow", 0},
{(const void*)&SparseSet___pool_take, "-\t-", "SparseSet___pool_take", 0},
{(const void*)&SparseSet___pool_give, "-\t-", "SparseSet___pool_give", 0},
{(const void*)&Runner__Mover_Moving___pool_grow, "-\t-", "Runner__Mover_Moving___pool_grow", 0},
{(const void*)&Runner__Mover_Moving___pool_take, "-\t-", "Runner__Mover_Moving___pool_take", 0},
{(const void*)&Runner__Mover_Moving___pool_give, "-\t-", "Runner__Mover_Moving___pool_give", 0},
{(const void*)&Benchmark__Long___pool_grow, "-\t-", "Benchmark__Long___pool_grow", 0},
{(const void*)&Benchmark__Long___pool_take, "-\t-", "Benchmark__Long___pool_take", 0},
{(const void*)&Benchmark__Long___pool_give, "-\t-", "Benchmark__Long___pool_give", 0},
{(const void*)&spite_singleton_Memory_Heap, "-\t-", "spite_singleton_Memory_Heap", 0},
{(const void*)&Console_Printable___retain, "-\t-", "Console_Printable___retain", 0},
{(const void*)&spite_singleton_Build, "-\t-", "spite_singleton_Build", 0},
{(const void*)&spite_singleton_Console_teardown, "-\t-", "spite_singleton_Console_teardown", 0},
{(const void*)&spite_singleton_Console, "-\t-", "spite_singleton_Console", 0},
{(const void*)&spite_singleton_TypedMemory__Integer, "-\t-", "spite_singleton_TypedMemory__Integer", 0},
{(const void*)&spite_singleton_TimeText, "-\t-", "spite_singleton_TimeText", 0},
{(const void*)&spite_singleton_Clock_teardown, "-\t-", "spite_singleton_Clock_teardown", 0},
{(const void*)&spite_singleton_Clock, "-\t-", "spite_singleton_Clock", 0},
{(const void*)&spite_singleton_TypedMemory__Spite_AttributeDeclaration, "-\t-", "spite_singleton_TypedMemory__Spite_AttributeDeclaration", 0},
{(const void*)&spite_singleton_TypedMemory__Spite_Function, "-\t-", "spite_singleton_TypedMemory__Spite_Function", 0},
{(const void*)&spite_singleton_TypedMemory__Spite_Argument, "-\t-", "spite_singleton_TypedMemory__Spite_Argument", 0},
{(const void*)&spite_singleton_InlineMemory__Position, "-\t-", "spite_singleton_InlineMemory__Position", 0},
{(const void*)&spite_singleton_Column__Position_teardown, "-\t-", "spite_singleton_Column__Position_teardown", 0},
{(const void*)&spite_singleton_Column__Position, "-\t-", "spite_singleton_Column__Position", 0},
{(const void*)&spite_singleton_InlineMemory__Velocity, "-\t-", "spite_singleton_InlineMemory__Velocity", 0},
{(const void*)&spite_singleton_Column__Velocity_teardown, "-\t-", "spite_singleton_Column__Velocity_teardown", 0},
{(const void*)&spite_singleton_Column__Velocity, "-\t-", "spite_singleton_Column__Velocity", 0},
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
{(const void*)&spite_long_digits, "-\t-", "spite_long_digits", 0},
{(const void*)&spite_string_block, "-\t-", "spite_string_block", 0},
{(const void*)&spite_string_held, "-\t-", "spite_string_held", 0},
{(const void*)&SpiteString___retain, "-\t-", "SpiteString___retain", 0},
{(const void*)&SpiteString___release, "-\t-", "SpiteString___release", 0},
{(const void*)&spite_string_from_bytes, "-\t-", "spite_string_from_bytes", 0},
{(const void*)&spite_string_join, "-\t-", "spite_string_join", 0},
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
{(const void*)&Spite_Function___release, "-\t-", "Spite_Function___release", 0},
{(const void*)&Spite_Function___free, "-\t-", "Spite_Function___free", 0},
{(const void*)&Spite_Namespace___release, "-\t-", "Spite_Namespace___release", 0},
{(const void*)&Spite_Namespace___free, "-\t-", "Spite_Namespace___free", 0},
{(const void*)&Naive___init, "-\t-", "Naive___init", 0},
{(const void*)&Naive___allocate, "-\t-", "Naive___allocate", 0},
{(const void*)&Naive___retain, "-\t-", "Naive___retain", 0},
{(const void*)&Naive___release, "-\t-", "Naive___release", 0},
{(const void*)&Naive___free, "-\t-", "Naive___free", 0},
{(const void*)&Naive_spawn_and_move___dropping_call, "-\t-", "Naive_spawn_and_move___dropping_call", 0},
{(const void*)&spite_function_value_Naive_spawn_and_move, "-\t-", "spite_function_value_Naive_spawn_and_move", 0},
{(const void*)&spite_tagged_SpiteInteger, "-\t-", "spite_tagged_SpiteInteger", 0},
{(const void*)&Position___framed, "-\t-", "Position___framed", 0},
{(const void*)&Position___make_into, "-\t-", "Position___make_into", 0},
{(const void*)&Velocity___framed, "-\t-", "Velocity___framed", 0},
{(const void*)&Velocity___make_into, "-\t-", "Velocity___make_into", 0},
{(const void*)&Entity___init, "-\t-", "Entity___init", 0},
{(const void*)&Mover___init, "-\t-", "Mover___init", 0},
{(const void*)&Mover___allocate, "-\t-", "Mover___allocate", 0},
{(const void*)&Mover___make, "-\t-", "Mover___make", 0},
{(const void*)&Mover___release, "-\t-", "Mover___release", 0},
{(const void*)&Mover___free, "-\t-", "Mover___free", 0},
{(const void*)&Position___init, "-\t-", "Position___init", 0},
{(const void*)&Position___retain, "-\t-", "Position___retain", 0},
{(const void*)&Position___release, "-\t-", "Position___release", 0},
{(const void*)&Position___free, "-\t-", "Position___free", 0},
{(const void*)&SparseSet___init, "-\t-", "SparseSet___init", 0},
{(const void*)&SparseSet___allocate, "-\t-", "SparseSet___allocate", 0},
{(const void*)&SparseSet___make, "-\t-", "SparseSet___make", 0},
{(const void*)&SparseSet___release, "-\t-", "SparseSet___release", 0},
{(const void*)&SparseSet___free, "-\t-", "SparseSet___free", 0},
{(const void*)&Velocity___init, "-\t-", "Velocity___init", 0},
{(const void*)&Velocity___retain, "-\t-", "Velocity___retain", 0},
{(const void*)&Velocity___release, "-\t-", "Velocity___release", 0},
{(const void*)&Velocity___free, "-\t-", "Velocity___free", 0},
{(const void*)&List_String___release, "-\t-", "List_String___release", 0},
{(const void*)&List_String___free, "-\t-", "List_String___free", 0},
{(const void*)&List_Integer___init, "-\t-", "List_Integer___init", 0},
{(const void*)&List_Integer___allocate, "-\t-", "List_Integer___allocate", 0},
{(const void*)&List_Integer___make, "-\t-", "List_Integer___make", 0},
{(const void*)&List_Integer___release, "-\t-", "List_Integer___release", 0},
{(const void*)&List_Integer___free, "-\t-", "List_Integer___free", 0},
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
{(const void*)&Runner__Mover_Moving___init, "-\t-", "Runner__Mover_Moving___init", 0},
{(const void*)&Runner__Mover_Moving___allocate, "-\t-", "Runner__Mover_Moving___allocate", 0},
{(const void*)&Runner__Mover_Moving___make, "-\t-", "Runner__Mover_Moving___make", 0},
{(const void*)&Runner__Mover_Moving___release, "-\t-", "Runner__Mover_Moving___release", 0},
{(const void*)&Runner__Mover_Moving___free, "-\t-", "Runner__Mover_Moving___free", 0},
{(const void*)&Column__Position___init, "-\t-", "Column__Position___init", 0},
{(const void*)&Column__Position___allocate, "-\t-", "Column__Position___allocate", 0},
{(const void*)&Column__Position___make, "-\t-", "Column__Position___make", 0},
{(const void*)&Column__Position___destroy, "-\t-", "Column__Position___destroy", 0},
{(const void*)&Column__Position___discard, "-\t-", "Column__Position___discard", 0},
{(const void*)&Vector__Position___init, "-\t-", "Vector__Position___init", 0},
{(const void*)&Vector__Position___allocate, "-\t-", "Vector__Position___allocate", 0},
{(const void*)&Vector__Position___make, "-\t-", "Vector__Position___make", 0},
{(const void*)&Vector__Position___release, "-\t-", "Vector__Position___release", 0},
{(const void*)&Vector__Position___free, "-\t-", "Vector__Position___free", 0},
{(const void*)&Column__Velocity___init, "-\t-", "Column__Velocity___init", 0},
{(const void*)&Column__Velocity___allocate, "-\t-", "Column__Velocity___allocate", 0},
{(const void*)&Column__Velocity___make, "-\t-", "Column__Velocity___make", 0},
{(const void*)&Column__Velocity___destroy, "-\t-", "Column__Velocity___destroy", 0},
{(const void*)&Column__Velocity___discard, "-\t-", "Column__Velocity___discard", 0},
{(const void*)&Vector__Velocity___init, "-\t-", "Vector__Velocity___init", 0},
{(const void*)&Vector__Velocity___allocate, "-\t-", "Vector__Velocity___allocate", 0},
{(const void*)&Vector__Velocity___make, "-\t-", "Vector__Velocity___make", 0},
{(const void*)&Vector__Velocity___release, "-\t-", "Vector__Velocity___release", 0},
{(const void*)&Vector__Velocity___free, "-\t-", "Vector__Velocity___free", 0},
{(const void*)&List_Console_Printable___init, "-\t-", "List_Console_Printable___init", 0},
{(const void*)&List_Console_Printable___retain, "-\t-", "List_Console_Printable___retain", 0},
{(const void*)&List_Console_Printable___release, "-\t-", "List_Console_Printable___release", 0},
{(const void*)&List_Console_Printable___free, "-\t-", "List_Console_Printable___free", 0},
{(const void*)&List_Symbol___release, "-\t-", "List_Symbol___release", 0},
{(const void*)&List_Symbol___free, "-\t-", "List_Symbol___free", 0},
{(const void*)&Benchmark__Long___init, "-\t-", "Benchmark__Long___init", 0},
{(const void*)&Benchmark__Long___allocate, "-\t-", "Benchmark__Long___allocate", 0},
{(const void*)&Benchmark__Long___make, "-\t-", "Benchmark__Long___make", 0},
{(const void*)&Benchmark__Long___release, "-\t-", "Benchmark__Long___release", 0},
{(const void*)&Benchmark__Long___free, "-\t-", "Benchmark__Long___free", 0},
{(const void*)&spite_class_object_Long, "-\t-", "spite_class_object_Long", 0},
{(const void*)&Console_Printable___release, "-\t-", "Console_Printable___release", 0},
{(const void*)&Console_Printable___call_to_string, "-\t-", "Console_Printable___call_to_string", 0},
{(const void*)&Mover_Moving___peek_position, "-\t-", "Mover_Moving___peek_position", 0},
{(const void*)&Mover_Moving___peek_velocity, "-\t-", "Mover_Moving___peek_velocity", 0},
{(const void*)&Mover_Moving___peek_entity, "-\t-", "Mover_Moving___peek_entity", 0},
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
{(const void*)&Naive_Naive, "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/naive.spite\tNaive", "Naive", 8},
{(const void*)&Naive_spawn_and_move, "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/naive.spite\tNaive", "spawn_and_move", 15},
{(const void*)&Naive_spawn_all, "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/naive.spite\tNaive", "spawn_all", 21},
{(const void*)&Naive_push, "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/naive.spite\tNaive", "push", 35},
{(const void*)&Naive_run_ticks, "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/naive.spite\tNaive", "run_ticks", 42},
{(const void*)&Entity_Entity, "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/entity.spite\tEntity", "Entity", 3},
{(const void*)&Position_get_place, "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/position.spite\tPosition", "get_place", 4},
{(const void*)&SparseSet_place, "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/sparse_set.spite\tSparseSet", "place", 4},
{(const void*)&SparseSet_dense_index, "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/sparse_set.spite\tSparseSet", "dense_index", 12},
{(const void*)&Velocity_Velocity, "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/velocity.spite\tVelocity", "Velocity", 4},
{(const void*)&List_String_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&List_String_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&TypedMemory__String_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&List_Integer_count, "library/list.spite\tList", "count", 9},
{(const void*)&List_Integer_append, "library/list.spite\tList", "append", 17},
{(const void*)&List_Integer_get_at, "library/list.spite\tList", "get_at", 41},
{(const void*)&List_Integer_set_at, "library/list.spite\tList", "set_at", 48},
{(const void*)&spite_failed_1, "-\t-", "spite_failed_1", 0},
{(const void*)&spite_failed_2, "-\t-", "spite_failed_2", 0},
{(const void*)&List_Integer_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&List_Integer_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_Integer_make_room, "library/list.spite\tList", "make_room", 849},
{(const void*)&List_Integer__grow, "library/list.spite\tList", "_grow", 855},
{(const void*)&List_Integer__resized, "library/list.spite\tList", "_resized", 865},
{(const void*)&TypedMemory__Integer_read_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "read_value", 1},
{(const void*)&TypedMemory__Integer_write_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "write_value", 2},
{(const void*)&TypedMemory__Integer_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&TypedMemory__Integer_value_bytes, "bootstrap/source/generation/prelude.spite\tTypedMemory", "value_bytes", 4},
{(const void*)&List_Spite_AttributeDeclaration_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_Spite_Function_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_Spite_Argument_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_Spite_Class_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_Spite_Namespace_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&Runner__Mover_Moving_run___held_0, "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/runner.spite\tRunner", "run", 6},
{(const void*)&spite_failed_3, "-\t-", "spite_failed_3", 0},
{(const void*)&spite_failed_4, "-\t-", "spite_failed_4", 0},
{(const void*)&spite_failed_5, "-\t-", "spite_failed_5", 0},
{(const void*)&spite_failed_6, "-\t-", "spite_failed_6", 0},
{(const void*)&Runner__Mover_Moving_find_attributes, "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/runner.spite\tRunner", "find_attributes", 21},
{(const void*)&Runner__Mover_Moving_find_entity, "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/runner.spite\tRunner", "find_entity", 0},
{(const void*)&Runner__Mover_Moving_find_position, "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/runner.spite\tRunner", "find_position", 0},
{(const void*)&Runner__Mover_Moving_find_velocity, "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/runner.spite\tRunner", "find_velocity", 0},
{(const void*)&Column__Position_insert___held_1, "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/column.spite\tColumn", "insert", 8},
{(const void*)&Vector__Position_append, "library/vector.spite\tVector", "append", 17},
{(const void*)&Vector__Position_get_at, "library/vector.spite\tVector", "get_at", 23},
{(const void*)&Vector__Position_drop, "library/vector.spite\tVector", "drop", 291},
{(const void*)&Vector__Position_make_room, "library/vector.spite\tVector", "make_room", 311},
{(const void*)&Vector__Position__grow, "library/vector.spite\tVector", "_grow", 317},
{(const void*)&Vector__Position__resized, "library/vector.spite\tVector", "_resized", 328},
{(const void*)&Vector__Position_sum_place, "library/vector.spite\tVector", "sum_place", 0},
{(const void*)&InlineMemory__Position_item_at, "bootstrap/source/generation/prelude.spite\tInlineMemory", "item_at", 1},
{(const void*)&InlineMemory__Position_write_item, "bootstrap/source/generation/prelude.spite\tInlineMemory", "write_item", 2},
{(const void*)&InlineMemory__Position_block_bytes, "bootstrap/source/generation/prelude.spite\tInlineMemory", "block_bytes", 7},
{(const void*)&Column__Velocity_insert___held_1, "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/column.spite\tColumn", "insert", 8},
{(const void*)&Vector__Velocity_append, "library/vector.spite\tVector", "append", 17},
{(const void*)&Vector__Velocity_get_at, "library/vector.spite\tVector", "get_at", 23},
{(const void*)&Vector__Velocity_drop, "library/vector.spite\tVector", "drop", 291},
{(const void*)&Vector__Velocity_make_room, "library/vector.spite\tVector", "make_room", 311},
{(const void*)&Vector__Velocity__grow, "library/vector.spite\tVector", "_grow", 317},
{(const void*)&Vector__Velocity__resized, "library/vector.spite\tVector", "_resized", 328},
{(const void*)&InlineMemory__Velocity_item_at, "bootstrap/source/generation/prelude.spite\tInlineMemory", "item_at", 1},
{(const void*)&InlineMemory__Velocity_write_item, "bootstrap/source/generation/prelude.spite\tInlineMemory", "write_item", 2},
{(const void*)&InlineMemory__Velocity_block_bytes, "bootstrap/source/generation/prelude.spite\tInlineMemory", "block_bytes", 7},
{(const void*)&List_Console_Printable_get_at, "library/list.spite\tList", "get_at", 41},
{(const void*)&List_Console_Printable_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&TypedMemory__Console_Printable_read_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "read_value", 1},
{(const void*)&List_Symbol_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&Benchmark__Long_Benchmark, "library/benchmark.spite\tBenchmark", "Benchmark", 7},
{(const void*)&Mover_update_each___lent_0, "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/mover.spite\tMover", "update_each", 9},
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
{(const void*)&Runner__Mover_Moving_note, "benchmarks/a_walked_crash_lines_read_is_the_rows_read/naive/runner.spite\tRunner", "note", 31},
{(const void*)&Vector__Position_clear, "library/vector.spite\tVector", "clear", 64},
{(const void*)&InlineMemory__Position_release_item, "bootstrap/source/generation/prelude.spite\tInlineMemory", "release_item", 4},
{(const void*)&Vector__Velocity_clear, "library/vector.spite\tVector", "clear", 64},
{(const void*)&InlineMemory__Velocity_release_item, "bootstrap/source/generation/prelude.spite\tInlineMemory", "release_item", 4},
{(const void*)&List_Console_Printable_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&TypedMemory__Console_Printable_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
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


if (spite_class_object_Long_cache != 0 && spite_class_object_Long_cache->_namespace_ != 0) { Spite_Namespace___release(spite_class_object_Long_cache->_namespace_); spite_class_object_Long_cache->_namespace_ = 0; }
Spite_Class___release(spite_class_object_Long_cache);










return 0;
}
