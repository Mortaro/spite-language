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
typedef struct List List;
typedef struct Memory_Arena Memory_Arena;
typedef struct Memory_Heap Memory_Heap;
typedef struct Naive Naive;
typedef struct Healer Healer;
typedef struct Health Health;
typedef struct Mover Mover;
typedef struct Position Position;
typedef struct Regeneration Regeneration;
typedef struct Velocity Velocity;
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
typedef void* Healer_Mending;
typedef void* Mover_Moving;
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
typedef struct { bool has_value; int32_t value; } Nullable_Integer;
typedef struct Vector__Position Vector__Position;
typedef struct InlineMemory__Position InlineMemory__Position;
typedef struct Vector__Velocity Vector__Velocity;
typedef struct InlineMemory__Velocity InlineMemory__Velocity;
typedef struct Vector__Health Vector__Health;
typedef struct InlineMemory__Health InlineMemory__Health;
typedef struct Vector__Regeneration Vector__Regeneration;
typedef struct InlineMemory__Regeneration InlineMemory__Regeneration;
typedef void* Spite_Allocator;
struct Launcher {
SpiteHeader header;
Build* build_;
};
typedef struct List_Console_Printable List_Console_Printable;
typedef struct TypedMemory__Console_Printable TypedMemory__Console_Printable;
typedef struct SpiteBox_SpiteLong { SpiteHeader header; int64_t value; } SpiteBox_SpiteLong;
typedef struct { bool has_value; bool value; } Nullable_Boolean;
#define SPITE_FRAMED_COUNT 1073741824
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
typedef struct { bool has_value; float value; } Nullable_Float;
struct Console {
SpiteHeader header;
Memory_Heap* heap_;
DynamicLibrary* library_;
int64_t input_;
};
static SpiteString spite_lit_2 = SPITE_STATIC_STRING("\n", 1);
static SpiteString spite_lit_3 = SPITE_STATIC_STRING("\n", 1);
static SpiteString spite_lit_4 = SPITE_STATIC_STRING(" ", 1);
#define spite_site_2() "library/console.spite:56 in Console._write_values"

struct DynamicLibrary {
SpiteHeader header;
SpiteString file_name_;
int64_t handle_;
};
typedef struct { bool has_value; int64_t value; } Nullable_Long;
#define SpiteFloat_to_long(self) ((int64_t)(self))
#define SpiteFloat_to_double(self) ((double)(self))
#define SpiteInteger_to_long(self) ((int64_t)(self))
#define SpiteInteger_to_unsigned_long(self) ((uint64_t)(self))
#define SpiteInteger_to_unsigned_integer(self) ((uint32_t)(self))
#define SpiteInteger_to_byte(self) ((uint8_t)(self))
#define SpiteInteger_to_float(self) ((float)(self))
static SpiteString spite_lit_5 = SPITE_STATIC_STRING("0", 1);
#define spite_site_3() "library/long.spite:15 in Long.to_string"
#define spite_site_4() "library/long.spite:17 in Long.to_string"
#define spite_site_5() "library/long.spite:18 in Long.to_string"
#define spite_site_6() "library/long.spite:22 in Long.to_string"
#define spite_site_7() "library/long.spite:26 in Long.to_string"
#define SpiteLong_to_unsigned_integer(self) ((uint32_t)(self))
#define SpiteLong_to_unsigned_long(self) ((uint64_t)(self))
typedef struct { bool has_value; uint8_t value; } Nullable_Byte;
typedef struct { bool has_value; int16_t value; } Nullable_Short;
typedef struct { bool has_value; double value; } Nullable_Double;
#define SpiteShort_to_integer(self) ((int32_t)(self))
#define SpiteShort_to_long(self) ((int64_t)(self))
typedef struct { bool has_value; int64_t value; } Nullable_Memory_Address;
typedef struct { bool has_value; int8_t value; } Nullable_Tiny;
typedef struct { bool has_value; uint16_t value; } Nullable_UnsignedShort;
typedef struct { bool has_value; uint32_t value; } Nullable_UnsignedInteger;
typedef struct { bool has_value; uint64_t value; } Nullable_UnsignedLong;
#define SpiteString_code_at(self, index) spite_string_code_at(&(self), (index))
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
#define SpiteMemory_Address_read_short(self, offset) ({ int16_t spite_read; memcpy(&spite_read, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_short(self, offset, value) memcpy(((char*)(intptr_t)(self) + (offset)), &(int16_t){ (value) }, sizeof(int16_t))
#define SpiteMemory_Address_read_unsigned_short(self, offset) ({ uint16_t spite_read; memcpy(&spite_read, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_unsigned_short(self, offset, value) memcpy(((char*)(intptr_t)(self) + (offset)), &(uint16_t){ (value) }, sizeof(uint16_t))
#define SpiteMemory_Address_read_integer(self, offset) ({ int32_t spite_read; memcpy(&spite_read, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_integer(self, offset, value) memcpy(((char*)(intptr_t)(self) + (offset)), &(int32_t){ (value) }, sizeof(int32_t))
#define SpiteMemory_Address_read_unsigned_integer(self, offset) ({ uint32_t spite_read; memcpy(&spite_read, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_unsigned_integer(self, offset, value) memcpy(((char*)(intptr_t)(self) + (offset)), &(uint32_t){ (value) }, sizeof(uint32_t))
#define SpiteMemory_Address_read_long(self, offset) ({ int64_t spite_read; memcpy(&spite_read, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_long(self, offset, value) memcpy(((char*)(intptr_t)(self) + (offset)), &(int64_t){ (value) }, sizeof(int64_t))
#define SpiteMemory_Address_read_float(self, offset) ({ float spite_read; memcpy(&spite_read, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_float(self, offset, value) memcpy(((char*)(intptr_t)(self) + (offset)), &(float){ (value) }, sizeof(float))
#define SpiteMemory_Address_read_double(self, offset) ({ double spite_read; memcpy(&spite_read, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_read)); spite_read; })
#define SpiteMemory_Address_write_double(self, offset, value) memcpy(((char*)(intptr_t)(self) + (offset)), &(double){ (value) }, sizeof(double))
#define SpiteMemory_Address_to_long(self) ((int64_t)(self))
struct Memory_Arena {
SpiteHeader header;
int64_t _block_;
int64_t _block_bytes_;
int64_t _end_;
int64_t _used_;
Memory_Heap* heap_;
};
#define spite_site_8() "library/memory/arena.spite:12 in Memory.Arena.allocate"
#define spite_site_9() "library/memory/arena.spite:13 in Memory.Arena.allocate"
#define spite_site_10() "library/memory/arena.spite:17 in Memory.Arena.allocate"
#define spite_site_11() "library/memory/arena.spite:25 in Memory.Arena.start_block"
#define spite_site_12() "library/memory/arena.spite:26 in Memory.Arena.start_block"
struct Memory_Heap {
SpiteHeader header;
};
struct Naive {
SpiteHeader header;
Console* console_;
Clock* clock_;
Mover* mover_;
Healer* healer_;
Vector__Position* positions_;
Vector__Velocity* velocities_;
Vector__Health* healths_;
Vector__Regeneration* regenerations_;
};
#define spite_site_13() "benchmarks/a_row_of_borrowed_items_lives_in_the_frame/naive/naive.spite:16 in Naive.Naive"
static SpiteBox_SpiteString spite_lit_6_box = { { 0, -1 }, SPITE_STATIC_STRING("places", 6) };
static SpiteBox_SpiteString spite_lit_7_box = { { 0, -1 }, SPITE_STATIC_STRING("health", 6) };
typedef struct SpiteBox_SpiteInteger { SpiteHeader header; int32_t value; } SpiteBox_SpiteInteger;
static SpiteString spite_lit_8 = SPITE_STATIC_STRING("microseconds ", 13);
#define spite_site_14() "benchmarks/a_row_of_borrowed_items_lives_in_the_frame/naive/naive.spite:26 in Naive.spawn_all"
#define spite_site_15() "spite.crash\t02129a23"
#define spite_site_16() "spite.crash\t789ab036"
#define spite_site_17() "spite.crash\t6f311787"
#define spite_site_18() "benchmarks/a_row_of_borrowed_items_lives_in_the_frame/naive/naive.spite:48 in Naive.tick_once"
typedef struct Object_position_Position_velocity_Velocity Object_position_Position_velocity_Velocity;
struct Object_position_Position_velocity_Velocity {
SpiteHeader header;
Position* position_;
Velocity* velocity_;
};
typedef struct Object_health_Health_regeneration_Regeneration Object_health_Health_regeneration_Regeneration;
struct Object_health_Health_regeneration_Regeneration {
SpiteHeader header;
Health* health_;
Regeneration* regeneration_;
};
struct Healer {
SpiteHeader header;
};
#define spite_site_19() "benchmarks/a_row_of_borrowed_items_lives_in_the_frame/naive/healer.spite:7 in Healer.update_each"
struct Health {
SpiteHeader header;
int32_t amount_;
};
struct Mover {
SpiteHeader header;
};
#define spite_site_20() "benchmarks/a_row_of_borrowed_items_lives_in_the_frame/naive/mover.spite:7 in Mover.update_each"
#define spite_site_21() "benchmarks/a_row_of_borrowed_items_lives_in_the_frame/naive/mover.spite:8 in Mover.update_each"
struct Position {
SpiteHeader header;
int32_t left_;
int32_t top_;
};
#define spite_site_22() "benchmarks/a_row_of_borrowed_items_lives_in_the_frame/naive/position.spite:6 in Position.get_place"
struct Regeneration {
SpiteHeader header;
int32_t per_tick_;
};
struct Velocity {
SpiteHeader header;
int32_t across_;
int32_t down_;
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
struct Vector__Health {
SpiteHeader header;
Memory_Heap* heap_;
InlineMemory__Health* values_;
int64_t items_;
int32_t item_count_;
int32_t capacity_;
};
struct InlineMemory__Health {
SpiteHeader header;
};
struct Vector__Regeneration {
SpiteHeader header;
Memory_Heap* heap_;
InlineMemory__Regeneration* values_;
int64_t items_;
int32_t item_count_;
int32_t capacity_;
};
struct InlineMemory__Regeneration {
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
#define SpiteByte_to_unsigned_integer(self) ((uint32_t)(self))
#define SpiteDouble_to_float(self) ((float)(self))
#define SpiteInteger_to_short(self) ((int16_t)(self))
#define SpiteLong_to_double(self) ((double)(self))
#define spite_site_23() "library/vector.spite:20 in Vector.append"
#define spite_site_24() "library/vector.spite:318 in Vector._grow"
#define spite_site_25() "library/vector.spite:168 in Vector.sum_place"
#define spite_site_26() "library/vector.spite:168 in Vector.sum_amount"
static SpiteString spite_symbol_1 = { (int64_t)0x797469746e656469ULL, (int64_t)0x0700000000000000ULL };
Memory_Heap* spite_singleton_Memory_Heap(void);
Console_Printable Console_Printable___retain(Console_Printable self);
void Console_Printable___release(Console_Printable self);
Build* spite_singleton_Build(void);
Console* spite_singleton_Console(void);
DynamicLibrary* spite_foreign_library_1(void);
DynamicLibrary* spite_foreign_library_2(void);
Clock* spite_singleton_Clock(void);
InlineMemory__Position* spite_singleton_InlineMemory__Position(void);
InlineMemory__Velocity* spite_singleton_InlineMemory__Velocity(void);
InlineMemory__Health* spite_singleton_InlineMemory__Health(void);
InlineMemory__Regeneration* spite_singleton_InlineMemory__Regeneration(void);
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
SpiteString SpiteMemory_Address_text(int64_t self, int64_t length_);
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
void Naive_spawn_all(Naive* self, int32_t count_);
void Naive_run_ticks(Naive* self, int32_t count_);
void Naive_tick_once(Naive* self);
int64_t Vector__Position_sum_place(Vector__Position* self);
int32_t Vector__Health_sum_amount(Vector__Health* self);
static inline SpiteTagged spite_tagged_SpiteInteger(int32_t value);
static SPITE_CRASH_REPORT void spite_failed_1(int32_t entity_);
static SPITE_CRASH_REPORT void spite_failed_2(int32_t entity_);
static SPITE_CRASH_REPORT void spite_failed_3(int32_t entity_);
void Mover_update_each___lent_0(Mover* self, Mover_Moving moving_);
void Healer_update_each___lent_0(Healer* self, Healer_Mending mending_);
void Healer___init(Healer* self);
Healer* Healer___allocate(void);
Healer* Healer___make(void);
static inline void Healer___release(Healer* self);
void Healer___free(Healer* self);
void Healer_update_each___lent_0(Healer* self, Healer_Mending mending_);
Health* Healer_Mending___peek_health(Healer_Mending self);
Regeneration* Healer_Mending___peek_regeneration(Healer_Mending self);
void Health___init(Health* self);
Health* Health___allocate(void);
Health* Health___make(void);
static inline Health* Health___retain(Health* self);
static inline void Health___release(Health* self);
void Health___free(Health* self);
void Mover___init(Mover* self);
Mover* Mover___allocate(void);
Mover* Mover___make(void);
static inline void Mover___release(Mover* self);
void Mover___free(Mover* self);
void Mover_update_each___lent_0(Mover* self, Mover_Moving moving_);
Position* Mover_Moving___peek_position(Mover_Moving self);
Velocity* Mover_Moving___peek_velocity(Mover_Moving self);
void Position___init(Position* self);
Position* Position___allocate(void);
Position* Position___make(void);
static inline Position* Position___retain(Position* self);
static inline void Position___release(Position* self);
void Position___free(Position* self);
int64_t Position_get_place(Position* self);
void Regeneration___init(Regeneration* self);
Regeneration* Regeneration___allocate(void);
Regeneration* Regeneration___make(int32_t new_per_tick_);
static inline Regeneration* Regeneration___retain(Regeneration* self);
static inline void Regeneration___release(Regeneration* self);
void Regeneration___free(Regeneration* self);
void Regeneration_Regeneration(Regeneration* self, int32_t new_per_tick_);
void Velocity___init(Velocity* self);
Velocity* Velocity___allocate(void);
Velocity* Velocity___make(int32_t new_across_, int32_t new_down_);
static inline Velocity* Velocity___retain(Velocity* self);
static inline void Velocity___release(Velocity* self);
void Velocity___free(Velocity* self);
void Velocity_Velocity(Velocity* self, int32_t new_across_, int32_t new_down_);
void Vector__Position___init(Vector__Position* self);
Vector__Position* Vector__Position___allocate(void);
Vector__Position* Vector__Position___make(void);
static inline void Vector__Position___release(Vector__Position* self);
void Vector__Position___free(Vector__Position* self);
void Vector__Position_drop(Vector__Position* self);
int32_t Vector__Position_count(Vector__Position* self);
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
void Vector__Health___init(Vector__Health* self);
Vector__Health* Vector__Health___allocate(void);
Vector__Health* Vector__Health___make(void);
static inline void Vector__Health___release(Vector__Health* self);
void Vector__Health___free(Vector__Health* self);
void Vector__Health_drop(Vector__Health* self);
void Vector__Health_append(Vector__Health* self, Health* value_);
Health* Vector__Health_get_at(Vector__Health* self, int32_t index_);
void Vector__Health_clear(Vector__Health* self);
void Vector__Health_drop(Vector__Health* self);
void Vector__Health_make_room(Vector__Health* self);
void Vector__Health__grow(Vector__Health* self);
int64_t Vector__Health__resized(Vector__Health* self, int64_t old_bytes_, int64_t new_bytes_);
int32_t Vector__Health_sum_amount(Vector__Health* self);
void InlineMemory__Health___release(InlineMemory__Health* self);
Health* InlineMemory__Health_item_at(InlineMemory__Health* self, int64_t address_, int32_t index_);
void InlineMemory__Health_write_item(InlineMemory__Health* self, int64_t address_, int32_t index_, Health* value_);
void InlineMemory__Health_release_item(InlineMemory__Health* self, int64_t address_, int32_t index_);
int64_t InlineMemory__Health_block_bytes(InlineMemory__Health* self, int32_t item_count_);
void Vector__Regeneration___init(Vector__Regeneration* self);
Vector__Regeneration* Vector__Regeneration___allocate(void);
Vector__Regeneration* Vector__Regeneration___make(void);
static inline void Vector__Regeneration___release(Vector__Regeneration* self);
void Vector__Regeneration___free(Vector__Regeneration* self);
void Vector__Regeneration_drop(Vector__Regeneration* self);
void Vector__Regeneration_append(Vector__Regeneration* self, Regeneration* value_);
Regeneration* Vector__Regeneration_get_at(Vector__Regeneration* self, int32_t index_);
void Vector__Regeneration_clear(Vector__Regeneration* self);
void Vector__Regeneration_drop(Vector__Regeneration* self);
void Vector__Regeneration_make_room(Vector__Regeneration* self);
void Vector__Regeneration__grow(Vector__Regeneration* self);
int64_t Vector__Regeneration__resized(Vector__Regeneration* self, int64_t old_bytes_, int64_t new_bytes_);
void InlineMemory__Regeneration___release(InlineMemory__Regeneration* self);
Regeneration* InlineMemory__Regeneration_item_at(InlineMemory__Regeneration* self, int64_t address_, int32_t index_);
void InlineMemory__Regeneration_write_item(InlineMemory__Regeneration* self, int64_t address_, int32_t index_, Regeneration* value_);
void InlineMemory__Regeneration_release_item(InlineMemory__Regeneration* self, int64_t address_, int32_t index_);
int64_t InlineMemory__Regeneration_block_bytes(InlineMemory__Regeneration* self, int32_t item_count_);
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
#define SPITE_ALLOCATOR_Vector__Health(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_Vector__Regeneration(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Console_Printable(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Console_Debuggable(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Directory_Entry(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_File(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Symbol(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Spite_Access(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Directory(object, heap) ((void)(object), ((Spite_Allocator)heap()))
static __typeof__(&Mover___init) spite_folded_Mover___init = ((__typeof__(&Mover___init))&Healer___init);
static __typeof__(&Mover___free) spite_folded_Mover___free = ((__typeof__(&Mover___free))&Healer___free);
static __typeof__(&InlineMemory__Position___release) spite_folded_InlineMemory__Position___release = ((__typeof__(&InlineMemory__Position___release))&Memory_Heap___release);
static __typeof__(&InlineMemory__Velocity___release) spite_folded_InlineMemory__Velocity___release = ((__typeof__(&InlineMemory__Velocity___release))&Memory_Heap___release);
static __typeof__(&InlineMemory__Health___release) spite_folded_InlineMemory__Health___release = ((__typeof__(&InlineMemory__Health___release))&Memory_Heap___release);
static __typeof__(&InlineMemory__Regeneration___release) spite_folded_InlineMemory__Regeneration___release = ((__typeof__(&InlineMemory__Regeneration___release))&Memory_Heap___release);
static __typeof__(&TypedMemory__Console_Printable___release) spite_folded_TypedMemory__Console_Printable___release = ((__typeof__(&TypedMemory__Console_Printable___release))&Memory_Heap___release);
static __typeof__(&List_Console_Printable_count) spite_folded_List_Console_Printable_count = ((__typeof__(&List_Console_Printable_count))&Vector__Position_count);
Memory_Heap* spite_singleton_Memory_Heap(void) {
static Memory_Heap spite_object = { { 1, 93 } };
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
InlineMemory__Position* spite_singleton_InlineMemory__Position(void) {
static InlineMemory__Position spite_object = { { 1, 154 } };
return &spite_object;
}
InlineMemory__Velocity* spite_singleton_InlineMemory__Velocity(void) {
static InlineMemory__Velocity spite_object = { { 1, 156 } };
return &spite_object;
}
InlineMemory__Health* spite_singleton_InlineMemory__Health(void) {
static InlineMemory__Health spite_object = { { 1, 158 } };
return &spite_object;
}
InlineMemory__Regeneration* spite_singleton_InlineMemory__Regeneration(void) {
static InlineMemory__Regeneration spite_object = { { 1, 160 } };
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
static TypedMemory__Console_Printable spite_object = { { 1, 162 } };
return &spite_object;
}
static List_Console_Printable* List_Console_Printable___framed(List_Console_Printable* self, int64_t items, int32_t count) {
List_Console_Printable___init(self);
self->header.ref_count = 2;
self->header.class_id = 161;
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
tagged.tag = 163;
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
void Memory_Heap___release(Memory_Heap* self) { (void)self; }
void Naive___init(Naive* self) {
self->console_ = spite_singleton_Console();
self->clock_ = spite_singleton_Clock();
self->mover_ = Mover___make();
self->healer_ = Healer___make();
self->positions_ = Vector__Position___make();
self->velocities_ = Vector__Velocity___make();
self->healths_ = Vector__Health___make();
self->regenerations_ = Vector__Regeneration___make();
}
Naive* Naive___allocate(void) {
Naive* self = (Naive*)SPITE_MALLOC(sizeof(Naive));
self->header.ref_count = 1;
self->header.class_id = 108;
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
Mover___release(self->mover_);
Healer___release(self->healer_);
Vector__Position___release(self->positions_);
Vector__Velocity___release(self->velocities_);
Vector__Health___release(self->healths_);
Vector__Regeneration___release(self->regenerations_);
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
tagged.tag = 178;
tagged.plain = 1;
tagged.value.bits = 0;
memcpy(&tagged.value, &value, sizeof(value));
return tagged;
}
void Healer___init(Healer* self) {
}
Healer* Healer___allocate(void) {
Healer* self = (Healer*)SPITE_MALLOC(sizeof(Healer));
self->header.ref_count = 1;
self->header.class_id = 109;
Healer___init(self);
#ifdef SPITE_TRACKS_Healer
spite_track_Healer(self);
#endif
return self;
}
Healer* Healer___make(void) {
Healer* self = Healer___allocate();
return self;
}
static inline void Healer___release(Healer* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Healer___free(self);
}
void Healer___free(Healer* self) {
#ifdef SPITE_TRACKS_Healer
spite_untrack_Healer(self);
#endif
#ifdef SPITE_WEAK_Healer
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}

void Health___init(Health* self) {
self->amount_ = 0;
}
Health* Health___allocate(void) {
Health* self = (Health*)SPITE_MALLOC(sizeof(Health));
self->header.ref_count = 1;
self->header.class_id = 110;
Health___init(self);
#ifdef SPITE_TRACKS_Health
spite_track_Health(self);
#endif
return self;
}
Health* Health___make(void) {
Health* self = Health___allocate();
return self;
}
static inline Health* Health___retain(Health* self) {
if (self != 0) SPITE_COUNT_UP(self->header.ref_count);
return self;
}
static inline void Health___release(Health* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Health___free(self);
}
void Health___free(Health* self) {
#ifdef SPITE_TRACKS_Health
spite_untrack_Health(self);
#endif
#ifdef SPITE_WEAK_Health
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
Mover* Mover___allocate(void) {
Mover* self = (Mover*)SPITE_MALLOC(sizeof(Mover));
self->header.ref_count = 1;
self->header.class_id = 111;
spite_folded_Mover___init(self);
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
spite_folded_Mover___free(self);
}

void Position___init(Position* self) {
self->left_ = 0;
self->top_ = 0;
}
Position* Position___allocate(void) {
Position* self = (Position*)SPITE_MALLOC(sizeof(Position));
self->header.ref_count = 1;
self->header.class_id = 112;
Position___init(self);
#ifdef SPITE_TRACKS_Position
spite_track_Position(self);
#endif
return self;
}
Position* Position___make(void) {
Position* self = Position___allocate();
return self;
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
void Regeneration___init(Regeneration* self) {
self->per_tick_ = 0;
}
Regeneration* Regeneration___allocate(void) {
Regeneration* self = (Regeneration*)SPITE_MALLOC(sizeof(Regeneration));
self->header.ref_count = 1;
self->header.class_id = 113;
Regeneration___init(self);
#ifdef SPITE_TRACKS_Regeneration
spite_track_Regeneration(self);
#endif
return self;
}
Regeneration* Regeneration___make(int32_t new_per_tick_) {
Regeneration* self = Regeneration___allocate();
Regeneration_Regeneration(self, new_per_tick_);
return self;
}
static inline Regeneration* Regeneration___retain(Regeneration* self) {
if (self != 0) SPITE_COUNT_UP(self->header.ref_count);
return self;
}
static inline void Regeneration___release(Regeneration* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Regeneration___free(self);
}
void Regeneration___free(Regeneration* self) {
#ifdef SPITE_TRACKS_Regeneration
spite_untrack_Regeneration(self);
#endif
#ifdef SPITE_WEAK_Regeneration
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void Velocity___init(Velocity* self) {
self->across_ = 0;
self->down_ = 0;
}
Velocity* Velocity___allocate(void) {
Velocity* self = (Velocity*)SPITE_MALLOC(sizeof(Velocity));
self->header.ref_count = 1;
self->header.class_id = 114;
Velocity___init(self);
#ifdef SPITE_TRACKS_Velocity
spite_track_Velocity(self);
#endif
return self;
}
Velocity* Velocity___make(int32_t new_across_, int32_t new_down_) {
Velocity* self = Velocity___allocate();
Velocity_Velocity(self, new_across_, new_down_);
return self;
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
self->header.class_id = 153;
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
Memory_Heap___release(self->heap_);
spite_folded_InlineMemory__Position___release(self->values_);
#ifdef SPITE_TRACKS_Vector__Position
spite_untrack_Vector__Position(self);
#endif
#ifdef SPITE_WEAK_Vector__Position
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
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
self->header.class_id = 155;
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
Memory_Heap___release(self->heap_);
spite_folded_InlineMemory__Velocity___release(self->values_);
#ifdef SPITE_TRACKS_Vector__Velocity
spite_untrack_Vector__Velocity(self);
#endif
#ifdef SPITE_WEAK_Vector__Velocity
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void Vector__Health___init(Vector__Health* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->values_ = spite_singleton_InlineMemory__Health();
self->items_ = ((int64_t)(0));
self->item_count_ = 0;
self->capacity_ = 0;
}
Vector__Health* Vector__Health___allocate(void) {
Vector__Health* self = (Vector__Health*)SPITE_MALLOC(sizeof(Vector__Health));
self->header.ref_count = 1;
self->header.class_id = 157;
Vector__Health___init(self);
#ifdef SPITE_TRACKS_Vector__Health
spite_track_Vector__Health(self);
#endif
return self;
}
Vector__Health* Vector__Health___make(void) {
Vector__Health* self = Vector__Health___allocate();
return self;
}
static inline void Vector__Health___release(Vector__Health* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Vector__Health___free(self);
}
void Vector__Health___free(Vector__Health* self) {
Vector__Health_drop(self);
Memory_Heap___release(self->heap_);
spite_folded_InlineMemory__Health___release(self->values_);
#ifdef SPITE_TRACKS_Vector__Health
spite_untrack_Vector__Health(self);
#endif
#ifdef SPITE_WEAK_Vector__Health
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void Vector__Regeneration___init(Vector__Regeneration* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->values_ = spite_singleton_InlineMemory__Regeneration();
self->items_ = ((int64_t)(0));
self->item_count_ = 0;
self->capacity_ = 0;
}
Vector__Regeneration* Vector__Regeneration___allocate(void) {
Vector__Regeneration* self = (Vector__Regeneration*)SPITE_MALLOC(sizeof(Vector__Regeneration));
self->header.ref_count = 1;
self->header.class_id = 159;
Vector__Regeneration___init(self);
#ifdef SPITE_TRACKS_Vector__Regeneration
spite_track_Vector__Regeneration(self);
#endif
return self;
}
Vector__Regeneration* Vector__Regeneration___make(void) {
Vector__Regeneration* self = Vector__Regeneration___allocate();
return self;
}
static inline void Vector__Regeneration___release(Vector__Regeneration* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Vector__Regeneration___free(self);
}
void Vector__Regeneration___free(Vector__Regeneration* self) {
Vector__Regeneration_drop(self);
Memory_Heap___release(self->heap_);
spite_folded_InlineMemory__Regeneration___release(self->values_);
#ifdef SPITE_TRACKS_Vector__Regeneration
spite_untrack_Vector__Regeneration(self);
#endif
#ifdef SPITE_WEAK_Vector__Regeneration
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
void Console_Printable___release(Console_Printable self) {
if (self.plain != 0 || self.value.object == 0) return;
if (((self).tag == 0) && ((self).plain == 0)) { spite_string_box_release(self.value.object); return; }
}
SpiteString Console_Printable___call_to_string(Console_Printable self) {
if (((self).tag == 0) && ((self).plain == 0)) return SpiteString_to_string((((SpiteBox_SpiteString*)(self).value.object)->value));
if ((self).tag == 163) return SpiteLong_to_string(SPITE_TAGGED_VALUE(self, int64_t));
if ((self).tag == 178) return SpiteInteger_to_string(SPITE_TAGGED_VALUE(self, int32_t));
fputs("spite.crash\tPrintable.to_string was called on a value of a class it was not compiled for\n", stderr);
abort();
}
Health* Healer_Mending___peek_health(Healer_Mending self) {
if (((SpiteHeader*)(self))->class_id == 180) return ((Object_health_Health_regeneration_Regeneration*)self)->health_;
return 0;
}
Regeneration* Healer_Mending___peek_regeneration(Healer_Mending self) {
if (((SpiteHeader*)(self))->class_id == 180) return ((Object_health_Health_regeneration_Regeneration*)self)->regeneration_;
return 0;
}
Position* Mover_Moving___peek_position(Mover_Moving self) {
if (((SpiteHeader*)(self))->class_id == 179) return ((Object_position_Position_velocity_Velocity*)self)->position_;
return 0;
}
Velocity* Mover_Moving___peek_velocity(Mover_Moving self) {
if (((SpiteHeader*)(self))->class_id == 179) return ((Object_position_Position_velocity_Velocity*)self)->velocity_;
return 0;
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
void Clock_Clock(Clock* self) {
int64_t spite_temp_1[1];
int64_t spite_temp_2 = SpiteInteger_to_long(8);
int64_t frequency_ = spite_temp_2 <= 8 ? (int64_t)(intptr_t)spite_temp_1 : Memory_Heap_allocate(self->heap_, spite_temp_2);
(void)(({ spite_last_foreign_call = "QueryPerformanceFrequency\tlibrary=kernel32.dll\tfrom=library/windows/clock.spite:6"; int32_t spite_temp_3 = ((int32_t (*)(int64_t))spite_foreign_1_0)((int64_t)(frequency_));  int32_t spite_foreign_result = spite_temp_3;  (void)spite_foreign_result; spite_temp_3; }));
self->_ticks_per_second_ = SpiteMemory_Address_read_long(frequency_, SpiteInteger_to_long(0));
if (frequency_ != (int64_t)(intptr_t)spite_temp_1) Memory_Heap_free(self->heap_, frequency_);
}
int64_t Clock_elapsed_nanoseconds(Clock* self) {
int64_t spite_temp_4[1];
int64_t spite_temp_5 = SpiteInteger_to_long(8);
int64_t counter_ = spite_temp_5 <= 8 ? (int64_t)(intptr_t)spite_temp_4 : Memory_Heap_allocate(self->heap_, spite_temp_5);
(void)(({ spite_last_foreign_call = "QueryPerformanceCounter\tlibrary=kernel32.dll\tfrom=library/windows/clock.spite:13"; int32_t spite_temp_6 = ((int32_t (*)(int64_t))spite_foreign_1_1)((int64_t)(counter_));  int32_t spite_foreign_result = spite_temp_6;  (void)spite_foreign_result; spite_temp_6; }));
int64_t ticks_ = SpiteMemory_Address_read_long(counter_, SpiteInteger_to_long(0));
if (counter_ != (int64_t)(intptr_t)spite_temp_4) Memory_Heap_free(self->heap_, counter_);
int64_t spite_temp_7 = ({ int64_t spite_temp_8 = ({ int64_t spite_temp_9 = ({ int64_t spite_temp_10 = ticks_; int64_t spite_temp_11 = self->_ticks_per_second_; if (spite_temp_11 == 0) spite_divided_by_zero("ticks / _ticks_per_second", spite_site_1()); int64_t spite_temp_12 = 0; if (__builtin_expect(spite_temp_11 == -1 && __builtin_sub_overflow((int64_t)0, spite_temp_10, &spite_temp_12), 0)) spite_overflowed("ticks / _ticks_per_second", "a Long", "/", (int64_t)spite_temp_10, (int64_t)spite_temp_11, spite_site_1()); (int64_t)(spite_temp_11 == -1 ? spite_temp_12 : spite_temp_10 / spite_temp_11); }); int64_t spite_temp_13 = SpiteInteger_to_long(1000000000); int64_t spite_temp_14; if (__builtin_expect(__builtin_mul_overflow(spite_temp_9, spite_temp_13, &spite_temp_14), 0)) spite_overflowed("ticks / _ticks_per_second * 1000000000", "a Long", "*", (int64_t)spite_temp_9, (int64_t)spite_temp_13, spite_site_1()); spite_temp_14; }); int64_t spite_temp_15 = ({ int64_t spite_temp_16 = ({ int64_t spite_temp_17 = ({ int64_t spite_temp_18 = ticks_; int64_t spite_temp_19 = self->_ticks_per_second_; if (spite_temp_19 == 0) spite_divided_by_zero("ticks % _ticks_per_second", spite_site_1()); (int64_t)(spite_temp_19 == -1 ? (int64_t)0 : spite_temp_18 % spite_temp_19); }); int64_t spite_temp_20 = SpiteInteger_to_long(1000000000); int64_t spite_temp_21; if (__builtin_expect(__builtin_mul_overflow(spite_temp_17, spite_temp_20, &spite_temp_21), 0)) spite_overflowed("ticks % _ticks_per_second * 1000000000", "a Long", "*", (int64_t)spite_temp_17, (int64_t)spite_temp_20, spite_site_1()); spite_temp_21; }); int64_t spite_temp_22 = self->_ticks_per_second_; if (spite_temp_22 == 0) spite_divided_by_zero("ticks % _ticks_per_second * 1000000000 / _ticks_per_second", spite_site_1()); int64_t spite_temp_23 = 0; if (__builtin_expect(spite_temp_22 == -1 && __builtin_sub_overflow((int64_t)0, spite_temp_16, &spite_temp_23), 0)) spite_overflowed("ticks % _ticks_per_second * 1000000000 / _ticks_per_second", "a Long", "/", (int64_t)spite_temp_16, (int64_t)spite_temp_22, spite_site_1()); (int64_t)(spite_temp_22 == -1 ? spite_temp_23 : spite_temp_16 / spite_temp_22); }); int64_t spite_temp_24; if (__builtin_expect(__builtin_add_overflow(spite_temp_8, spite_temp_15, &spite_temp_24), 0)) spite_overflowed("ticks / _ticks_per_second * 1000000000 + ticks % _ticks_per_second * 1000000000 / _ticks_per_second", "a Long", "+", (int64_t)spite_temp_8, (int64_t)spite_temp_15, spite_site_1()); spite_temp_24; });
return spite_temp_7;
}
void Console_print(Console* self, List_Console_Printable* values_) {
Console__write_values(self, List_Console_Printable___retain(values_), Console_Stream_output);
Console__write_output(self, spite_lit_2);
Console__flush(self);
List_Console_Printable___release(values_);
}
void Console_error(Console* self, List_Console_Printable* values_) {
Console__flush(self);
Console__write_values(self, List_Console_Printable___retain(values_), Console_Stream_error);
Console__write_error(self, spite_lit_3);
Console__flush(self);
List_Console_Printable___release(values_);
}
void Console__write_values(Console* self, List_Console_Printable* values_, Console_Stream stream_) {
int32_t index_ = 0;
while (((index_ < spite_folded_List_Console_Printable_count(values_)))) {
if (((index_ > 0))) {
Console__write_to(self, spite_lit_4, stream_);
}
SpiteString text_ = ({ Console_Printable spite_temp_25 = ({ Console_Printable spite_temp_26 = List_Console_Printable_get_at(values_, index_); if (__builtin_expect(!(SPITE_TAGGED_PRESENT(spite_temp_26)), 0)) spite_outside_list("values[index]", spite_site_2()); spite_temp_26; }); SpiteString spite_temp_27 = Console_Printable___call_to_string(spite_temp_25); Console_Printable___release(spite_temp_25); spite_temp_27; });
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
SpiteString spite_temp_28 = SpiteString___retain(file_);
SpiteString___release(self->file_name_);
self->file_name_ = spite_temp_28;
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
SpiteString spite_temp_29 = SpiteLong_to_string(wide_);
return spite_temp_29;
}
SpiteString SpiteLong_to_string(int64_t self) {
if (((self == SpiteInteger_to_long(0)))) {
SpiteString spite_temp_30 = spite_lit_5;
return spite_temp_30;
}
Memory_Heap* heap_ = spite_singleton_Memory_Heap();
int64_t buffer_bytes_ = SpiteInteger_to_long(24);
int64_t spite_temp_31[32];
int64_t spite_temp_32 = buffer_bytes_;
int64_t address_ = spite_temp_32 <= 256 ? (int64_t)(intptr_t)spite_temp_31 : Memory_Heap_allocate(heap_, spite_temp_32);
int64_t position_ = buffer_bytes_;
int64_t rest_ = self;
while (((rest_ != SpiteInteger_to_long(0)))) {
int64_t digit_ = (rest_ % SpiteInteger_to_long(10));
if (((digit_ < SpiteInteger_to_long(0)))) {
digit_ = ({ int64_t spite_temp_33 = digit_; int64_t spite_temp_34; if (__builtin_expect(__builtin_sub_overflow((int64_t)0, spite_temp_33, &spite_temp_34), 0)) spite_overflowed("-digit", "a Long", "-", (int64_t)0, (int64_t)spite_temp_33, spite_site_3()); spite_temp_34; });
}
position_ = ({ int64_t spite_temp_35 = position_; int64_t spite_temp_36 = SpiteInteger_to_long(1); int64_t spite_temp_37; if (__builtin_expect(__builtin_sub_overflow(spite_temp_35, spite_temp_36, &spite_temp_37), 0)) spite_overflowed("position - 1", "a Long", "-", (int64_t)spite_temp_35, (int64_t)spite_temp_36, spite_site_4()); spite_temp_37; });
SpiteMemory_Address_write_byte(address_, position_, ({ int64_t spite_temp_38 = ({ int64_t spite_temp_39 = digit_; int64_t spite_temp_40 = SpiteInteger_to_long(48); int64_t spite_temp_41; if (__builtin_expect(__builtin_add_overflow(spite_temp_39, spite_temp_40, &spite_temp_41), 0)) spite_overflowed("digit + 48", "a Long", "+", (int64_t)spite_temp_39, (int64_t)spite_temp_40, spite_site_5()); spite_temp_41; }); if (__builtin_expect(spite_temp_38 < 0 || spite_temp_38 > UINT8_MAX, 0)) spite_narrowed((int64_t)spite_temp_38, "a Long", "a Byte", spite_site_5()); (uint8_t)spite_temp_38; }));
rest_ = (rest_ / SpiteInteger_to_long(10));
}
if (((self < SpiteInteger_to_long(0)))) {
position_ = ({ int64_t spite_temp_42 = position_; int64_t spite_temp_43 = SpiteInteger_to_long(1); int64_t spite_temp_44; if (__builtin_expect(__builtin_sub_overflow(spite_temp_42, spite_temp_43, &spite_temp_44), 0)) spite_overflowed("position - 1", "a Long", "-", (int64_t)spite_temp_42, (int64_t)spite_temp_43, spite_site_6()); spite_temp_44; });
SpiteMemory_Address_write_byte(address_, position_, SpiteInteger_to_byte(45));
}
int64_t first_digit_ = (address_ + ((int64_t)(position_)));
SpiteString text_ = SpiteMemory_Address_text(first_digit_, ({ int64_t spite_temp_45 = buffer_bytes_; int64_t spite_temp_46 = position_; int64_t spite_temp_47; if (__builtin_expect(__builtin_sub_overflow(spite_temp_45, spite_temp_46, &spite_temp_47), 0)) spite_overflowed("buffer_bytes - position", "a Long", "-", (int64_t)spite_temp_45, (int64_t)spite_temp_46, spite_site_7()); spite_temp_47; }));
if (address_ != (int64_t)(intptr_t)spite_temp_31) Memory_Heap_free(heap_, address_);
SpiteString spite_temp_48 = SpiteString___retain(text_);
SpiteString___release(text_);
Memory_Heap___release(heap_);
return spite_temp_48;
}
SpiteString SpiteString_to_string(SpiteString self) {
SpiteString spite_temp_49 = SpiteString___retain(self);
return spite_temp_49;
}
SpiteString SpiteMemory_Address_text(int64_t self, int64_t length_) {
return spite_string_from_bytes((const char*)(intptr_t)self, length_);
}
int64_t Memory_Arena_allocate(Memory_Arena* self, int64_t bytes_) {
int64_t rounded_ = ({ int64_t spite_temp_50 = (({ int64_t spite_temp_51 = bytes_; int64_t spite_temp_52 = SpiteInteger_to_long(15); int64_t spite_temp_53; if (__builtin_expect(__builtin_add_overflow(spite_temp_51, spite_temp_52, &spite_temp_53), 0)) spite_overflowed("bytes + 15", "a Long", "+", (int64_t)spite_temp_51, (int64_t)spite_temp_52, spite_site_8()); spite_temp_53; }) / SpiteInteger_to_long(16)); int64_t spite_temp_54 = SpiteInteger_to_long(16); int64_t spite_temp_55; if (__builtin_expect(__builtin_mul_overflow(spite_temp_50, spite_temp_54, &spite_temp_55), 0)) spite_overflowed("(bytes + 15) / 16 * 16", "a Long", "*", (int64_t)spite_temp_50, (int64_t)spite_temp_54, spite_site_8()); spite_temp_55; });
if (((((self->_block_ == ((int64_t)(0)))) || ((({ int64_t spite_temp_56 = self->_used_; int64_t spite_temp_57 = rounded_; int64_t spite_temp_58; if (__builtin_expect(__builtin_add_overflow(spite_temp_56, spite_temp_57, &spite_temp_58), 0)) spite_overflowed("_used + rounded", "a Long", "+", (int64_t)spite_temp_56, (int64_t)spite_temp_57, spite_site_9()); spite_temp_58; }) > self->_end_))))) {
Memory_Arena_start_block(self, rounded_);
}
int64_t address_ = (self->_block_ + ((int64_t)(self->_used_)));
self->_used_ = ({ int64_t spite_temp_59 = self->_used_; int64_t spite_temp_60 = rounded_; int64_t spite_temp_61; if (__builtin_expect(__builtin_add_overflow(spite_temp_59, spite_temp_60, &spite_temp_61), 0)) spite_overflowed("_used + rounded", "a Long", "+", (int64_t)spite_temp_59, (int64_t)spite_temp_60, spite_site_10()); spite_temp_61; });
int64_t spite_temp_62 = address_;
return spite_temp_62;
}
void Memory_Arena_free(Memory_Arena* self, int64_t _address_) {
}
void Memory_Arena_start_block(Memory_Arena* self, int64_t at_least_) {
int64_t size_ = self->_block_bytes_;
if (((({ int64_t spite_temp_63 = at_least_; int64_t spite_temp_64 = SpiteInteger_to_long(16); int64_t spite_temp_65; if (__builtin_expect(__builtin_add_overflow(spite_temp_63, spite_temp_64, &spite_temp_65), 0)) spite_overflowed("at_least + 16", "a Long", "+", (int64_t)spite_temp_63, (int64_t)spite_temp_64, spite_site_11()); spite_temp_65; }) > size_))) {
size_ = ({ int64_t spite_temp_66 = at_least_; int64_t spite_temp_67 = SpiteInteger_to_long(16); int64_t spite_temp_68; if (__builtin_expect(__builtin_add_overflow(spite_temp_66, spite_temp_67, &spite_temp_68), 0)) spite_overflowed("at_least + 16", "a Long", "+", (int64_t)spite_temp_66, (int64_t)spite_temp_67, spite_site_12()); spite_temp_68; });
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
Naive_spawn_all(self, 100000);
Naive_run_ticks(self, 100);
int64_t places_ = Vector__Position_sum_place(self->positions_);
int32_t amounts_ = Vector__Health_sum_amount(self->healths_);
int64_t microseconds_ = (({ int64_t spite_temp_69 = Clock_elapsed_nanoseconds(self->clock_); int64_t spite_temp_70 = start_; int64_t spite_temp_71; if (__builtin_expect(__builtin_sub_overflow(spite_temp_69, spite_temp_70, &spite_temp_71), 0)) spite_overflowed("clock.elapsed_nanoseconds() - start", "a Long", "-", (int64_t)spite_temp_69, (int64_t)spite_temp_70, spite_site_13()); spite_temp_71; }) / SpiteInteger_to_long(1000));
List_Console_Printable spite_framed_1; Console_Printable spite_framed_1_items[4]; int32_t spite_framed_1_count = 0;
Console_print(self->console_, ({ spite_framed_1_items[0] = spite_tagged_object(0, ((void*)&spite_lit_6_box)); spite_framed_1_items[1] = spite_tagged_SpiteLong(places_); spite_framed_1_items[2] = spite_tagged_object(0, ((void*)&spite_lit_7_box)); spite_framed_1_items[3] = spite_tagged_SpiteInteger(amounts_); spite_framed_1_count = 4; List_Console_Printable___framed(&spite_framed_1, (int64_t)(intptr_t)spite_framed_1_items, 4); }));
for (int32_t spite_index = 0; spite_index < spite_framed_1_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_1_items[spite_index]); }
List_Console_Printable spite_framed_2; Console_Printable spite_framed_2_items[1]; int32_t spite_framed_2_count = 0;
Console_error(self->console_, ({ spite_framed_2_items[0] = spite_tagged_object(0, spite_box_SpiteString(({ char spite_temp_72_digits[24]; SpiteString spite_temp_72 = SPITE_STATIC_STRING(spite_temp_72_digits, spite_long_digits(spite_temp_72_digits, (int64_t)(microseconds_))); SpiteString spite_temp_73[] = {spite_lit_8, spite_temp_72}; SpiteString spite_temp_74 = spite_string_join(2, spite_temp_73); spite_temp_74; }))); spite_framed_2_count = 1; List_Console_Printable___framed(&spite_framed_2, (int64_t)(intptr_t)spite_framed_2_items, 1); }));
for (int32_t spite_index = 0; spite_index < spite_framed_2_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_2_items[spite_index]); }
}
void Naive_spawn_all(Naive* self, int32_t count_) {
int32_t entity_ = 0;
while (((entity_ < count_))) {
Position* position_ = Position___make();
Vector__Position_append(self->positions_, Position___retain(position_));
Velocity* velocity_ = Velocity___make(({ int32_t spite_temp_75 = (entity_ % 13); int32_t spite_temp_76 = 6; int32_t spite_temp_77; if (__builtin_expect(__builtin_sub_overflow(spite_temp_75, spite_temp_76, &spite_temp_77), 0)) spite_overflowed("entity % 13 - 6", "an Integer", "-", (int64_t)spite_temp_75, (int64_t)spite_temp_76, spite_site_14()); spite_temp_77; }), ({ int32_t spite_temp_78 = (entity_ % 7); int32_t spite_temp_79 = 3; int32_t spite_temp_80; if (__builtin_expect(__builtin_sub_overflow(spite_temp_78, spite_temp_79, &spite_temp_80), 0)) spite_overflowed("entity % 7 - 3", "an Integer", "-", (int64_t)spite_temp_78, (int64_t)spite_temp_79, spite_site_14()); spite_temp_80; }));
Vector__Velocity_append(self->velocities_, Velocity___retain(velocity_));
Health* health_ = Health___make();
Vector__Health_append(self->healths_, Health___retain(health_));
Regeneration* regeneration_ = Regeneration___make((entity_ % 4));
Vector__Regeneration_append(self->regenerations_, Regeneration___retain(regeneration_));
entity_ = (entity_ + 1);
Regeneration___release(regeneration_);
Health___release(health_);
Velocity___release(velocity_);
Position___release(position_);
}
}
void Naive_run_ticks(Naive* self, int32_t count_) {
int32_t tick_ = 0;
while (((tick_ < count_))) {
Naive_tick_once(self);
tick_ = (tick_ + 1);
}
}
void Naive_tick_once(Naive* self) {
int32_t entity_ = 0;
while (((entity_ < Vector__Position_count(self->positions_)))) {
if (!(((Vector__Velocity_get_at(self->velocities_, entity_)) != 0))) {
spite_failed_1(entity_);
}
if (!(((Vector__Health_get_at(self->healths_, entity_)) != 0))) {
spite_failed_2(entity_);
}
if (!(((Vector__Regeneration_get_at(self->regenerations_, entity_)) != 0))) {
spite_failed_3(entity_);
}
Object_position_Position_velocity_Velocity spite_temp_81 = { { 1, 179 } };
spite_temp_81.position_ = ({ Position* spite_temp_82 = Vector__Position_get_at(self->positions_, entity_); if (__builtin_expect(!(((spite_temp_82) != 0)), 0)) spite_outside_list("positions[entity]", spite_site_18()); spite_temp_82; });
spite_temp_81.velocity_ = Vector__Velocity_get_at(self->velocities_, entity_);
Object_position_Position_velocity_Velocity* moving_ = (&spite_temp_81);
Mover_update_each___lent_0(self->mover_, ((Mover_Moving)(moving_)));
Object_health_Health_regeneration_Regeneration spite_temp_83 = { { 1, 180 } };
spite_temp_83.health_ = Vector__Health_get_at(self->healths_, entity_);
spite_temp_83.regeneration_ = Vector__Regeneration_get_at(self->regenerations_, entity_);
Object_health_Health_regeneration_Regeneration* mending_ = (&spite_temp_83);
Healer_update_each___lent_0(self->healer_, ((Healer_Mending)(mending_)));
entity_ = (entity_ + 1);
}
}
static SPITE_CRASH_REPORT void spite_failed_1(int32_t entity_) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_15(), stderr);
{
fputs("\tvelocities[entity] is missing: key ", stderr);
{ SpiteString spite_temp_84 = SpiteInteger_to_string(entity_); fwrite(spite_string_bytes(&spite_temp_84), 1, (size_t)spite_string_length(spite_temp_84), stderr); SpiteString___release(spite_temp_84); }
}
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
static SPITE_CRASH_REPORT void spite_failed_2(int32_t entity_) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_16(), stderr);
{
fputs("\thealths[entity] is missing: key ", stderr);
{ SpiteString spite_temp_85 = SpiteInteger_to_string(entity_); fwrite(spite_string_bytes(&spite_temp_85), 1, (size_t)spite_string_length(spite_temp_85), stderr); SpiteString___release(spite_temp_85); }
}
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
static SPITE_CRASH_REPORT void spite_failed_3(int32_t entity_) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_17(), stderr);
{
fputs("\tregenerations[entity] is missing: key ", stderr);
{ SpiteString spite_temp_86 = SpiteInteger_to_string(entity_); fwrite(spite_string_bytes(&spite_temp_86), 1, (size_t)spite_string_length(spite_temp_86), stderr); SpiteString___release(spite_temp_86); }
}
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
void Healer_update_each___lent_0(Healer* self, Healer_Mending mending_) {
Health spite_temp_87_scratch;
Health* spite_temp_87 = Healer_Mending___peek_health(mending_);
if (spite_temp_87 == 0) spite_temp_87 = &spite_temp_87_scratch;
(spite_temp_87)->amount_ = ({ int32_t spite_temp_88 = ({ Health* spite_temp_89 = Healer_Mending___peek_health(mending_); spite_temp_89 != 0 ? (spite_temp_89)->amount_ : (0); }); int32_t spite_temp_90 = ({ Regeneration* spite_temp_91 = Healer_Mending___peek_regeneration(mending_); spite_temp_91 != 0 ? (spite_temp_91)->per_tick_ : (0); }); int32_t spite_temp_92; if (__builtin_expect(__builtin_add_overflow(spite_temp_88, spite_temp_90, &spite_temp_92), 0)) spite_overflowed("mending.health.amount + mending.regeneration.per_tick", "an Integer", "+", (int64_t)spite_temp_88, (int64_t)spite_temp_90, spite_site_19()); spite_temp_92; });
}
void Mover_update_each___lent_0(Mover* self, Mover_Moving moving_) {
Position spite_temp_93_scratch;
Position* spite_temp_93 = Mover_Moving___peek_position(moving_);
if (spite_temp_93 == 0) spite_temp_93 = &spite_temp_93_scratch;
(spite_temp_93)->left_ = ({ int32_t spite_temp_94 = ({ Position* spite_temp_95 = Mover_Moving___peek_position(moving_); spite_temp_95 != 0 ? (spite_temp_95)->left_ : (0); }); int32_t spite_temp_96 = ({ Velocity* spite_temp_97 = Mover_Moving___peek_velocity(moving_); spite_temp_97 != 0 ? (spite_temp_97)->across_ : (0); }); int32_t spite_temp_98; if (__builtin_expect(__builtin_add_overflow(spite_temp_94, spite_temp_96, &spite_temp_98), 0)) spite_overflowed("moving.position.left + moving.velocity.across", "an Integer", "+", (int64_t)spite_temp_94, (int64_t)spite_temp_96, spite_site_20()); spite_temp_98; });
Position spite_temp_99_scratch;
Position* spite_temp_99 = Mover_Moving___peek_position(moving_);
if (spite_temp_99 == 0) spite_temp_99 = &spite_temp_99_scratch;
(spite_temp_99)->top_ = ({ int32_t spite_temp_100 = ({ Position* spite_temp_101 = Mover_Moving___peek_position(moving_); spite_temp_101 != 0 ? (spite_temp_101)->top_ : (0); }); int32_t spite_temp_102 = ({ Velocity* spite_temp_103 = Mover_Moving___peek_velocity(moving_); spite_temp_103 != 0 ? (spite_temp_103)->down_ : (0); }); int32_t spite_temp_104; if (__builtin_expect(__builtin_add_overflow(spite_temp_100, spite_temp_102, &spite_temp_104), 0)) spite_overflowed("moving.position.top + moving.velocity.down", "an Integer", "+", (int64_t)spite_temp_100, (int64_t)spite_temp_102, spite_site_21()); spite_temp_104; });
}
int64_t Position_get_place(Position* self) {
int64_t wide_ = SpiteInteger_to_long(self->left_);
int64_t spite_temp_105 = ({ int64_t spite_temp_106 = wide_; int64_t spite_temp_107 = SpiteInteger_to_long(self->top_); int64_t spite_temp_108; if (__builtin_expect(__builtin_add_overflow(spite_temp_106, spite_temp_107, &spite_temp_108), 0)) spite_overflowed("wide + top", "a Long", "+", (int64_t)spite_temp_106, (int64_t)spite_temp_107, spite_site_22()); spite_temp_108; });
return spite_temp_105;
}
void Regeneration_Regeneration(Regeneration* self, int32_t new_per_tick_) {
self->per_tick_ = new_per_tick_;
}
void Velocity_Velocity(Velocity* self, int32_t new_across_, int32_t new_down_) {
self->across_ = new_across_;
self->down_ = new_down_;
}
int32_t Vector__Position_count(Vector__Position* self) {
int32_t spite_temp_109 = self->item_count_;
return spite_temp_109;
}
void Vector__Position_append(Vector__Position* self, Position* value_) {
Vector__Position_make_room(self);
InlineMemory__Position_write_item(self->values_, self->items_, self->item_count_, Position___retain(value_));
self->item_count_ = ({ int32_t spite_temp_110 = self->item_count_; int32_t spite_temp_111 = 1; int32_t spite_temp_112; if (__builtin_expect(__builtin_add_overflow(spite_temp_110, spite_temp_111, &spite_temp_112), 0)) spite_overflowed("item_count + 1", "an Integer", "+", (int64_t)spite_temp_110, (int64_t)spite_temp_111, spite_site_23()); spite_temp_112; });
Position___release(value_);
}
Position* Vector__Position_get_at(Vector__Position* self, int32_t index_) {
if ((((index_ >= 0))) && (((index_ < self->item_count_)))) {
Position* spite_temp_113 = InlineMemory__Position_item_at(self->values_, self->items_, index_);
return spite_temp_113;
}
Position* spite_temp_114 = 0;
return spite_temp_114;
}
void Vector__Position_drop(Vector__Position* self) {
Vector__Position_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_115 = SPITE_ALLOCATOR_Vector__Position(self, spite_singleton_Memory_Heap); int64_t spite_temp_116 = self->items_; if (((SpiteHeader*)(spite_temp_115))->class_id == 92) { Memory_Arena_free(((Memory_Arena*)spite_temp_115), spite_temp_116); } else if (((SpiteHeader*)(spite_temp_115))->class_id == 93) { Memory_Heap_free(((Memory_Heap*)spite_temp_115), spite_temp_116); } });
}
}
void Vector__Position_make_room(Vector__Position* self) {
if (((self->item_count_ == self->capacity_))) {
Vector__Position__grow(self);
}
}
void Vector__Position__grow(Vector__Position* self) {
int32_t grown_ = ({ int32_t spite_temp_117 = self->capacity_; int32_t spite_temp_118 = 2; int32_t spite_temp_119; if (__builtin_expect(__builtin_mul_overflow(spite_temp_117, spite_temp_118, &spite_temp_119), 0)) spite_overflowed("capacity * 2", "an Integer", "*", (int64_t)spite_temp_117, (int64_t)spite_temp_118, spite_site_24()); spite_temp_119; });
if (((self->capacity_ == 0))) {
grown_ = 4;
}
int64_t old_bytes_ = InlineMemory__Position_block_bytes(self->values_, self->capacity_);
int64_t bytes_ = InlineMemory__Position_block_bytes(self->values_, grown_);
self->items_ = Vector__Position__resized(self, old_bytes_, bytes_);
self->capacity_ = grown_;
}
int64_t Vector__Position__resized(Vector__Position* self, int64_t old_bytes_, int64_t new_bytes_) {
if ((({ Spite_Allocator spite_temp_120 = SPITE_ALLOCATOR_Vector__Position(self, spite_singleton_Memory_Heap); bool spite_temp_121 = (((SpiteHeader*)(spite_temp_120))->class_id == 93); spite_temp_121; }))) {
int64_t spite_temp_122 = Memory_Heap_resize(self->heap_, self->items_, new_bytes_);
return spite_temp_122;
}
int64_t moved_ = ({ Spite_Allocator spite_temp_123 = SPITE_ALLOCATOR_Vector__Position(self, spite_singleton_Memory_Heap); int64_t spite_temp_124 = new_bytes_; int64_t spite_temp_125 = 0; if (((SpiteHeader*)(spite_temp_123))->class_id == 92) { spite_temp_125 = Memory_Arena_allocate(((Memory_Arena*)spite_temp_123), spite_temp_124); } else if (((SpiteHeader*)(spite_temp_123))->class_id == 93) { spite_temp_125 = Memory_Heap_allocate(((Memory_Heap*)spite_temp_123), spite_temp_124); } spite_temp_125; });
if (((self->items_ != ((int64_t)(0))))) {
SpiteMemory_Address_copy_to(self->items_, moved_, old_bytes_);
({ Spite_Allocator spite_temp_126 = SPITE_ALLOCATOR_Vector__Position(self, spite_singleton_Memory_Heap); int64_t spite_temp_127 = self->items_; if (((SpiteHeader*)(spite_temp_126))->class_id == 92) { Memory_Arena_free(((Memory_Arena*)spite_temp_126), spite_temp_127); } else if (((SpiteHeader*)(spite_temp_126))->class_id == 93) { Memory_Heap_free(((Memory_Heap*)spite_temp_126), spite_temp_127); } });
}
int64_t spite_temp_128 = moved_;
return spite_temp_128;
}
int64_t Vector__Position_sum_place(Vector__Position* self) {
int64_t total_ = SpiteInteger_to_long(0);
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
Position* item_ = InlineMemory__Position_item_at(self->values_, self->items_, index_);
total_ = ({ int64_t spite_temp_129 = total_; int64_t spite_temp_130 = Position_get_place(item_); int64_t spite_temp_131; if (__builtin_expect(__builtin_add_overflow(spite_temp_129, spite_temp_130, &spite_temp_131), 0)) spite_overflowed("total + item.attributes[member]", "a Long", "+", (int64_t)spite_temp_129, (int64_t)spite_temp_130, spite_site_25()); spite_temp_131; });
index_ = (index_ + 1);
}
int64_t spite_temp_132 = total_;
return spite_temp_132;
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
void Vector__Velocity_append(Vector__Velocity* self, Velocity* value_) {
Vector__Velocity_make_room(self);
InlineMemory__Velocity_write_item(self->values_, self->items_, self->item_count_, Velocity___retain(value_));
self->item_count_ = ({ int32_t spite_temp_133 = self->item_count_; int32_t spite_temp_134 = 1; int32_t spite_temp_135; if (__builtin_expect(__builtin_add_overflow(spite_temp_133, spite_temp_134, &spite_temp_135), 0)) spite_overflowed("item_count + 1", "an Integer", "+", (int64_t)spite_temp_133, (int64_t)spite_temp_134, spite_site_23()); spite_temp_135; });
Velocity___release(value_);
}
Velocity* Vector__Velocity_get_at(Vector__Velocity* self, int32_t index_) {
if ((((index_ >= 0))) && (((index_ < self->item_count_)))) {
Velocity* spite_temp_136 = InlineMemory__Velocity_item_at(self->values_, self->items_, index_);
return spite_temp_136;
}
Velocity* spite_temp_137 = 0;
return spite_temp_137;
}
void Vector__Velocity_drop(Vector__Velocity* self) {
Vector__Velocity_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_138 = SPITE_ALLOCATOR_Vector__Velocity(self, spite_singleton_Memory_Heap); int64_t spite_temp_139 = self->items_; if (((SpiteHeader*)(spite_temp_138))->class_id == 92) { Memory_Arena_free(((Memory_Arena*)spite_temp_138), spite_temp_139); } else if (((SpiteHeader*)(spite_temp_138))->class_id == 93) { Memory_Heap_free(((Memory_Heap*)spite_temp_138), spite_temp_139); } });
}
}
void Vector__Velocity_make_room(Vector__Velocity* self) {
if (((self->item_count_ == self->capacity_))) {
Vector__Velocity__grow(self);
}
}
void Vector__Velocity__grow(Vector__Velocity* self) {
int32_t grown_ = ({ int32_t spite_temp_140 = self->capacity_; int32_t spite_temp_141 = 2; int32_t spite_temp_142; if (__builtin_expect(__builtin_mul_overflow(spite_temp_140, spite_temp_141, &spite_temp_142), 0)) spite_overflowed("capacity * 2", "an Integer", "*", (int64_t)spite_temp_140, (int64_t)spite_temp_141, spite_site_24()); spite_temp_142; });
if (((self->capacity_ == 0))) {
grown_ = 4;
}
int64_t old_bytes_ = InlineMemory__Velocity_block_bytes(self->values_, self->capacity_);
int64_t bytes_ = InlineMemory__Velocity_block_bytes(self->values_, grown_);
self->items_ = Vector__Velocity__resized(self, old_bytes_, bytes_);
self->capacity_ = grown_;
}
int64_t Vector__Velocity__resized(Vector__Velocity* self, int64_t old_bytes_, int64_t new_bytes_) {
if ((({ Spite_Allocator spite_temp_143 = SPITE_ALLOCATOR_Vector__Velocity(self, spite_singleton_Memory_Heap); bool spite_temp_144 = (((SpiteHeader*)(spite_temp_143))->class_id == 93); spite_temp_144; }))) {
int64_t spite_temp_145 = Memory_Heap_resize(self->heap_, self->items_, new_bytes_);
return spite_temp_145;
}
int64_t moved_ = ({ Spite_Allocator spite_temp_146 = SPITE_ALLOCATOR_Vector__Velocity(self, spite_singleton_Memory_Heap); int64_t spite_temp_147 = new_bytes_; int64_t spite_temp_148 = 0; if (((SpiteHeader*)(spite_temp_146))->class_id == 92) { spite_temp_148 = Memory_Arena_allocate(((Memory_Arena*)spite_temp_146), spite_temp_147); } else if (((SpiteHeader*)(spite_temp_146))->class_id == 93) { spite_temp_148 = Memory_Heap_allocate(((Memory_Heap*)spite_temp_146), spite_temp_147); } spite_temp_148; });
if (((self->items_ != ((int64_t)(0))))) {
SpiteMemory_Address_copy_to(self->items_, moved_, old_bytes_);
({ Spite_Allocator spite_temp_149 = SPITE_ALLOCATOR_Vector__Velocity(self, spite_singleton_Memory_Heap); int64_t spite_temp_150 = self->items_; if (((SpiteHeader*)(spite_temp_149))->class_id == 92) { Memory_Arena_free(((Memory_Arena*)spite_temp_149), spite_temp_150); } else if (((SpiteHeader*)(spite_temp_149))->class_id == 93) { Memory_Heap_free(((Memory_Heap*)spite_temp_149), spite_temp_150); } });
}
int64_t spite_temp_151 = moved_;
return spite_temp_151;
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
void Vector__Health_append(Vector__Health* self, Health* value_) {
Vector__Health_make_room(self);
InlineMemory__Health_write_item(self->values_, self->items_, self->item_count_, Health___retain(value_));
self->item_count_ = ({ int32_t spite_temp_152 = self->item_count_; int32_t spite_temp_153 = 1; int32_t spite_temp_154; if (__builtin_expect(__builtin_add_overflow(spite_temp_152, spite_temp_153, &spite_temp_154), 0)) spite_overflowed("item_count + 1", "an Integer", "+", (int64_t)spite_temp_152, (int64_t)spite_temp_153, spite_site_23()); spite_temp_154; });
Health___release(value_);
}
Health* Vector__Health_get_at(Vector__Health* self, int32_t index_) {
if ((((index_ >= 0))) && (((index_ < self->item_count_)))) {
Health* spite_temp_155 = InlineMemory__Health_item_at(self->values_, self->items_, index_);
return spite_temp_155;
}
Health* spite_temp_156 = 0;
return spite_temp_156;
}
void Vector__Health_drop(Vector__Health* self) {
Vector__Health_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_157 = SPITE_ALLOCATOR_Vector__Health(self, spite_singleton_Memory_Heap); int64_t spite_temp_158 = self->items_; if (((SpiteHeader*)(spite_temp_157))->class_id == 92) { Memory_Arena_free(((Memory_Arena*)spite_temp_157), spite_temp_158); } else if (((SpiteHeader*)(spite_temp_157))->class_id == 93) { Memory_Heap_free(((Memory_Heap*)spite_temp_157), spite_temp_158); } });
}
}
void Vector__Health_make_room(Vector__Health* self) {
if (((self->item_count_ == self->capacity_))) {
Vector__Health__grow(self);
}
}
void Vector__Health__grow(Vector__Health* self) {
int32_t grown_ = ({ int32_t spite_temp_159 = self->capacity_; int32_t spite_temp_160 = 2; int32_t spite_temp_161; if (__builtin_expect(__builtin_mul_overflow(spite_temp_159, spite_temp_160, &spite_temp_161), 0)) spite_overflowed("capacity * 2", "an Integer", "*", (int64_t)spite_temp_159, (int64_t)spite_temp_160, spite_site_24()); spite_temp_161; });
if (((self->capacity_ == 0))) {
grown_ = 4;
}
int64_t old_bytes_ = InlineMemory__Health_block_bytes(self->values_, self->capacity_);
int64_t bytes_ = InlineMemory__Health_block_bytes(self->values_, grown_);
self->items_ = Vector__Health__resized(self, old_bytes_, bytes_);
self->capacity_ = grown_;
}
int64_t Vector__Health__resized(Vector__Health* self, int64_t old_bytes_, int64_t new_bytes_) {
if ((({ Spite_Allocator spite_temp_162 = SPITE_ALLOCATOR_Vector__Health(self, spite_singleton_Memory_Heap); bool spite_temp_163 = (((SpiteHeader*)(spite_temp_162))->class_id == 93); spite_temp_163; }))) {
int64_t spite_temp_164 = Memory_Heap_resize(self->heap_, self->items_, new_bytes_);
return spite_temp_164;
}
int64_t moved_ = ({ Spite_Allocator spite_temp_165 = SPITE_ALLOCATOR_Vector__Health(self, spite_singleton_Memory_Heap); int64_t spite_temp_166 = new_bytes_; int64_t spite_temp_167 = 0; if (((SpiteHeader*)(spite_temp_165))->class_id == 92) { spite_temp_167 = Memory_Arena_allocate(((Memory_Arena*)spite_temp_165), spite_temp_166); } else if (((SpiteHeader*)(spite_temp_165))->class_id == 93) { spite_temp_167 = Memory_Heap_allocate(((Memory_Heap*)spite_temp_165), spite_temp_166); } spite_temp_167; });
if (((self->items_ != ((int64_t)(0))))) {
SpiteMemory_Address_copy_to(self->items_, moved_, old_bytes_);
({ Spite_Allocator spite_temp_168 = SPITE_ALLOCATOR_Vector__Health(self, spite_singleton_Memory_Heap); int64_t spite_temp_169 = self->items_; if (((SpiteHeader*)(spite_temp_168))->class_id == 92) { Memory_Arena_free(((Memory_Arena*)spite_temp_168), spite_temp_169); } else if (((SpiteHeader*)(spite_temp_168))->class_id == 93) { Memory_Heap_free(((Memory_Heap*)spite_temp_168), spite_temp_169); } });
}
int64_t spite_temp_170 = moved_;
return spite_temp_170;
}
int32_t Vector__Health_sum_amount(Vector__Health* self) {
int32_t total_ = 0;
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
Health* item_ = InlineMemory__Health_item_at(self->values_, self->items_, index_);
total_ = ({ int32_t spite_temp_171 = total_; int32_t spite_temp_172 = (item_)->amount_; int32_t spite_temp_173; if (__builtin_expect(__builtin_add_overflow(spite_temp_171, spite_temp_172, &spite_temp_173), 0)) spite_overflowed("total + item.attributes[member]", "an Integer", "+", (int64_t)spite_temp_171, (int64_t)spite_temp_172, spite_site_26()); spite_temp_173; });
index_ = (index_ + 1);
}
int32_t spite_temp_174 = total_;
return spite_temp_174;
}
Health* InlineMemory__Health_item_at(InlineMemory__Health* self, int64_t address_, int32_t index_) {
return ((Health*)((char*)(intptr_t)address_ + (int64_t)index_ * ((int64_t)sizeof(Health) - (int64_t)sizeof(SpiteHeader))));
}
void InlineMemory__Health_write_item(InlineMemory__Health* self, int64_t address_, int32_t index_, Health* value_) {
Health* spite_slot = ((Health*)((char*)(intptr_t)address_ + (int64_t)index_ * ((int64_t)sizeof(Health) - (int64_t)sizeof(SpiteHeader))));
memcpy((char*)spite_slot + sizeof(SpiteHeader), (char*)value_ + sizeof(SpiteHeader), (size_t)((int64_t)sizeof(Health) - (int64_t)sizeof(SpiteHeader)));
Health___release(value_);
}
int64_t InlineMemory__Health_block_bytes(InlineMemory__Health* self, int32_t item_count_) {
return (int64_t)item_count_ * ((int64_t)sizeof(Health) - (int64_t)sizeof(SpiteHeader)) + (int64_t)sizeof(SpiteHeader);
}
void Vector__Regeneration_append(Vector__Regeneration* self, Regeneration* value_) {
Vector__Regeneration_make_room(self);
InlineMemory__Regeneration_write_item(self->values_, self->items_, self->item_count_, Regeneration___retain(value_));
self->item_count_ = ({ int32_t spite_temp_175 = self->item_count_; int32_t spite_temp_176 = 1; int32_t spite_temp_177; if (__builtin_expect(__builtin_add_overflow(spite_temp_175, spite_temp_176, &spite_temp_177), 0)) spite_overflowed("item_count + 1", "an Integer", "+", (int64_t)spite_temp_175, (int64_t)spite_temp_176, spite_site_23()); spite_temp_177; });
Regeneration___release(value_);
}
Regeneration* Vector__Regeneration_get_at(Vector__Regeneration* self, int32_t index_) {
if ((((index_ >= 0))) && (((index_ < self->item_count_)))) {
Regeneration* spite_temp_178 = InlineMemory__Regeneration_item_at(self->values_, self->items_, index_);
return spite_temp_178;
}
Regeneration* spite_temp_179 = 0;
return spite_temp_179;
}
void Vector__Regeneration_drop(Vector__Regeneration* self) {
Vector__Regeneration_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_180 = SPITE_ALLOCATOR_Vector__Regeneration(self, spite_singleton_Memory_Heap); int64_t spite_temp_181 = self->items_; if (((SpiteHeader*)(spite_temp_180))->class_id == 92) { Memory_Arena_free(((Memory_Arena*)spite_temp_180), spite_temp_181); } else if (((SpiteHeader*)(spite_temp_180))->class_id == 93) { Memory_Heap_free(((Memory_Heap*)spite_temp_180), spite_temp_181); } });
}
}
void Vector__Regeneration_make_room(Vector__Regeneration* self) {
if (((self->item_count_ == self->capacity_))) {
Vector__Regeneration__grow(self);
}
}
void Vector__Regeneration__grow(Vector__Regeneration* self) {
int32_t grown_ = ({ int32_t spite_temp_182 = self->capacity_; int32_t spite_temp_183 = 2; int32_t spite_temp_184; if (__builtin_expect(__builtin_mul_overflow(spite_temp_182, spite_temp_183, &spite_temp_184), 0)) spite_overflowed("capacity * 2", "an Integer", "*", (int64_t)spite_temp_182, (int64_t)spite_temp_183, spite_site_24()); spite_temp_184; });
if (((self->capacity_ == 0))) {
grown_ = 4;
}
int64_t old_bytes_ = InlineMemory__Regeneration_block_bytes(self->values_, self->capacity_);
int64_t bytes_ = InlineMemory__Regeneration_block_bytes(self->values_, grown_);
self->items_ = Vector__Regeneration__resized(self, old_bytes_, bytes_);
self->capacity_ = grown_;
}
int64_t Vector__Regeneration__resized(Vector__Regeneration* self, int64_t old_bytes_, int64_t new_bytes_) {
if ((({ Spite_Allocator spite_temp_185 = SPITE_ALLOCATOR_Vector__Regeneration(self, spite_singleton_Memory_Heap); bool spite_temp_186 = (((SpiteHeader*)(spite_temp_185))->class_id == 93); spite_temp_186; }))) {
int64_t spite_temp_187 = Memory_Heap_resize(self->heap_, self->items_, new_bytes_);
return spite_temp_187;
}
int64_t moved_ = ({ Spite_Allocator spite_temp_188 = SPITE_ALLOCATOR_Vector__Regeneration(self, spite_singleton_Memory_Heap); int64_t spite_temp_189 = new_bytes_; int64_t spite_temp_190 = 0; if (((SpiteHeader*)(spite_temp_188))->class_id == 92) { spite_temp_190 = Memory_Arena_allocate(((Memory_Arena*)spite_temp_188), spite_temp_189); } else if (((SpiteHeader*)(spite_temp_188))->class_id == 93) { spite_temp_190 = Memory_Heap_allocate(((Memory_Heap*)spite_temp_188), spite_temp_189); } spite_temp_190; });
if (((self->items_ != ((int64_t)(0))))) {
SpiteMemory_Address_copy_to(self->items_, moved_, old_bytes_);
({ Spite_Allocator spite_temp_191 = SPITE_ALLOCATOR_Vector__Regeneration(self, spite_singleton_Memory_Heap); int64_t spite_temp_192 = self->items_; if (((SpiteHeader*)(spite_temp_191))->class_id == 92) { Memory_Arena_free(((Memory_Arena*)spite_temp_191), spite_temp_192); } else if (((SpiteHeader*)(spite_temp_191))->class_id == 93) { Memory_Heap_free(((Memory_Heap*)spite_temp_191), spite_temp_192); } });
}
int64_t spite_temp_193 = moved_;
return spite_temp_193;
}
Regeneration* InlineMemory__Regeneration_item_at(InlineMemory__Regeneration* self, int64_t address_, int32_t index_) {
return ((Regeneration*)((char*)(intptr_t)address_ + (int64_t)index_ * ((int64_t)sizeof(Regeneration) - (int64_t)sizeof(SpiteHeader))));
}
void InlineMemory__Regeneration_write_item(InlineMemory__Regeneration* self, int64_t address_, int32_t index_, Regeneration* value_) {
Regeneration* spite_slot = ((Regeneration*)((char*)(intptr_t)address_ + (int64_t)index_ * ((int64_t)sizeof(Regeneration) - (int64_t)sizeof(SpiteHeader))));
memcpy((char*)spite_slot + sizeof(SpiteHeader), (char*)value_ + sizeof(SpiteHeader), (size_t)((int64_t)sizeof(Regeneration) - (int64_t)sizeof(SpiteHeader)));
Regeneration___release(value_);
}
int64_t InlineMemory__Regeneration_block_bytes(InlineMemory__Regeneration* self, int32_t item_count_) {
return (int64_t)item_count_ * ((int64_t)sizeof(Regeneration) - (int64_t)sizeof(SpiteHeader)) + (int64_t)sizeof(SpiteHeader);
}
Console_Printable List_Console_Printable_get_at(List_Console_Printable* self, int32_t index_) {
if ((((index_ >= 0))) && (((index_ < self->item_count_)))) {
Console_Printable spite_temp_194 = TypedMemory__Console_Printable_read_value(self->values_, self->items_, index_);
return spite_temp_194;
}
Console_Printable spite_temp_195 = SPITE_TAGGED_NULL;
return spite_temp_195;
}
void List_Console_Printable_drop(List_Console_Printable* self) {
List_Console_Printable_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_196 = SPITE_ALLOCATOR_List_Console_Printable(self, spite_singleton_Memory_Heap); int64_t spite_temp_197 = self->items_; if (((SpiteHeader*)(spite_temp_196))->class_id == 92) { Memory_Arena_free(((Memory_Arena*)spite_temp_196), spite_temp_197); } else if (((SpiteHeader*)(spite_temp_196))->class_id == 93) { Memory_Heap_free(((Memory_Heap*)spite_temp_196), spite_temp_197); } });
}
}
Console_Printable TypedMemory__Console_Printable_read_value(TypedMemory__Console_Printable* self, int64_t address_, int32_t index_) {
return Console_Printable___retain(((Console_Printable*)(intptr_t)address_)[index_]);
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
void Vector__Health_clear(Vector__Health* self) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
InlineMemory__Health_release_item(self->values_, self->items_, index_);
index_ = (index_ + 1);
}
self->item_count_ = 0;
}
void InlineMemory__Health_release_item(InlineMemory__Health* self, int64_t address_, int32_t index_) {
Health* spite_slot = ((Health*)((char*)(intptr_t)address_ + (int64_t)index_ * ((int64_t)sizeof(Health) - (int64_t)sizeof(SpiteHeader))));
}
void Vector__Regeneration_clear(Vector__Regeneration* self) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
InlineMemory__Regeneration_release_item(self->values_, self->items_, index_);
index_ = (index_ + 1);
}
self->item_count_ = 0;
}
void InlineMemory__Regeneration_release_item(InlineMemory__Regeneration* self, int64_t address_, int32_t index_) {
Regeneration* spite_slot = ((Regeneration*)((char*)(intptr_t)address_ + (int64_t)index_ * ((int64_t)sizeof(Regeneration) - (int64_t)sizeof(SpiteHeader))));
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
{(const void*)&spite_crash_begin, "-\t-", "spite_crash_begin", 0},
{(const void*)&spite_report_assert_trace, "-\t-", "spite_report_assert_trace", 0},
{(const void*)&spite_string_length, "-\t-", "spite_string_length", 0},
{(const void*)&spite_string_bytes, "-\t-", "spite_string_bytes", 0},
{(const void*)&spite_string_code_at, "-\t-", "spite_string_code_at", 0},
{(const void*)&spite_singleton_Memory_Heap, "-\t-", "spite_singleton_Memory_Heap", 0},
{(const void*)&Console_Printable___retain, "-\t-", "Console_Printable___retain", 0},
{(const void*)&spite_singleton_Build, "-\t-", "spite_singleton_Build", 0},
{(const void*)&spite_singleton_Console_teardown, "-\t-", "spite_singleton_Console_teardown", 0},
{(const void*)&spite_singleton_Console, "-\t-", "spite_singleton_Console", 0},
{(const void*)&spite_singleton_Clock_teardown, "-\t-", "spite_singleton_Clock_teardown", 0},
{(const void*)&spite_singleton_Clock, "-\t-", "spite_singleton_Clock", 0},
{(const void*)&spite_singleton_InlineMemory__Position, "-\t-", "spite_singleton_InlineMemory__Position", 0},
{(const void*)&spite_singleton_InlineMemory__Velocity, "-\t-", "spite_singleton_InlineMemory__Velocity", 0},
{(const void*)&spite_singleton_InlineMemory__Health, "-\t-", "spite_singleton_InlineMemory__Health", 0},
{(const void*)&spite_singleton_InlineMemory__Regeneration, "-\t-", "spite_singleton_InlineMemory__Regeneration", 0},
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
{(const void*)&Naive___init, "-\t-", "Naive___init", 0},
{(const void*)&Naive___allocate, "-\t-", "Naive___allocate", 0},
{(const void*)&Naive___release, "-\t-", "Naive___release", 0},
{(const void*)&Naive___free, "-\t-", "Naive___free", 0},
{(const void*)&spite_tagged_SpiteInteger, "-\t-", "spite_tagged_SpiteInteger", 0},
{(const void*)&Healer___init, "-\t-", "Healer___init", 0},
{(const void*)&Healer___allocate, "-\t-", "Healer___allocate", 0},
{(const void*)&Healer___make, "-\t-", "Healer___make", 0},
{(const void*)&Healer___release, "-\t-", "Healer___release", 0},
{(const void*)&Healer___free, "-\t-", "Healer___free", 0},
{(const void*)&Health___init, "-\t-", "Health___init", 0},
{(const void*)&Health___allocate, "-\t-", "Health___allocate", 0},
{(const void*)&Health___make, "-\t-", "Health___make", 0},
{(const void*)&Health___retain, "-\t-", "Health___retain", 0},
{(const void*)&Health___release, "-\t-", "Health___release", 0},
{(const void*)&Health___free, "-\t-", "Health___free", 0},
{(const void*)&Mover___allocate, "-\t-", "Mover___allocate", 0},
{(const void*)&Mover___make, "-\t-", "Mover___make", 0},
{(const void*)&Mover___release, "-\t-", "Mover___release", 0},
{(const void*)&Position___init, "-\t-", "Position___init", 0},
{(const void*)&Position___allocate, "-\t-", "Position___allocate", 0},
{(const void*)&Position___make, "-\t-", "Position___make", 0},
{(const void*)&Position___retain, "-\t-", "Position___retain", 0},
{(const void*)&Position___release, "-\t-", "Position___release", 0},
{(const void*)&Position___free, "-\t-", "Position___free", 0},
{(const void*)&Regeneration___init, "-\t-", "Regeneration___init", 0},
{(const void*)&Regeneration___allocate, "-\t-", "Regeneration___allocate", 0},
{(const void*)&Regeneration___make, "-\t-", "Regeneration___make", 0},
{(const void*)&Regeneration___retain, "-\t-", "Regeneration___retain", 0},
{(const void*)&Regeneration___release, "-\t-", "Regeneration___release", 0},
{(const void*)&Regeneration___free, "-\t-", "Regeneration___free", 0},
{(const void*)&Velocity___init, "-\t-", "Velocity___init", 0},
{(const void*)&Velocity___allocate, "-\t-", "Velocity___allocate", 0},
{(const void*)&Velocity___make, "-\t-", "Velocity___make", 0},
{(const void*)&Velocity___retain, "-\t-", "Velocity___retain", 0},
{(const void*)&Velocity___release, "-\t-", "Velocity___release", 0},
{(const void*)&Velocity___free, "-\t-", "Velocity___free", 0},
{(const void*)&Vector__Position___init, "-\t-", "Vector__Position___init", 0},
{(const void*)&Vector__Position___allocate, "-\t-", "Vector__Position___allocate", 0},
{(const void*)&Vector__Position___make, "-\t-", "Vector__Position___make", 0},
{(const void*)&Vector__Position___release, "-\t-", "Vector__Position___release", 0},
{(const void*)&Vector__Position___free, "-\t-", "Vector__Position___free", 0},
{(const void*)&Vector__Velocity___init, "-\t-", "Vector__Velocity___init", 0},
{(const void*)&Vector__Velocity___allocate, "-\t-", "Vector__Velocity___allocate", 0},
{(const void*)&Vector__Velocity___make, "-\t-", "Vector__Velocity___make", 0},
{(const void*)&Vector__Velocity___release, "-\t-", "Vector__Velocity___release", 0},
{(const void*)&Vector__Velocity___free, "-\t-", "Vector__Velocity___free", 0},
{(const void*)&Vector__Health___init, "-\t-", "Vector__Health___init", 0},
{(const void*)&Vector__Health___allocate, "-\t-", "Vector__Health___allocate", 0},
{(const void*)&Vector__Health___make, "-\t-", "Vector__Health___make", 0},
{(const void*)&Vector__Health___release, "-\t-", "Vector__Health___release", 0},
{(const void*)&Vector__Health___free, "-\t-", "Vector__Health___free", 0},
{(const void*)&Vector__Regeneration___init, "-\t-", "Vector__Regeneration___init", 0},
{(const void*)&Vector__Regeneration___allocate, "-\t-", "Vector__Regeneration___allocate", 0},
{(const void*)&Vector__Regeneration___make, "-\t-", "Vector__Regeneration___make", 0},
{(const void*)&Vector__Regeneration___release, "-\t-", "Vector__Regeneration___release", 0},
{(const void*)&Vector__Regeneration___free, "-\t-", "Vector__Regeneration___free", 0},
{(const void*)&List_Console_Printable___init, "-\t-", "List_Console_Printable___init", 0},
{(const void*)&List_Console_Printable___retain, "-\t-", "List_Console_Printable___retain", 0},
{(const void*)&List_Console_Printable___release, "-\t-", "List_Console_Printable___release", 0},
{(const void*)&List_Console_Printable___free, "-\t-", "List_Console_Printable___free", 0},
{(const void*)&Console_Printable___release, "-\t-", "Console_Printable___release", 0},
{(const void*)&Console_Printable___call_to_string, "-\t-", "Console_Printable___call_to_string", 0},
{(const void*)&Healer_Mending___peek_health, "-\t-", "Healer_Mending___peek_health", 0},
{(const void*)&Healer_Mending___peek_regeneration, "-\t-", "Healer_Mending___peek_regeneration", 0},
{(const void*)&Mover_Moving___peek_position, "-\t-", "Mover_Moving___peek_position", 0},
{(const void*)&Mover_Moving___peek_velocity, "-\t-", "Mover_Moving___peek_velocity", 0},
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
{(const void*)&SpiteInteger_to_string, "library/integer.spite\tInteger", "to_string", 3},
{(const void*)&SpiteLong_to_string, "library/long.spite\tLong", "to_string", 3},
{(const void*)&SpiteString_to_string, "library/string.spite\tString", "to_string", 184},
{(const void*)&SpiteMemory_Address_text, "library/memory/address.spite\tMemory.Address", "text", 3},
{(const void*)&Memory_Arena_allocate, "library/memory/arena.spite\tMemory.Arena", "allocate", 11},
{(const void*)&Memory_Arena_free, "library/memory/arena.spite\tMemory.Arena", "free", 21},
{(const void*)&Memory_Arena_start_block, "library/memory/arena.spite\tMemory.Arena", "start_block", 23},
{(const void*)&Memory_Heap_allocate, "bootstrap/source/generation/prelude.spite\tMemory.Heap", "allocate", 1},
{(const void*)&Memory_Heap_resize, "bootstrap/source/generation/prelude.spite\tMemory.Heap", "resize", 2},
{(const void*)&Memory_Heap_free, "bootstrap/source/generation/prelude.spite\tMemory.Heap", "free", 3},
{(const void*)&Naive_Naive, "benchmarks/a_row_of_borrowed_items_lives_in_the_frame/naive/naive.spite\tNaive", "Naive", 10},
{(const void*)&Naive_spawn_all, "benchmarks/a_row_of_borrowed_items_lives_in_the_frame/naive/naive.spite\tNaive", "spawn_all", 21},
{(const void*)&Naive_run_ticks, "benchmarks/a_row_of_borrowed_items_lives_in_the_frame/naive/naive.spite\tNaive", "run_ticks", 36},
{(const void*)&Naive_tick_once, "benchmarks/a_row_of_borrowed_items_lives_in_the_frame/naive/naive.spite\tNaive", "tick_once", 44},
{(const void*)&spite_failed_1, "-\t-", "spite_failed_1", 0},
{(const void*)&spite_failed_2, "-\t-", "spite_failed_2", 0},
{(const void*)&spite_failed_3, "-\t-", "spite_failed_3", 0},
{(const void*)&Healer_update_each___lent_0, "benchmarks/a_row_of_borrowed_items_lives_in_the_frame/naive/healer.spite\tHealer", "update_each", 6},
{(const void*)&Mover_update_each___lent_0, "benchmarks/a_row_of_borrowed_items_lives_in_the_frame/naive/mover.spite\tMover", "update_each", 6},
{(const void*)&Position_get_place, "benchmarks/a_row_of_borrowed_items_lives_in_the_frame/naive/position.spite\tPosition", "get_place", 4},
{(const void*)&Regeneration_Regeneration, "benchmarks/a_row_of_borrowed_items_lives_in_the_frame/naive/regeneration.spite\tRegeneration", "Regeneration", 3},
{(const void*)&Velocity_Velocity, "benchmarks/a_row_of_borrowed_items_lives_in_the_frame/naive/velocity.spite\tVelocity", "Velocity", 4},
{(const void*)&Vector__Position_count, "library/vector.spite\tVector", "count", 9},
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
{(const void*)&Vector__Velocity_append, "library/vector.spite\tVector", "append", 17},
{(const void*)&Vector__Velocity_get_at, "library/vector.spite\tVector", "get_at", 23},
{(const void*)&Vector__Velocity_drop, "library/vector.spite\tVector", "drop", 291},
{(const void*)&Vector__Velocity_make_room, "library/vector.spite\tVector", "make_room", 311},
{(const void*)&Vector__Velocity__grow, "library/vector.spite\tVector", "_grow", 317},
{(const void*)&Vector__Velocity__resized, "library/vector.spite\tVector", "_resized", 328},
{(const void*)&InlineMemory__Velocity_item_at, "bootstrap/source/generation/prelude.spite\tInlineMemory", "item_at", 1},
{(const void*)&InlineMemory__Velocity_write_item, "bootstrap/source/generation/prelude.spite\tInlineMemory", "write_item", 2},
{(const void*)&InlineMemory__Velocity_block_bytes, "bootstrap/source/generation/prelude.spite\tInlineMemory", "block_bytes", 7},
{(const void*)&Vector__Health_append, "library/vector.spite\tVector", "append", 17},
{(const void*)&Vector__Health_get_at, "library/vector.spite\tVector", "get_at", 23},
{(const void*)&Vector__Health_drop, "library/vector.spite\tVector", "drop", 291},
{(const void*)&Vector__Health_make_room, "library/vector.spite\tVector", "make_room", 311},
{(const void*)&Vector__Health__grow, "library/vector.spite\tVector", "_grow", 317},
{(const void*)&Vector__Health__resized, "library/vector.spite\tVector", "_resized", 328},
{(const void*)&Vector__Health_sum_amount, "library/vector.spite\tVector", "sum_amount", 0},
{(const void*)&InlineMemory__Health_item_at, "bootstrap/source/generation/prelude.spite\tInlineMemory", "item_at", 1},
{(const void*)&InlineMemory__Health_write_item, "bootstrap/source/generation/prelude.spite\tInlineMemory", "write_item", 2},
{(const void*)&InlineMemory__Health_block_bytes, "bootstrap/source/generation/prelude.spite\tInlineMemory", "block_bytes", 7},
{(const void*)&Vector__Regeneration_append, "library/vector.spite\tVector", "append", 17},
{(const void*)&Vector__Regeneration_get_at, "library/vector.spite\tVector", "get_at", 23},
{(const void*)&Vector__Regeneration_drop, "library/vector.spite\tVector", "drop", 291},
{(const void*)&Vector__Regeneration_make_room, "library/vector.spite\tVector", "make_room", 311},
{(const void*)&Vector__Regeneration__grow, "library/vector.spite\tVector", "_grow", 317},
{(const void*)&Vector__Regeneration__resized, "library/vector.spite\tVector", "_resized", 328},
{(const void*)&InlineMemory__Regeneration_item_at, "bootstrap/source/generation/prelude.spite\tInlineMemory", "item_at", 1},
{(const void*)&InlineMemory__Regeneration_write_item, "bootstrap/source/generation/prelude.spite\tInlineMemory", "write_item", 2},
{(const void*)&InlineMemory__Regeneration_block_bytes, "bootstrap/source/generation/prelude.spite\tInlineMemory", "block_bytes", 7},
{(const void*)&List_Console_Printable_get_at, "library/list.spite\tList", "get_at", 41},
{(const void*)&List_Console_Printable_drop, "library/list.spite\tList", "drop", 496},
{(const void*)&TypedMemory__Console_Printable_read_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "read_value", 1},
{(const void*)&Vector__Position_clear, "library/vector.spite\tVector", "clear", 64},
{(const void*)&InlineMemory__Position_release_item, "bootstrap/source/generation/prelude.spite\tInlineMemory", "release_item", 4},
{(const void*)&Vector__Velocity_clear, "library/vector.spite\tVector", "clear", 64},
{(const void*)&InlineMemory__Velocity_release_item, "bootstrap/source/generation/prelude.spite\tInlineMemory", "release_item", 4},
{(const void*)&Vector__Health_clear, "library/vector.spite\tVector", "clear", 64},
{(const void*)&InlineMemory__Health_release_item, "bootstrap/source/generation/prelude.spite\tInlineMemory", "release_item", 4},
{(const void*)&Vector__Regeneration_clear, "library/vector.spite\tVector", "clear", 64},
{(const void*)&InlineMemory__Regeneration_release_item, "bootstrap/source/generation/prelude.spite\tInlineMemory", "release_item", 4},
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


return 0;
}
