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
typedef struct BinaryInput BinaryInput;
typedef struct BinaryOutput BinaryOutput;
typedef struct Build Build;
typedef struct Clock Clock;
typedef struct Console Console;
typedef struct Dictionary Dictionary;
typedef struct DynamicLibrary DynamicLibrary;
typedef struct List List;
typedef struct Memory_Arena Memory_Arena;
typedef struct Memory_Heap Memory_Heap;
typedef struct Naive Naive;
typedef struct Reading Reading;
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
static SpiteString spite_lit_1 = SPITE_STATIC_STRING("", 0);
static DynamicLibrary* spite_foreign_library_1_cache = 0;
static bool spite_foreign_library_1_tracked = false;
static DynamicLibrary* spite_foreign_library_2_cache = 0;
static bool spite_foreign_library_2_tracked = false;
typedef struct List_Byte List_Byte;
typedef struct TypedMemory__Byte TypedMemory__Byte;
static SpiteString spite_lit_2 = SPITE_STATIC_STRING("", 0);
static Clock* spite_singleton_Clock_cache = 0;
static bool spite_singleton_Clock_destroyed = false;
static int32_t spite_singleton_Clock_lock = 0;
typedef struct BinaryWriter__Reading BinaryWriter__Reading;
typedef struct BinaryFormat__Reading BinaryFormat__Reading;
typedef struct BinaryReader__Reading BinaryReader__Reading;
static SpiteString spite_lit_3 = SPITE_STATIC_STRING("", 0);
typedef void* Spite_Allocator;
struct Launcher {
SpiteHeader header;
Build* build_;
};
typedef struct List_Console_Printable List_Console_Printable;
typedef struct TypedMemory__Console_Printable TypedMemory__Console_Printable;
typedef struct SpiteBox_SpiteLong { SpiteHeader header; int64_t value; } SpiteBox_SpiteLong;
#define SPITE_FRAMED_COUNT 1073741824
struct BinaryInput {
SpiteHeader header;
int64_t address_;
int32_t count_;
int32_t position_;
bool failed_;
int32_t walked_;
int32_t found_;
SpiteString found_name_;
};
#define spite_site_1() "library/binary_input.spite:15 in BinaryInput.take"
#define spite_site_2() "library/binary_input.spite:19 in BinaryInput.take"
#define spite_site_3() "library/binary_input.spite:27 in BinaryInput.read_byte"
#define spite_site_4() "library/binary_input.spite:48 in BinaryInput.read_integer"
#define spite_site_5() "library/binary_input.spite:62 in BinaryInput.read_float"
#define spite_site_6() "library/binary_input.spite:78 in BinaryInput.read_count"
#define spite_site_7() "library/binary_input.spite:79 in BinaryInput.read_count"
#define spite_site_8() "library/binary_input.spite:81 in BinaryInput.read_count"
#define spite_site_9() "library/binary_input.spite:85 in BinaryInput.read_count"
static SpiteString spite_lit_4 = SPITE_STATIC_STRING("", 0);
#define spite_site_10() "library/binary_input.spite:93 in BinaryInput.read_text"
struct BinaryOutput {
SpiteHeader header;
List_Byte* bytes_;
int32_t walked_;
int32_t found_;
};
#define spite_site_11() "spite.crash\t0de8f51b"
#define spite_site_12() "library/binary_output.spite:12 in BinaryOutput.room"
#define spite_site_13() "library/binary_output.spite:21 in BinaryOutput.write_byte"
#define spite_site_14() "library/binary_output.spite:53 in BinaryOutput.write_count"
#define spite_site_15() "library/binary_output.spite:66 in BinaryOutput.write_text"
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
#define spite_site_16() "library/windows/clock.spite:16 in Clock.elapsed_nanoseconds"
struct Console {
SpiteHeader header;
Memory_Heap* heap_;
DynamicLibrary* library_;
int64_t input_;
};
static SpiteString spite_lit_5 = SPITE_STATIC_STRING("\n", 1);
static SpiteString spite_lit_6 = SPITE_STATIC_STRING("\n", 1);
static SpiteString spite_lit_7 = SPITE_STATIC_STRING(" ", 1);
#define spite_site_17() "library/console.spite:56 in Console._write_values"

struct DynamicLibrary {
SpiteHeader header;
SpiteString file_name_;
int64_t handle_;
};
#define SpiteFloat_to_long(self) ((int64_t)(self))
#define SpiteFloat_to_double(self) ((double)(self))
#define SpiteInteger_to_long(self) ((int64_t)(self))
#define SpiteInteger_to_unsigned_long(self) ((uint64_t)(self))
#define SpiteInteger_to_unsigned_integer(self) ((uint32_t)(self))
#define SpiteInteger_to_byte(self) ((uint8_t)(self))
#define SpiteInteger_to_float(self) ((float)(self))
static SpiteString spite_lit_8 = SPITE_STATIC_STRING("0", 1);
#define spite_site_18() "library/long.spite:15 in Long.to_string"
#define spite_site_19() "library/long.spite:17 in Long.to_string"
#define spite_site_20() "library/long.spite:18 in Long.to_string"
#define spite_site_21() "library/long.spite:22 in Long.to_string"
#define spite_site_22() "library/long.spite:26 in Long.to_string"
#define SpiteLong_to_unsigned_integer(self) ((uint32_t)(self))
#define SpiteLong_to_unsigned_long(self) ((uint64_t)(self))
#define SpiteShort_to_integer(self) ((int32_t)(self))
#define SpiteShort_to_long(self) ((int64_t)(self))
#define spite_site_23() "library/string.spite:5 in String.length"
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
#define spite_site_24() "library/memory/arena.spite:12 in Memory.Arena.allocate"
#define spite_site_25() "library/memory/arena.spite:13 in Memory.Arena.allocate"
#define spite_site_26() "library/memory/arena.spite:17 in Memory.Arena.allocate"
#define spite_site_27() "library/memory/arena.spite:25 in Memory.Arena.start_block"
#define spite_site_28() "library/memory/arena.spite:26 in Memory.Arena.start_block"
struct Memory_Heap {
SpiteHeader header;
};
struct Naive {
SpiteHeader header;
Console* console_;
Clock* clock_;
BinaryWriter__Reading* writer_;
BinaryReader__Reading* reader_;
};
#define spite_site_29() "benchmarks/a_binary_schema_is_a_constant/naive/naive.spite:12 in Naive.Naive"
#define spite_site_30() "benchmarks/a_binary_schema_is_a_constant/naive/naive.spite:15 in Naive.Naive"
static SpiteBox_SpiteString spite_lit_9_box = { { 0, -1 }, SPITE_STATIC_STRING("accepted", 8) };
typedef struct SpiteBox_SpiteInteger { SpiteHeader header; int32_t value; } SpiteBox_SpiteInteger;
static SpiteBox_SpiteString spite_lit_10_box = { { 0, -1 }, SPITE_STATIC_STRING("sensor", 6) };
static SpiteBox_SpiteString spite_lit_11_box = { { 0, -1 }, SPITE_STATIC_STRING("schema", 6) };
static SpiteString spite_lit_12 = SPITE_STATIC_STRING("microseconds ", 13);
#define spite_site_31() "spite.crash\t015817d8"
struct Reading {
SpiteHeader header;
int32_t sensor_;
float level_;
SpiteString label_;
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
struct List_Byte {
SpiteHeader header;
Memory_Heap* heap_;
TypedMemory__Byte* values_;
int64_t items_;
int32_t item_count_;
int32_t capacity_;
};
struct TypedMemory__Byte {
SpiteHeader header;
};
struct BinaryWriter__Reading {
SpiteHeader header;
BinaryFormat__Reading* format_;
};
struct BinaryFormat__Reading {
SpiteHeader header;
};
struct BinaryReader__Reading {
SpiteHeader header;
int32_t position_;
BinaryFormat__Reading* format_;
BinaryInput* input_;
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
#define spite_site_32() "library/list.spite:20 in List.append"
#define spite_site_33() "library/list.spite:856 in List._grow"
#define spite_site_34() "library/list.spite:861 in List._grow"
#define spite_site_35() "library/list.spite:671 in List.count_is_current_for_naive"
#define spite_site_36() "library/list.spite:839 in List.reserve"
#define spite_site_37() "library/list.spite:844 in List.reserve"
#define BinaryWriter__Reading_schema(self) ((void)(self), (int64_t)(4647632506827894408LL)) /* Reading{sensor:Integer;level:Float;label:String} */
typedef struct BinaryFormat__Integer BinaryFormat__Integer;
typedef struct BinaryFormat__Float BinaryFormat__Float;
typedef struct BinaryFormat__String BinaryFormat__String;
#define spite_site_38() "spite.crash\t5bd6c2fe"
#define BinaryReader__Reading_schema(self) ((void)(self), (int64_t)(4647632506827894408LL)) /* Reading{sensor:Integer;level:Float;label:String} */
struct BinaryFormat__Integer {
SpiteHeader header;
};
struct BinaryFormat__Float {
SpiteHeader header;
};
struct BinaryFormat__String {
SpiteHeader header;
};
static SpiteString spite_symbol_1 = { (int64_t)0x797469746e656469ULL, (int64_t)0x0700000000000000ULL };
Memory_Heap* spite_singleton_Memory_Heap(void);
Console_Printable Console_Printable___retain(Console_Printable self);
void Console_Printable___release(Console_Printable self);
Build* spite_singleton_Build(void);
Console* spite_singleton_Console(void);
TypedMemory__Long* spite_singleton_TypedMemory__Long(void);
DynamicLibrary* spite_foreign_library_1(void);
DynamicLibrary* spite_foreign_library_2(void);
TypedMemory__Byte* spite_singleton_TypedMemory__Byte(void);
Clock* spite_singleton_Clock(void);
BinaryFormat__Reading* spite_singleton_BinaryFormat__Reading(void);
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
void BinaryInput___init(BinaryInput* self);
BinaryInput* BinaryInput___allocate(void);
BinaryInput* BinaryInput___make(void);
static inline BinaryInput* BinaryInput___retain(BinaryInput* self);
static inline void BinaryInput___release(BinaryInput* self);
void BinaryInput___free(BinaryInput* self);
void BinaryInput_fail(BinaryInput* self);
bool BinaryInput_take(BinaryInput* self, int32_t wanted_);
int32_t BinaryInput_read_byte(BinaryInput* self);
int32_t BinaryInput_read_integer(BinaryInput* self);
float BinaryInput_read_float(BinaryInput* self);
int32_t BinaryInput_read_count(BinaryInput* self);
SpiteString BinaryInput_read_text(BinaryInput* self);
void BinaryOutput___init(BinaryOutput* self);
BinaryOutput* BinaryOutput___allocate(void);
BinaryOutput* BinaryOutput___make(List_Byte* target_);
static inline BinaryOutput* BinaryOutput___retain(BinaryOutput* self);
static inline void BinaryOutput___release(BinaryOutput* self);
void BinaryOutput___free(BinaryOutput* self);
void BinaryOutput_BinaryOutput(BinaryOutput* self, List_Byte* target_);
int64_t BinaryOutput_room(BinaryOutput* self, int32_t extra_);
void BinaryOutput_write_byte(BinaryOutput* self, int32_t value_);
void BinaryOutput_write_integer(BinaryOutput* self, int32_t value_);
void BinaryOutput_write_float(BinaryOutput* self, float value_);
void BinaryOutput_write_count(BinaryOutput* self, int32_t count_);
void BinaryOutput_write_text(BinaryOutput* self, SpiteString text_);
static SPITE_CRASH_REPORT void spite_failed_1(int32_t extra_, BinaryOutput* self);
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
int32_t SpiteInteger_bits_and(int32_t self, int32_t other_);
SpiteString SpiteLong_to_string(int64_t self);
int32_t SpiteString_length(SpiteString self);
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
static inline Naive* Naive___retain(Naive* self);
static inline void Naive___release(Naive* self);
void Naive___free(Naive* self);
void Naive_Naive(Naive* self);
List_Long* Naive_received_headers(Naive* self, int32_t count_);
bool Naive_is_current(Naive* self, int64_t header_);
int32_t Naive_sent_and_read_back(Naive* self, int32_t sensor_);
int32_t List_Long_count_is_current_for_naive(List_Long* self, Naive* owner_);
static inline SpiteTagged spite_tagged_SpiteInteger(int32_t value);
static SPITE_CRASH_REPORT void spite_failed_2(int32_t sensor_);
void Reading___init(Reading* self);
Reading* Reading___allocate(void);
Reading* Reading___default(void);
Reading* Reading___make(void);
static inline Reading* Reading___retain(Reading* self);
static inline void Reading___release(Reading* self);
void Reading___free(Reading* self);
void List_Long___init(List_Long* self);
List_Long* List_Long___allocate(void);
List_Long* List_Long___make(void);
static inline List_Long* List_Long___retain(List_Long* self);
static inline void List_Long___release(List_Long* self);
void List_Long___free(List_Long* self);
void List_Long_drop(List_Long* self);
void List_Long_append(List_Long* self, int64_t value_);
void List_Long_clear(List_Long* self);
void List_Long_drop(List_Long* self);
void List_Long_make_room(List_Long* self);
void List_Long__grow(List_Long* self);
int64_t List_Long__resized(List_Long* self, int64_t old_bytes_, int64_t new_bytes_);
int32_t List_Long_count_is_current_for_naive(List_Long* self, Naive* owner_);
void TypedMemory__Long___release(TypedMemory__Long* self);
int64_t TypedMemory__Long_read_value(TypedMemory__Long* self, int64_t address_, int32_t index_);
void TypedMemory__Long_write_value(TypedMemory__Long* self, int64_t address_, int32_t index_, int64_t value_);
void TypedMemory__Long_release_value(TypedMemory__Long* self, int64_t address_, int32_t index_);
int64_t TypedMemory__Long_value_bytes(TypedMemory__Long* self);
void List_Byte___init(List_Byte* self);
List_Byte* List_Byte___allocate(void);
List_Byte* List_Byte___make(void);
static inline List_Byte* List_Byte___retain(List_Byte* self);
static inline void List_Byte___release(List_Byte* self);
void List_Byte___free(List_Byte* self);
void List_Byte_drop(List_Byte* self);
int32_t List_Byte_count(List_Byte* self);
void List_Byte_clear(List_Byte* self);
void List_Byte_drop(List_Byte* self);
void List_Byte_reserve(List_Byte* self, int32_t wanted_);
int64_t List_Byte__resized(List_Byte* self, int64_t old_bytes_, int64_t new_bytes_);
void TypedMemory__Byte___release(TypedMemory__Byte* self);
void TypedMemory__Byte_release_value(TypedMemory__Byte* self, int64_t address_, int32_t index_);
int64_t TypedMemory__Byte_value_bytes(TypedMemory__Byte* self);
void BinaryWriter__Reading___init(BinaryWriter__Reading* self);
BinaryWriter__Reading* BinaryWriter__Reading___allocate(void);
BinaryWriter__Reading* BinaryWriter__Reading___make(void);
static inline void BinaryWriter__Reading___release(BinaryWriter__Reading* self);
void BinaryWriter__Reading___free(BinaryWriter__Reading* self);
List_Byte* BinaryWriter__Reading_write(BinaryWriter__Reading* self, Reading* value_);
void BinaryWriter__Reading_append_to(BinaryWriter__Reading* self, Reading* value_, List_Byte* bytes_);
void BinaryFormat__Reading___release(BinaryFormat__Reading* self);
void BinaryFormat__Reading_write(BinaryFormat__Reading* self, Reading* value_, BinaryOutput* output_);
Reading* BinaryFormat__Reading_read(BinaryFormat__Reading* self, BinaryInput* input_);
void BinaryReader__Reading___init(BinaryReader__Reading* self);
BinaryReader__Reading* BinaryReader__Reading___allocate(void);
BinaryReader__Reading* BinaryReader__Reading___make(void);
static inline void BinaryReader__Reading___release(BinaryReader__Reading* self);
void BinaryReader__Reading___free(BinaryReader__Reading* self);
Reading* BinaryReader__Reading_read(BinaryReader__Reading* self, List_Byte* bytes_);
Reading* BinaryReader__Reading_read_from(BinaryReader__Reading* self, List_Byte* bytes_, int32_t start_);
Reading* BinaryReader__Reading_read_input(BinaryReader__Reading* self, int32_t start_);
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
void BinaryFormat__Reading_write_attributes(BinaryFormat__Reading* self, Reading* value_, BinaryOutput* output_);
void BinaryFormat__Reading_write_sensor(BinaryFormat__Reading* self, Reading* value_, BinaryOutput* output_);
void BinaryFormat__Reading_write_level(BinaryFormat__Reading* self, Reading* value_, BinaryOutput* output_);
void BinaryFormat__Reading_write_label(BinaryFormat__Reading* self, Reading* value_, BinaryOutput* output_);
BinaryFormat__Integer* spite_singleton_BinaryFormat__Integer(void);
BinaryFormat__Float* spite_singleton_BinaryFormat__Float(void);
BinaryFormat__String* spite_singleton_BinaryFormat__String(void);
static SPITE_CRASH_REPORT void spite_failed_3(int32_t start_, BinaryReader__Reading* self);
void BinaryFormat__Integer___release(BinaryFormat__Integer* self);
void BinaryFormat__Integer_write(BinaryFormat__Integer* self, int32_t value_, BinaryOutput* output_);
int32_t BinaryFormat__Integer_read(BinaryFormat__Integer* self, BinaryInput* input_);
void BinaryFormat__Float___release(BinaryFormat__Float* self);
void BinaryFormat__Float_write(BinaryFormat__Float* self, float value_, BinaryOutput* output_);
float BinaryFormat__Float_read(BinaryFormat__Float* self, BinaryInput* input_);
void BinaryFormat__String___release(BinaryFormat__String* self);
void BinaryFormat__String_write(BinaryFormat__String* self, SpiteString value_, BinaryOutput* output_);
SpiteString BinaryFormat__String_read(BinaryFormat__String* self, BinaryInput* input_);
void BinaryFormat__Reading_read_attributes(BinaryFormat__Reading* self, Reading* created_, BinaryInput* input_);
void BinaryFormat__Reading_read_sensor(BinaryFormat__Reading* self, Reading* created_, BinaryInput* input_);
void BinaryFormat__Reading_read_level(BinaryFormat__Reading* self, Reading* created_, BinaryInput* input_);
void BinaryFormat__Reading_read_label(BinaryFormat__Reading* self, Reading* created_, BinaryInput* input_);
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
static __typeof__(&TypedMemory__Long___release) spite_folded_TypedMemory__Long___release = ((__typeof__(&TypedMemory__Long___release))&Memory_Heap___release);
static __typeof__(&TypedMemory__Byte___release) spite_folded_TypedMemory__Byte___release = ((__typeof__(&TypedMemory__Byte___release))&Memory_Heap___release);
static __typeof__(&BinaryFormat__Reading___release) spite_folded_BinaryFormat__Reading___release = ((__typeof__(&BinaryFormat__Reading___release))&Memory_Heap___release);
static __typeof__(&TypedMemory__Console_Printable___release) spite_folded_TypedMemory__Console_Printable___release = ((__typeof__(&TypedMemory__Console_Printable___release))&Memory_Heap___release);
static __typeof__(&BinaryFormat__Integer___release) spite_folded_BinaryFormat__Integer___release = ((__typeof__(&BinaryFormat__Integer___release))&Memory_Heap___release);
static __typeof__(&BinaryFormat__Float___release) spite_folded_BinaryFormat__Float___release = ((__typeof__(&BinaryFormat__Float___release))&Memory_Heap___release);
static __typeof__(&BinaryFormat__String___release) spite_folded_BinaryFormat__String___release = ((__typeof__(&BinaryFormat__String___release))&Memory_Heap___release);
static __typeof__(&List_Console_Printable_count) spite_folded_List_Console_Printable_count = ((__typeof__(&List_Console_Printable_count))&List_Byte_count);
static __typeof__(&List_Byte_clear) spite_folded_List_Byte_clear = ((__typeof__(&List_Byte_clear))&List_Long_clear);
static __typeof__(&TypedMemory__Byte_release_value) spite_folded_TypedMemory__Byte_release_value = ((__typeof__(&TypedMemory__Byte_release_value))&TypedMemory__Long_release_value);
Memory_Heap* spite_singleton_Memory_Heap(void) {
static Memory_Heap spite_object = { { 1, 94 } };
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
static TypedMemory__Long spite_object = { { 1, 120 } };
return &spite_object;
}
TypedMemory__Byte* spite_singleton_TypedMemory__Byte(void) {
static TypedMemory__Byte spite_object = { { 1, 124 } };
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
BinaryFormat__Reading* spite_singleton_BinaryFormat__Reading(void) {
static BinaryFormat__Reading spite_object = { { 1, 150 } };
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
static TypedMemory__Console_Printable spite_object = { { 1, 153 } };
return &spite_object;
}
static List_Console_Printable* List_Console_Printable___framed(List_Console_Printable* self, int64_t items, int32_t count) {
List_Console_Printable___init(self);
self->header.ref_count = 2;
self->header.class_id = 152;
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
tagged.tag = 154;
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
void BinaryInput___init(BinaryInput* self) {
self->address_ = ((int64_t)(0));
self->count_ = 0;
self->position_ = 0;
self->failed_ = false;
self->walked_ = 0;
self->found_ = 0;
self->found_name_ = spite_lit_1;
}
BinaryInput* BinaryInput___allocate(void) {
BinaryInput* self = (BinaryInput*)SPITE_MALLOC(sizeof(BinaryInput));
self->header.ref_count = 1;
self->header.class_id = 8;
BinaryInput___init(self);
#ifdef SPITE_TRACKS_BinaryInput
spite_track_BinaryInput(self);
#endif
return self;
}
BinaryInput* BinaryInput___make(void) {
BinaryInput* self = BinaryInput___allocate();
return self;
}
static inline BinaryInput* BinaryInput___retain(BinaryInput* self) {
if (self != 0) SPITE_COUNT_UP(self->header.ref_count);
return self;
}
static inline void BinaryInput___release(BinaryInput* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
BinaryInput___free(self);
}
void BinaryInput___free(BinaryInput* self) {
SpiteString___release(self->found_name_);
#ifdef SPITE_TRACKS_BinaryInput
spite_untrack_BinaryInput(self);
#endif
#ifdef SPITE_WEAK_BinaryInput
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void BinaryOutput___init(BinaryOutput* self) {
self->bytes_ = 0;
self->walked_ = 0;
self->found_ = 0;
}
BinaryOutput* BinaryOutput___allocate(void) {
BinaryOutput* self = (BinaryOutput*)SPITE_MALLOC(sizeof(BinaryOutput));
self->header.ref_count = 1;
self->header.class_id = 9;
BinaryOutput___init(self);
#ifdef SPITE_TRACKS_BinaryOutput
spite_track_BinaryOutput(self);
#endif
return self;
}
BinaryOutput* BinaryOutput___make(List_Byte* target_) {
BinaryOutput* self = BinaryOutput___allocate();
BinaryOutput_BinaryOutput(self, target_);
return self;
}
static inline BinaryOutput* BinaryOutput___retain(BinaryOutput* self) {
if (self != 0) SPITE_COUNT_UP(self->header.ref_count);
return self;
}
static inline void BinaryOutput___release(BinaryOutput* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
BinaryOutput___free(self);
}
void BinaryOutput___free(BinaryOutput* self) {
List_Byte___release(self->bytes_);
#ifdef SPITE_TRACKS_BinaryOutput
spite_untrack_BinaryOutput(self);
#endif
#ifdef SPITE_WEAK_BinaryOutput
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
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
self->file_name_ = spite_lit_2;
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
self->writer_ = BinaryWriter__Reading___make();
self->reader_ = BinaryReader__Reading___make();
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
Clock___release(self->clock_);
BinaryWriter__Reading___release(self->writer_);
BinaryReader__Reading___release(self->reader_);
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
tagged.tag = 169;
tagged.plain = 1;
tagged.value.bits = 0;
memcpy(&tagged.value, &value, sizeof(value));
return tagged;
}
void Reading___init(Reading* self) {
self->sensor_ = 0;
self->level_ = 0.0;
self->label_ = spite_lit_3;
}
Reading* Reading___allocate(void) {
Reading* self = (Reading*)SPITE_MALLOC(sizeof(Reading));
self->header.ref_count = 1;
self->header.class_id = 110;
Reading___init(self);
#ifdef SPITE_TRACKS_Reading
spite_track_Reading(self);
#endif
return self;
}
Reading* Reading___default(void) { return Reading___allocate(); }
Reading* Reading___make(void) {
Reading* self = Reading___allocate();
return self;
}
static inline Reading* Reading___retain(Reading* self) {
if (self != 0) SPITE_COUNT_UP(self->header.ref_count);
return self;
}
static inline void Reading___release(Reading* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Reading___free(self);
}
void Reading___free(Reading* self) {
SpiteString___release(self->label_);
#ifdef SPITE_TRACKS_Reading
spite_untrack_Reading(self);
#endif
#ifdef SPITE_WEAK_Reading
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
self->header.class_id = 119;
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
void List_Byte___init(List_Byte* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->values_ = spite_singleton_TypedMemory__Byte();
self->items_ = ((int64_t)(0));
self->item_count_ = 0;
self->capacity_ = 0;
}
List_Byte* List_Byte___allocate(void) {
List_Byte* self = (List_Byte*)SPITE_MALLOC(sizeof(List_Byte));
self->header.ref_count = 1;
self->header.class_id = 123;
List_Byte___init(self);
#ifdef SPITE_TRACKS_List_Byte
spite_track_List_Byte(self);
#endif
return self;
}
List_Byte* List_Byte___make(void) {
List_Byte* self = List_Byte___allocate();
return self;
}
static inline List_Byte* List_Byte___retain(List_Byte* self) {
if (self != 0) SPITE_COUNT_UP(self->header.ref_count);
return self;
}
static inline void List_Byte___release(List_Byte* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
List_Byte___free(self);
}
void List_Byte___free(List_Byte* self) {
List_Byte_drop(self);
Memory_Heap___release(self->heap_);
spite_folded_TypedMemory__Byte___release(self->values_);
#ifdef SPITE_TRACKS_List_Byte
spite_untrack_List_Byte(self);
#endif
#ifdef SPITE_WEAK_List_Byte
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void BinaryWriter__Reading___init(BinaryWriter__Reading* self) {
self->format_ = spite_singleton_BinaryFormat__Reading();
}
BinaryWriter__Reading* BinaryWriter__Reading___allocate(void) {
BinaryWriter__Reading* self = (BinaryWriter__Reading*)SPITE_MALLOC(sizeof(BinaryWriter__Reading));
self->header.ref_count = 1;
self->header.class_id = 149;
BinaryWriter__Reading___init(self);
#ifdef SPITE_TRACKS_BinaryWriter__Reading
spite_track_BinaryWriter__Reading(self);
#endif
return self;
}
BinaryWriter__Reading* BinaryWriter__Reading___make(void) {
BinaryWriter__Reading* self = BinaryWriter__Reading___allocate();
return self;
}
static inline void BinaryWriter__Reading___release(BinaryWriter__Reading* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
BinaryWriter__Reading___free(self);
}
void BinaryWriter__Reading___free(BinaryWriter__Reading* self) {
spite_folded_BinaryFormat__Reading___release(self->format_);
#ifdef SPITE_TRACKS_BinaryWriter__Reading
spite_untrack_BinaryWriter__Reading(self);
#endif
#ifdef SPITE_WEAK_BinaryWriter__Reading
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void BinaryReader__Reading___init(BinaryReader__Reading* self) {
self->position_ = 0;
self->format_ = spite_singleton_BinaryFormat__Reading();
self->input_ = BinaryInput___make();
}
BinaryReader__Reading* BinaryReader__Reading___allocate(void) {
BinaryReader__Reading* self = (BinaryReader__Reading*)SPITE_MALLOC(sizeof(BinaryReader__Reading));
self->header.ref_count = 1;
self->header.class_id = 151;
BinaryReader__Reading___init(self);
#ifdef SPITE_TRACKS_BinaryReader__Reading
spite_track_BinaryReader__Reading(self);
#endif
return self;
}
BinaryReader__Reading* BinaryReader__Reading___make(void) {
BinaryReader__Reading* self = BinaryReader__Reading___allocate();
return self;
}
static inline void BinaryReader__Reading___release(BinaryReader__Reading* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
BinaryReader__Reading___free(self);
}
void BinaryReader__Reading___free(BinaryReader__Reading* self) {
spite_folded_BinaryFormat__Reading___release(self->format_);
BinaryInput___release(self->input_);
#ifdef SPITE_TRACKS_BinaryReader__Reading
spite_untrack_BinaryReader__Reading(self);
#endif
#ifdef SPITE_WEAK_BinaryReader__Reading
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
BinaryFormat__Integer* spite_singleton_BinaryFormat__Integer(void) {
static BinaryFormat__Integer spite_object = { { 1, 170 } };
return &spite_object;
}
BinaryFormat__Float* spite_singleton_BinaryFormat__Float(void) {
static BinaryFormat__Float spite_object = { { 1, 171 } };
return &spite_object;
}
BinaryFormat__String* spite_singleton_BinaryFormat__String(void) {
static BinaryFormat__String spite_object = { { 1, 172 } };
return &spite_object;
}
void Console_Printable___release(Console_Printable self) {
if (self.plain != 0 || self.value.object == 0) return;
if (((self).tag == 0) && ((self).plain == 0)) { spite_string_box_release(self.value.object); return; }
}
SpiteString Console_Printable___call_to_string(Console_Printable self) {
if (((self).tag == 0) && ((self).plain == 0)) return SpiteString_to_string((((SpiteBox_SpiteString*)(self).value.object)->value));
if ((self).tag == 154) return SpiteLong_to_string(SPITE_TAGGED_VALUE(self, int64_t));
if ((self).tag == 169) return SpiteInteger_to_string(SPITE_TAGGED_VALUE(self, int32_t));
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
void BinaryInput_fail(BinaryInput* self) {
self->failed_ = true;
self->position_ = self->count_;
}
bool BinaryInput_take(BinaryInput* self, int32_t wanted_) {
if ((((self->failed_) || ((wanted_ > ({ int32_t spite_temp_1 = self->count_; int32_t spite_temp_2 = self->position_; int32_t spite_temp_3; if (__builtin_expect(__builtin_sub_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("count - position", "an Integer", "-", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; })))))) {
BinaryInput_fail(self);
bool spite_temp_4 = false;
return spite_temp_4;
}
self->position_ = ({ int32_t spite_temp_5 = self->position_; int32_t spite_temp_6 = wanted_; int32_t spite_temp_7; if (__builtin_expect(__builtin_add_overflow(spite_temp_5, spite_temp_6, &spite_temp_7), 0)) spite_overflowed("position + wanted", "an Integer", "+", (int64_t)spite_temp_5, (int64_t)spite_temp_6, spite_site_2()); spite_temp_7; });
bool spite_temp_8 = true;
return spite_temp_8;
}
int32_t BinaryInput_read_byte(BinaryInput* self) {
if (!(!(((!(BinaryInput_take(self, 1))))))) {
int32_t spite_temp_9 = 0;
return spite_temp_9;
}
int32_t spite_temp_10 = SpiteByte_to_integer(SpiteMemory_Address_read_byte(self->address_, SpiteInteger_to_long(({ int32_t spite_temp_11 = self->position_; int32_t spite_temp_12 = 1; int32_t spite_temp_13; if (__builtin_expect(__builtin_sub_overflow(spite_temp_11, spite_temp_12, &spite_temp_13), 0)) spite_overflowed("position - 1", "an Integer", "-", (int64_t)spite_temp_11, (int64_t)spite_temp_12, spite_site_3()); spite_temp_13; }))));
return spite_temp_10;
}
int32_t BinaryInput_read_integer(BinaryInput* self) {
if (!(!(((!(BinaryInput_take(self, 4))))))) {
int32_t spite_temp_14 = 0;
return spite_temp_14;
}
int32_t spite_temp_15 = SpiteMemory_Address_read_integer(self->address_, SpiteInteger_to_long(({ int32_t spite_temp_16 = self->position_; int32_t spite_temp_17 = 4; int32_t spite_temp_18; if (__builtin_expect(__builtin_sub_overflow(spite_temp_16, spite_temp_17, &spite_temp_18), 0)) spite_overflowed("position - 4", "an Integer", "-", (int64_t)spite_temp_16, (int64_t)spite_temp_17, spite_site_4()); spite_temp_18; })));
return spite_temp_15;
}
float BinaryInput_read_float(BinaryInput* self) {
if (!(!(((!(BinaryInput_take(self, 4))))))) {
float spite_temp_19 = 0.0;
return spite_temp_19;
}
float spite_temp_20 = SpiteMemory_Address_read_float(self->address_, SpiteInteger_to_long(({ int32_t spite_temp_21 = self->position_; int32_t spite_temp_22 = 4; int32_t spite_temp_23; if (__builtin_expect(__builtin_sub_overflow(spite_temp_21, spite_temp_22, &spite_temp_23), 0)) spite_overflowed("position - 4", "an Integer", "-", (int64_t)spite_temp_21, (int64_t)spite_temp_22, spite_site_5()); spite_temp_23; })));
return spite_temp_20;
}
int32_t BinaryInput_read_count(BinaryInput* self) {
int64_t result_ = SpiteInteger_to_long(0);
int64_t scale_ = SpiteInteger_to_long(1);
bool more_ = true;
while ((((more_) && ((!(self->failed_)))))) {
int32_t byte_ = BinaryInput_read_byte(self);
result_ = ({ int64_t spite_temp_24 = result_; int64_t spite_temp_25 = ({ int64_t spite_temp_26 = scale_; int64_t spite_temp_27 = SpiteInteger_to_long((byte_ % 128)); int64_t spite_temp_28; if (__builtin_expect(__builtin_mul_overflow(spite_temp_26, spite_temp_27, &spite_temp_28), 0)) spite_overflowed("scale * (byte % 128)", "a Long", "*", (int64_t)spite_temp_26, (int64_t)spite_temp_27, spite_site_6()); spite_temp_28; }); int64_t spite_temp_29; if (__builtin_expect(__builtin_add_overflow(spite_temp_24, spite_temp_25, &spite_temp_29), 0)) spite_overflowed("result + scale * (byte % 128)", "a Long", "+", (int64_t)spite_temp_24, (int64_t)spite_temp_25, spite_site_6()); spite_temp_29; });
scale_ = ({ int64_t spite_temp_30 = scale_; int64_t spite_temp_31 = SpiteInteger_to_long(128); int64_t spite_temp_32; if (__builtin_expect(__builtin_mul_overflow(spite_temp_30, spite_temp_31, &spite_temp_32), 0)) spite_overflowed("scale * 128", "a Long", "*", (int64_t)spite_temp_30, (int64_t)spite_temp_31, spite_site_7()); spite_temp_32; });
more_ = (byte_ >= 128);
if (((((result_ > SpiteInteger_to_long(({ int32_t spite_temp_33 = self->count_; int32_t spite_temp_34 = self->position_; int32_t spite_temp_35; if (__builtin_expect(__builtin_sub_overflow(spite_temp_33, spite_temp_34, &spite_temp_35), 0)) spite_overflowed("count - position", "an Integer", "-", (int64_t)spite_temp_33, (int64_t)spite_temp_34, spite_site_8()); spite_temp_35; })))) || ((scale_ > 34359738368))))) {
BinaryInput_fail(self);
}
}
int32_t spite_temp_36 = ({ int64_t spite_temp_37 = result_; if (__builtin_expect(spite_temp_37 < INT32_MIN || spite_temp_37 > INT32_MAX, 0)) spite_narrowed((int64_t)spite_temp_37, "a Long", "an Integer", spite_site_9()); (int32_t)spite_temp_37; });
return spite_temp_36;
}
SpiteString BinaryInput_read_text(BinaryInput* self) {
int32_t length_ = BinaryInput_read_count(self);
if (!(!(((!(BinaryInput_take(self, length_))))))) {
SpiteString spite_temp_38 = spite_lit_4;
return spite_temp_38;
}
int64_t start_ = (self->address_ + ((int64_t)(({ int32_t spite_temp_39 = self->position_; int32_t spite_temp_40 = length_; int32_t spite_temp_41; if (__builtin_expect(__builtin_sub_overflow(spite_temp_39, spite_temp_40, &spite_temp_41), 0)) spite_overflowed("position - length", "an Integer", "-", (int64_t)spite_temp_39, (int64_t)spite_temp_40, spite_site_10()); spite_temp_41; }))));
SpiteString spite_temp_42 = SpiteMemory_Address_text(start_, SpiteInteger_to_long(length_));
return spite_temp_42;
}
void BinaryOutput_BinaryOutput(BinaryOutput* self, List_Byte* target_) {
List_Byte* spite_temp_43 = List_Byte___retain(target_);
List_Byte___release(self->bytes_);
self->bytes_ = spite_temp_43;
List_Byte___release(target_);
}
int64_t BinaryOutput_room(BinaryOutput* self, int32_t extra_) {
if (!(((self->bytes_) != 0))) {
spite_failed_1(extra_, self);
}
int32_t used_ = (self->bytes_)->item_count_;
int32_t wanted_ = ({ int32_t spite_temp_44 = used_; int32_t spite_temp_45 = extra_; int32_t spite_temp_46; if (__builtin_expect(__builtin_add_overflow(spite_temp_44, spite_temp_45, &spite_temp_46), 0)) spite_overflowed("used + extra", "an Integer", "+", (int64_t)spite_temp_44, (int64_t)spite_temp_45, spite_site_12()); spite_temp_46; });
List_Byte_reserve(self->bytes_, wanted_);
(self->bytes_)->item_count_ = wanted_;
int64_t spite_temp_47 = ((self->bytes_)->items_ + ((int64_t)(used_)));
return spite_temp_47;
}
static SPITE_CRASH_REPORT void spite_failed_1(int32_t extra_, BinaryOutput* self) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_11(), stderr);
fputs("\tbytes is null", stderr);
fputs("\textra=", stderr);
{ SpiteString spite_temp_48 = SpiteInteger_to_string(extra_); spite_crash_text(spite_string_bytes(&spite_temp_48), spite_string_length(spite_temp_48)); SpiteString___release(spite_temp_48); }
fputs("\twalked=", stderr);
{ SpiteString spite_temp_49 = SpiteInteger_to_string(self->walked_); spite_crash_text(spite_string_bytes(&spite_temp_49), spite_string_length(spite_temp_49)); SpiteString___release(spite_temp_49); }
fputs("\tfound=", stderr);
{ SpiteString spite_temp_50 = SpiteInteger_to_string(self->found_); spite_crash_text(spite_string_bytes(&spite_temp_50), spite_string_length(spite_temp_50)); SpiteString___release(spite_temp_50); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
void BinaryOutput_write_byte(BinaryOutput* self, int32_t value_) {
int64_t at_ = BinaryOutput_room(self, 1);
int32_t low_bits_ = SpiteInteger_bits_and(value_, 255);
SpiteMemory_Address_write_byte(at_, SpiteInteger_to_long(0), ({ int32_t spite_temp_51 = low_bits_; if (__builtin_expect(spite_temp_51 < 0 || spite_temp_51 > UINT8_MAX, 0)) spite_narrowed((int64_t)spite_temp_51, "an Integer", "a Byte", spite_site_13()); (uint8_t)spite_temp_51; }));
}
void BinaryOutput_write_integer(BinaryOutput* self, int32_t value_) {
int64_t at_ = BinaryOutput_room(self, 4);
SpiteMemory_Address_write_integer(at_, SpiteInteger_to_long(0), value_);
}
void BinaryOutput_write_float(BinaryOutput* self, float value_) {
int64_t at_ = BinaryOutput_room(self, 4);
SpiteMemory_Address_write_float(at_, SpiteInteger_to_long(0), value_);
}
void BinaryOutput_write_count(BinaryOutput* self, int32_t count_) {
int32_t rest_ = count_;
while (((rest_ >= 128))) {
BinaryOutput_write_byte(self, ({ int32_t spite_temp_52 = (rest_ % 128); int32_t spite_temp_53 = 128; int32_t spite_temp_54; if (__builtin_expect(__builtin_add_overflow(spite_temp_52, spite_temp_53, &spite_temp_54), 0)) spite_overflowed("rest % 128 + 128", "an Integer", "+", (int64_t)spite_temp_52, (int64_t)spite_temp_53, spite_site_14()); spite_temp_54; }));
rest_ = (rest_ / 128);
}
BinaryOutput_write_byte(self, rest_);
}
void BinaryOutput_write_text(BinaryOutput* self, SpiteString text_) {
int32_t length_ = SpiteString_length(text_);
BinaryOutput_write_count(self, length_);
int64_t at_ = BinaryOutput_room(self, length_);
int32_t index_ = 0;
while (((index_ < length_))) {
int32_t code_ = SpiteString_code_at(text_, index_);
SpiteMemory_Address_write_byte(at_, SpiteInteger_to_long(index_), ({ int32_t spite_temp_55 = code_; if (__builtin_expect(spite_temp_55 < 0 || spite_temp_55 > UINT8_MAX, 0)) spite_narrowed((int64_t)spite_temp_55, "an Integer", "a Byte", spite_site_15()); (uint8_t)spite_temp_55; }));
index_ = (index_ + 1);
}
SpiteString___release(text_);
}
void Clock_Clock(Clock* self) {
int64_t spite_temp_56[1];
int64_t spite_temp_57 = SpiteInteger_to_long(8);
int64_t frequency_ = spite_temp_57 <= 8 ? (int64_t)(intptr_t)spite_temp_56 : Memory_Heap_allocate(self->heap_, spite_temp_57);
(void)(({ spite_last_foreign_call = "QueryPerformanceFrequency\tlibrary=kernel32.dll\tfrom=library/windows/clock.spite:6"; int32_t spite_temp_58 = ((int32_t (*)(int64_t))spite_foreign_1_0)((int64_t)(frequency_));  int32_t spite_foreign_result = spite_temp_58;  (void)spite_foreign_result; spite_temp_58; }));
self->_ticks_per_second_ = SpiteMemory_Address_read_long(frequency_, SpiteInteger_to_long(0));
if (frequency_ != (int64_t)(intptr_t)spite_temp_56) Memory_Heap_free(self->heap_, frequency_);
}
int64_t Clock_elapsed_nanoseconds(Clock* self) {
int64_t spite_temp_59[1];
int64_t spite_temp_60 = SpiteInteger_to_long(8);
int64_t counter_ = spite_temp_60 <= 8 ? (int64_t)(intptr_t)spite_temp_59 : Memory_Heap_allocate(self->heap_, spite_temp_60);
(void)(({ spite_last_foreign_call = "QueryPerformanceCounter\tlibrary=kernel32.dll\tfrom=library/windows/clock.spite:13"; int32_t spite_temp_61 = ((int32_t (*)(int64_t))spite_foreign_1_1)((int64_t)(counter_));  int32_t spite_foreign_result = spite_temp_61;  (void)spite_foreign_result; spite_temp_61; }));
int64_t ticks_ = SpiteMemory_Address_read_long(counter_, SpiteInteger_to_long(0));
if (counter_ != (int64_t)(intptr_t)spite_temp_59) Memory_Heap_free(self->heap_, counter_);
int64_t spite_temp_62 = ({ int64_t spite_temp_63 = ({ int64_t spite_temp_64 = ({ int64_t spite_temp_65 = ticks_; int64_t spite_temp_66 = self->_ticks_per_second_; if (spite_temp_66 == 0) spite_divided_by_zero("ticks / _ticks_per_second", spite_site_16()); int64_t spite_temp_67 = 0; if (__builtin_expect(spite_temp_66 == -1 && __builtin_sub_overflow((int64_t)0, spite_temp_65, &spite_temp_67), 0)) spite_overflowed("ticks / _ticks_per_second", "a Long", "/", (int64_t)spite_temp_65, (int64_t)spite_temp_66, spite_site_16()); (int64_t)(spite_temp_66 == -1 ? spite_temp_67 : spite_temp_65 / spite_temp_66); }); int64_t spite_temp_68 = SpiteInteger_to_long(1000000000); int64_t spite_temp_69; if (__builtin_expect(__builtin_mul_overflow(spite_temp_64, spite_temp_68, &spite_temp_69), 0)) spite_overflowed("ticks / _ticks_per_second * 1000000000", "a Long", "*", (int64_t)spite_temp_64, (int64_t)spite_temp_68, spite_site_16()); spite_temp_69; }); int64_t spite_temp_70 = ({ int64_t spite_temp_71 = ({ int64_t spite_temp_72 = ({ int64_t spite_temp_73 = ticks_; int64_t spite_temp_74 = self->_ticks_per_second_; if (spite_temp_74 == 0) spite_divided_by_zero("ticks % _ticks_per_second", spite_site_16()); (int64_t)(spite_temp_74 == -1 ? (int64_t)0 : spite_temp_73 % spite_temp_74); }); int64_t spite_temp_75 = SpiteInteger_to_long(1000000000); int64_t spite_temp_76; if (__builtin_expect(__builtin_mul_overflow(spite_temp_72, spite_temp_75, &spite_temp_76), 0)) spite_overflowed("ticks % _ticks_per_second * 1000000000", "a Long", "*", (int64_t)spite_temp_72, (int64_t)spite_temp_75, spite_site_16()); spite_temp_76; }); int64_t spite_temp_77 = self->_ticks_per_second_; if (spite_temp_77 == 0) spite_divided_by_zero("ticks % _ticks_per_second * 1000000000 / _ticks_per_second", spite_site_16()); int64_t spite_temp_78 = 0; if (__builtin_expect(spite_temp_77 == -1 && __builtin_sub_overflow((int64_t)0, spite_temp_71, &spite_temp_78), 0)) spite_overflowed("ticks % _ticks_per_second * 1000000000 / _ticks_per_second", "a Long", "/", (int64_t)spite_temp_71, (int64_t)spite_temp_77, spite_site_16()); (int64_t)(spite_temp_77 == -1 ? spite_temp_78 : spite_temp_71 / spite_temp_77); }); int64_t spite_temp_79; if (__builtin_expect(__builtin_add_overflow(spite_temp_63, spite_temp_70, &spite_temp_79), 0)) spite_overflowed("ticks / _ticks_per_second * 1000000000 + ticks % _ticks_per_second * 1000000000 / _ticks_per_second", "a Long", "+", (int64_t)spite_temp_63, (int64_t)spite_temp_70, spite_site_16()); spite_temp_79; });
return spite_temp_62;
}
void Console_print(Console* self, List_Console_Printable* values_) {
Console__write_values(self, List_Console_Printable___retain(values_), Console_Stream_output);
Console__write_output(self, spite_lit_5);
Console__flush(self);
List_Console_Printable___release(values_);
}
void Console_error(Console* self, List_Console_Printable* values_) {
Console__flush(self);
Console__write_values(self, List_Console_Printable___retain(values_), Console_Stream_error);
Console__write_error(self, spite_lit_6);
Console__flush(self);
List_Console_Printable___release(values_);
}
void Console__write_values(Console* self, List_Console_Printable* values_, Console_Stream stream_) {
int32_t index_ = 0;
while (((index_ < spite_folded_List_Console_Printable_count(values_)))) {
if (((index_ > 0))) {
Console__write_to(self, spite_lit_7, stream_);
}
SpiteString text_ = ({ Console_Printable spite_temp_80 = ({ Console_Printable spite_temp_81 = List_Console_Printable_get_at(values_, index_); if (__builtin_expect(!(SPITE_TAGGED_PRESENT(spite_temp_81)), 0)) spite_outside_list("values[index]", spite_site_17()); spite_temp_81; }); SpiteString spite_temp_82 = Console_Printable___call_to_string(spite_temp_80); Console_Printable___release(spite_temp_80); spite_temp_82; });
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
SpiteString spite_temp_83 = SpiteString___retain(file_);
SpiteString___release(self->file_name_);
self->file_name_ = spite_temp_83;
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
SpiteString spite_temp_84 = SpiteLong_to_string(wide_);
return spite_temp_84;
}
int32_t SpiteInteger_bits_and(int32_t self, int32_t other_) {
return (int32_t)(self & other_);
}
SpiteString SpiteLong_to_string(int64_t self) {
if (((self == SpiteInteger_to_long(0)))) {
SpiteString spite_temp_85 = spite_lit_8;
return spite_temp_85;
}
Memory_Heap* heap_ = spite_singleton_Memory_Heap();
int64_t buffer_bytes_ = SpiteInteger_to_long(24);
int64_t spite_temp_86[32];
int64_t spite_temp_87 = buffer_bytes_;
int64_t address_ = spite_temp_87 <= 256 ? (int64_t)(intptr_t)spite_temp_86 : Memory_Heap_allocate(heap_, spite_temp_87);
int64_t position_ = buffer_bytes_;
int64_t rest_ = self;
while (((rest_ != SpiteInteger_to_long(0)))) {
int64_t digit_ = (rest_ % SpiteInteger_to_long(10));
if (((digit_ < SpiteInteger_to_long(0)))) {
digit_ = ({ int64_t spite_temp_88 = digit_; int64_t spite_temp_89; if (__builtin_expect(__builtin_sub_overflow((int64_t)0, spite_temp_88, &spite_temp_89), 0)) spite_overflowed("-digit", "a Long", "-", (int64_t)0, (int64_t)spite_temp_88, spite_site_18()); spite_temp_89; });
}
position_ = ({ int64_t spite_temp_90 = position_; int64_t spite_temp_91 = SpiteInteger_to_long(1); int64_t spite_temp_92; if (__builtin_expect(__builtin_sub_overflow(spite_temp_90, spite_temp_91, &spite_temp_92), 0)) spite_overflowed("position - 1", "a Long", "-", (int64_t)spite_temp_90, (int64_t)spite_temp_91, spite_site_19()); spite_temp_92; });
SpiteMemory_Address_write_byte(address_, position_, ({ int64_t spite_temp_93 = ({ int64_t spite_temp_94 = digit_; int64_t spite_temp_95 = SpiteInteger_to_long(48); int64_t spite_temp_96; if (__builtin_expect(__builtin_add_overflow(spite_temp_94, spite_temp_95, &spite_temp_96), 0)) spite_overflowed("digit + 48", "a Long", "+", (int64_t)spite_temp_94, (int64_t)spite_temp_95, spite_site_20()); spite_temp_96; }); if (__builtin_expect(spite_temp_93 < 0 || spite_temp_93 > UINT8_MAX, 0)) spite_narrowed((int64_t)spite_temp_93, "a Long", "a Byte", spite_site_20()); (uint8_t)spite_temp_93; }));
rest_ = (rest_ / SpiteInteger_to_long(10));
}
if (((self < SpiteInteger_to_long(0)))) {
position_ = ({ int64_t spite_temp_97 = position_; int64_t spite_temp_98 = SpiteInteger_to_long(1); int64_t spite_temp_99; if (__builtin_expect(__builtin_sub_overflow(spite_temp_97, spite_temp_98, &spite_temp_99), 0)) spite_overflowed("position - 1", "a Long", "-", (int64_t)spite_temp_97, (int64_t)spite_temp_98, spite_site_21()); spite_temp_99; });
SpiteMemory_Address_write_byte(address_, position_, SpiteInteger_to_byte(45));
}
int64_t first_digit_ = (address_ + ((int64_t)(position_)));
SpiteString text_ = SpiteMemory_Address_text(first_digit_, ({ int64_t spite_temp_100 = buffer_bytes_; int64_t spite_temp_101 = position_; int64_t spite_temp_102; if (__builtin_expect(__builtin_sub_overflow(spite_temp_100, spite_temp_101, &spite_temp_102), 0)) spite_overflowed("buffer_bytes - position", "a Long", "-", (int64_t)spite_temp_100, (int64_t)spite_temp_101, spite_site_22()); spite_temp_102; }));
if (address_ != (int64_t)(intptr_t)spite_temp_86) Memory_Heap_free(heap_, address_);
SpiteString spite_temp_103 = SpiteString___retain(text_);
SpiteString___release(text_);
Memory_Heap___release(heap_);
return spite_temp_103;
}
int32_t SpiteString_length(SpiteString self) {
int32_t spite_temp_104 = ({ int64_t spite_temp_105 = spite_string_length(self); if (__builtin_expect(spite_temp_105 < INT32_MIN || spite_temp_105 > INT32_MAX, 0)) spite_narrowed((int64_t)spite_temp_105, "a Long", "an Integer", spite_site_23()); (int32_t)spite_temp_105; });
return spite_temp_104;
}
SpiteString SpiteString_to_string(SpiteString self) {
SpiteString spite_temp_106 = SpiteString___retain(self);
return spite_temp_106;
}
SpiteString SpiteMemory_Address_text(int64_t self, int64_t length_) {
return spite_string_from_bytes((const char*)(intptr_t)self, length_);
}
int64_t Memory_Arena_allocate(Memory_Arena* self, int64_t bytes_) {
int64_t rounded_ = ({ int64_t spite_temp_107 = (({ int64_t spite_temp_108 = bytes_; int64_t spite_temp_109 = SpiteInteger_to_long(15); int64_t spite_temp_110; if (__builtin_expect(__builtin_add_overflow(spite_temp_108, spite_temp_109, &spite_temp_110), 0)) spite_overflowed("bytes + 15", "a Long", "+", (int64_t)spite_temp_108, (int64_t)spite_temp_109, spite_site_24()); spite_temp_110; }) / SpiteInteger_to_long(16)); int64_t spite_temp_111 = SpiteInteger_to_long(16); int64_t spite_temp_112; if (__builtin_expect(__builtin_mul_overflow(spite_temp_107, spite_temp_111, &spite_temp_112), 0)) spite_overflowed("(bytes + 15) / 16 * 16", "a Long", "*", (int64_t)spite_temp_107, (int64_t)spite_temp_111, spite_site_24()); spite_temp_112; });
if (((((self->_block_ == ((int64_t)(0)))) || ((({ int64_t spite_temp_113 = self->_used_; int64_t spite_temp_114 = rounded_; int64_t spite_temp_115; if (__builtin_expect(__builtin_add_overflow(spite_temp_113, spite_temp_114, &spite_temp_115), 0)) spite_overflowed("_used + rounded", "a Long", "+", (int64_t)spite_temp_113, (int64_t)spite_temp_114, spite_site_25()); spite_temp_115; }) > self->_end_))))) {
Memory_Arena_start_block(self, rounded_);
}
int64_t address_ = (self->_block_ + ((int64_t)(self->_used_)));
self->_used_ = ({ int64_t spite_temp_116 = self->_used_; int64_t spite_temp_117 = rounded_; int64_t spite_temp_118; if (__builtin_expect(__builtin_add_overflow(spite_temp_116, spite_temp_117, &spite_temp_118), 0)) spite_overflowed("_used + rounded", "a Long", "+", (int64_t)spite_temp_116, (int64_t)spite_temp_117, spite_site_26()); spite_temp_118; });
int64_t spite_temp_119 = address_;
return spite_temp_119;
}
void Memory_Arena_free(Memory_Arena* self, int64_t _address_) {
}
void Memory_Arena_start_block(Memory_Arena* self, int64_t at_least_) {
int64_t size_ = self->_block_bytes_;
if (((({ int64_t spite_temp_120 = at_least_; int64_t spite_temp_121 = SpiteInteger_to_long(16); int64_t spite_temp_122; if (__builtin_expect(__builtin_add_overflow(spite_temp_120, spite_temp_121, &spite_temp_122), 0)) spite_overflowed("at_least + 16", "a Long", "+", (int64_t)spite_temp_120, (int64_t)spite_temp_121, spite_site_27()); spite_temp_122; }) > size_))) {
size_ = ({ int64_t spite_temp_123 = at_least_; int64_t spite_temp_124 = SpiteInteger_to_long(16); int64_t spite_temp_125; if (__builtin_expect(__builtin_add_overflow(spite_temp_123, spite_temp_124, &spite_temp_125), 0)) spite_overflowed("at_least + 16", "a Long", "+", (int64_t)spite_temp_123, (int64_t)spite_temp_124, spite_site_28()); spite_temp_125; });
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
List_Long* headers_ = Naive_received_headers(self, 200000);
int32_t accepted_ = 0;
int32_t round_ = 0;
while (((round_ < 10))) {
accepted_ = ({ int32_t spite_temp_126 = accepted_; int32_t spite_temp_127 = List_Long_count_is_current_for_naive(headers_, Naive___retain(self)); int32_t spite_temp_128; if (__builtin_expect(__builtin_add_overflow(spite_temp_126, spite_temp_127, &spite_temp_128), 0)) spite_overflowed("accepted + headers.count(is_current)", "an Integer", "+", (int64_t)spite_temp_126, (int64_t)spite_temp_127, spite_site_29()); spite_temp_128; });
round_ = (round_ + 1);
}
int64_t microseconds_ = (({ int64_t spite_temp_129 = Clock_elapsed_nanoseconds(self->clock_); int64_t spite_temp_130 = start_; int64_t spite_temp_131; if (__builtin_expect(__builtin_sub_overflow(spite_temp_129, spite_temp_130, &spite_temp_131), 0)) spite_overflowed("clock.elapsed_nanoseconds() - start", "a Long", "-", (int64_t)spite_temp_129, (int64_t)spite_temp_130, spite_site_30()); spite_temp_131; }) / SpiteInteger_to_long(1000));
int32_t sensor_ = Naive_sent_and_read_back(self, 7);
List_Console_Printable spite_framed_1; Console_Printable spite_framed_1_items[4]; int32_t spite_framed_1_count = 0;
Console_print(self->console_, ({ spite_framed_1_items[0] = spite_tagged_object(0, ((void*)&spite_lit_9_box)); spite_framed_1_items[1] = spite_tagged_SpiteInteger(accepted_); spite_framed_1_items[2] = spite_tagged_object(0, ((void*)&spite_lit_10_box)); spite_framed_1_items[3] = spite_tagged_SpiteInteger(sensor_); spite_framed_1_count = 4; List_Console_Printable___framed(&spite_framed_1, (int64_t)(intptr_t)spite_framed_1_items, 4); }));
for (int32_t spite_index = 0; spite_index < spite_framed_1_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_1_items[spite_index]); }
int64_t schema_ = BinaryWriter__Reading_schema(self->writer_);
List_Console_Printable spite_framed_2; Console_Printable spite_framed_2_items[2]; int32_t spite_framed_2_count = 0;
Console_print(self->console_, ({ spite_framed_2_items[0] = spite_tagged_object(0, ((void*)&spite_lit_11_box)); spite_framed_2_items[1] = spite_tagged_SpiteLong(schema_); spite_framed_2_count = 2; List_Console_Printable___framed(&spite_framed_2, (int64_t)(intptr_t)spite_framed_2_items, 2); }));
for (int32_t spite_index = 0; spite_index < spite_framed_2_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_2_items[spite_index]); }
List_Console_Printable spite_framed_3; Console_Printable spite_framed_3_items[1]; int32_t spite_framed_3_count = 0;
Console_error(self->console_, ({ spite_framed_3_items[0] = spite_tagged_object(0, spite_box_SpiteString(({ char spite_temp_132_digits[24]; SpiteString spite_temp_132 = SPITE_STATIC_STRING(spite_temp_132_digits, spite_long_digits(spite_temp_132_digits, (int64_t)(microseconds_))); SpiteString spite_temp_133[] = {spite_lit_12, spite_temp_132}; SpiteString spite_temp_134 = spite_string_join(2, spite_temp_133); spite_temp_134; }))); spite_framed_3_count = 1; List_Console_Printable___framed(&spite_framed_3, (int64_t)(intptr_t)spite_framed_3_items, 1); }));
for (int32_t spite_index = 0; spite_index < spite_framed_3_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_3_items[spite_index]); }
List_Long___release(headers_);
}
List_Long* Naive_received_headers(Naive* self, int32_t count_) {
List_Long* headers_ = List_Long___make();
int32_t index_ = 0;
while (((index_ < count_))) {
int64_t header_ = BinaryWriter__Reading_schema(self->writer_);
if ((((index_ % 10) == 0))) {
header_ = SpiteInteger_to_long(index_);
}
List_Long_append(headers_, header_);
index_ = (index_ + 1);
}
List_Long* spite_temp_135 = List_Long___retain(headers_);
List_Long___release(headers_);
return spite_temp_135;
}
bool Naive_is_current(Naive* self, int64_t header_) {
bool spite_temp_136 = (header_ == BinaryReader__Reading_schema(self->reader_));
return spite_temp_136;
}
int32_t Naive_sent_and_read_back(Naive* self, int32_t sensor_) {
Reading* reading_ = Reading___make();
(reading_)->sensor_ = sensor_;
List_Byte* bytes_ = BinaryWriter__Reading_write(self->writer_, Reading___retain(reading_));
Reading* read_ = BinaryReader__Reading_read(self->reader_, List_Byte___retain(bytes_));
if (!(((read_) != 0))) {
spite_failed_2(sensor_);
}
int32_t spite_temp_137 = (read_)->sensor_;
Reading___release(read_);
List_Byte___release(bytes_);
Reading___release(reading_);
return spite_temp_137;
}
static SPITE_CRASH_REPORT void spite_failed_2(int32_t sensor_) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_31(), stderr);
fputs("\tread is null", stderr);
fputs("\tsensor=", stderr);
{ SpiteString spite_temp_138 = SpiteInteger_to_string(sensor_); spite_crash_text(spite_string_bytes(&spite_temp_138), spite_string_length(spite_temp_138)); SpiteString___release(spite_temp_138); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
void List_Long_append(List_Long* self, int64_t value_) {
List_Long_make_room(self);
TypedMemory__Long_write_value(self->values_, self->items_, self->item_count_, value_);
self->item_count_ = ({ int32_t spite_temp_139 = self->item_count_; int32_t spite_temp_140 = 1; int32_t spite_temp_141; if (__builtin_expect(__builtin_add_overflow(spite_temp_139, spite_temp_140, &spite_temp_141), 0)) spite_overflowed("item_count + 1", "an Integer", "+", (int64_t)spite_temp_139, (int64_t)spite_temp_140, spite_site_32()); spite_temp_141; });
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
({ Spite_Allocator spite_temp_142 = SPITE_ALLOCATOR_List_Long(self, spite_singleton_Memory_Heap); int64_t spite_temp_143 = self->items_; if (((SpiteHeader*)(spite_temp_142))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_142), spite_temp_143); } else if (((SpiteHeader*)(spite_temp_142))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_142), spite_temp_143); } });
}
}
void List_Long_make_room(List_Long* self) {
if (((self->item_count_ == self->capacity_))) {
List_Long__grow(self);
}
}
void List_Long__grow(List_Long* self) {
int32_t grown_ = ({ int32_t spite_temp_144 = self->capacity_; int32_t spite_temp_145 = 2; int32_t spite_temp_146; if (__builtin_expect(__builtin_mul_overflow(spite_temp_144, spite_temp_145, &spite_temp_146), 0)) spite_overflowed("capacity * 2", "an Integer", "*", (int64_t)spite_temp_144, (int64_t)spite_temp_145, spite_site_33()); spite_temp_146; });
if (((self->capacity_ == 0))) {
grown_ = 4;
}
int64_t bytes_ = TypedMemory__Long_value_bytes(self->values_);
self->items_ = List_Long__resized(self, ({ int64_t spite_temp_147 = bytes_; int64_t spite_temp_148 = SpiteInteger_to_long(self->capacity_); int64_t spite_temp_149; if (__builtin_expect(__builtin_mul_overflow(spite_temp_147, spite_temp_148, &spite_temp_149), 0)) spite_overflowed("bytes * capacity", "a Long", "*", (int64_t)spite_temp_147, (int64_t)spite_temp_148, spite_site_34()); spite_temp_149; }), ({ int64_t spite_temp_150 = bytes_; int64_t spite_temp_151 = SpiteInteger_to_long(grown_); int64_t spite_temp_152; if (__builtin_expect(__builtin_mul_overflow(spite_temp_150, spite_temp_151, &spite_temp_152), 0)) spite_overflowed("bytes * grown", "a Long", "*", (int64_t)spite_temp_150, (int64_t)spite_temp_151, spite_site_34()); spite_temp_152; }));
self->capacity_ = grown_;
}
int64_t List_Long__resized(List_Long* self, int64_t old_bytes_, int64_t new_bytes_) {
if ((({ Spite_Allocator spite_temp_153 = SPITE_ALLOCATOR_List_Long(self, spite_singleton_Memory_Heap); bool spite_temp_154 = (((SpiteHeader*)(spite_temp_153))->class_id == 94); spite_temp_154; }))) {
int64_t spite_temp_155 = Memory_Heap_resize(self->heap_, self->items_, new_bytes_);
return spite_temp_155;
}
int64_t moved_ = ({ Spite_Allocator spite_temp_156 = SPITE_ALLOCATOR_List_Long(self, spite_singleton_Memory_Heap); int64_t spite_temp_157 = new_bytes_; int64_t spite_temp_158 = 0; if (((SpiteHeader*)(spite_temp_156))->class_id == 93) { spite_temp_158 = Memory_Arena_allocate(((Memory_Arena*)spite_temp_156), spite_temp_157); } else if (((SpiteHeader*)(spite_temp_156))->class_id == 94) { spite_temp_158 = Memory_Heap_allocate(((Memory_Heap*)spite_temp_156), spite_temp_157); } spite_temp_158; });
if (((self->items_ != ((int64_t)(0))))) {
SpiteMemory_Address_copy_to(self->items_, moved_, old_bytes_);
({ Spite_Allocator spite_temp_159 = SPITE_ALLOCATOR_List_Long(self, spite_singleton_Memory_Heap); int64_t spite_temp_160 = self->items_; if (((SpiteHeader*)(spite_temp_159))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_159), spite_temp_160); } else if (((SpiteHeader*)(spite_temp_159))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_159), spite_temp_160); } });
}
int64_t spite_temp_161 = moved_;
return spite_temp_161;
}
int32_t List_Long_count_is_current_for_naive(List_Long* self, Naive* owner_) {
int32_t counted_ = 0;
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
int64_t item_ = TypedMemory__Long_read_value(self->values_, self->items_, index_);
if ((Naive_is_current(owner_, item_))) {
counted_ = ({ int32_t spite_temp_162 = counted_; int32_t spite_temp_163 = 1; int32_t spite_temp_164; if (__builtin_expect(__builtin_add_overflow(spite_temp_162, spite_temp_163, &spite_temp_164), 0)) spite_overflowed("counted + 1", "an Integer", "+", (int64_t)spite_temp_162, (int64_t)spite_temp_163, spite_site_35()); spite_temp_164; });
}
index_ = (index_ + 1);
}
int32_t spite_temp_165 = counted_;
Naive___release(owner_);
return spite_temp_165;
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
int32_t List_Byte_count(List_Byte* self) {
int32_t spite_temp_166 = self->item_count_;
return spite_temp_166;
}
void List_Byte_drop(List_Byte* self) {
spite_folded_List_Byte_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_167 = SPITE_ALLOCATOR_List_Byte(self, spite_singleton_Memory_Heap); int64_t spite_temp_168 = self->items_; if (((SpiteHeader*)(spite_temp_167))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_167), spite_temp_168); } else if (((SpiteHeader*)(spite_temp_167))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_167), spite_temp_168); } });
}
}
void List_Byte_reserve(List_Byte* self, int32_t wanted_) {
if (((wanted_ > self->capacity_))) {
int32_t grown_ = ({ int32_t spite_temp_169 = self->capacity_; int32_t spite_temp_170 = 2; int32_t spite_temp_171; if (__builtin_expect(__builtin_mul_overflow(spite_temp_169, spite_temp_170, &spite_temp_171), 0)) spite_overflowed("capacity * 2", "an Integer", "*", (int64_t)spite_temp_169, (int64_t)spite_temp_170, spite_site_36()); spite_temp_171; });
if (((grown_ < wanted_))) {
grown_ = wanted_;
}
int64_t bytes_ = TypedMemory__Byte_value_bytes(self->values_);
self->items_ = List_Byte__resized(self, ({ int64_t spite_temp_172 = bytes_; int64_t spite_temp_173 = SpiteInteger_to_long(self->capacity_); int64_t spite_temp_174; if (__builtin_expect(__builtin_mul_overflow(spite_temp_172, spite_temp_173, &spite_temp_174), 0)) spite_overflowed("bytes * capacity", "a Long", "*", (int64_t)spite_temp_172, (int64_t)spite_temp_173, spite_site_37()); spite_temp_174; }), ({ int64_t spite_temp_175 = bytes_; int64_t spite_temp_176 = SpiteInteger_to_long(grown_); int64_t spite_temp_177; if (__builtin_expect(__builtin_mul_overflow(spite_temp_175, spite_temp_176, &spite_temp_177), 0)) spite_overflowed("bytes * grown", "a Long", "*", (int64_t)spite_temp_175, (int64_t)spite_temp_176, spite_site_37()); spite_temp_177; }));
self->capacity_ = grown_;
}
}
int64_t List_Byte__resized(List_Byte* self, int64_t old_bytes_, int64_t new_bytes_) {
if ((({ Spite_Allocator spite_temp_178 = SPITE_ALLOCATOR_List_Byte(self, spite_singleton_Memory_Heap); bool spite_temp_179 = (((SpiteHeader*)(spite_temp_178))->class_id == 94); spite_temp_179; }))) {
int64_t spite_temp_180 = Memory_Heap_resize(self->heap_, self->items_, new_bytes_);
return spite_temp_180;
}
int64_t moved_ = ({ Spite_Allocator spite_temp_181 = SPITE_ALLOCATOR_List_Byte(self, spite_singleton_Memory_Heap); int64_t spite_temp_182 = new_bytes_; int64_t spite_temp_183 = 0; if (((SpiteHeader*)(spite_temp_181))->class_id == 93) { spite_temp_183 = Memory_Arena_allocate(((Memory_Arena*)spite_temp_181), spite_temp_182); } else if (((SpiteHeader*)(spite_temp_181))->class_id == 94) { spite_temp_183 = Memory_Heap_allocate(((Memory_Heap*)spite_temp_181), spite_temp_182); } spite_temp_183; });
if (((self->items_ != ((int64_t)(0))))) {
SpiteMemory_Address_copy_to(self->items_, moved_, old_bytes_);
({ Spite_Allocator spite_temp_184 = SPITE_ALLOCATOR_List_Byte(self, spite_singleton_Memory_Heap); int64_t spite_temp_185 = self->items_; if (((SpiteHeader*)(spite_temp_184))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_184), spite_temp_185); } else if (((SpiteHeader*)(spite_temp_184))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_184), spite_temp_185); } });
}
int64_t spite_temp_186 = moved_;
return spite_temp_186;
}
int64_t TypedMemory__Byte_value_bytes(TypedMemory__Byte* self) {
return (int64_t)sizeof(uint8_t);
}
List_Byte* BinaryWriter__Reading_write(BinaryWriter__Reading* self, Reading* value_) {
List_Byte* bytes_ = List_Byte___make();
BinaryWriter__Reading_append_to(self, Reading___retain(value_), List_Byte___retain(bytes_));
List_Byte* spite_temp_187 = List_Byte___retain(bytes_);
List_Byte___release(bytes_);
Reading___release(value_);
return spite_temp_187;
}
void BinaryWriter__Reading_append_to(BinaryWriter__Reading* self, Reading* value_, List_Byte* bytes_) {
BinaryOutput* output_ = BinaryOutput___make(List_Byte___retain(bytes_));
BinaryFormat__Reading_write(self->format_, Reading___retain(value_), BinaryOutput___retain(output_));
BinaryOutput___release(output_);
List_Byte___release(bytes_);
Reading___release(value_);
}
void BinaryFormat__Reading_write(BinaryFormat__Reading* self, Reading* value_, BinaryOutput* output_) {
{
{
{
{
{
{
{
{
{
{
{
{
{
{
BinaryFormat__Reading_write_attributes(self, Reading___retain(value_), BinaryOutput___retain(output_));
}
}
}
}
}
}
}
}
}
}
}
}
}
}
BinaryOutput___release(output_);
Reading___release(value_);
}
void BinaryFormat__Reading_write_attributes(BinaryFormat__Reading* self, Reading* value_, BinaryOutput* output_) {
BinaryFormat__Reading_write_sensor(self, Reading___retain(value_), BinaryOutput___retain(output_));
BinaryFormat__Reading_write_level(self, Reading___retain(value_), BinaryOutput___retain(output_));
BinaryFormat__Reading_write_label(self, Reading___retain(value_), BinaryOutput___retain(output_));
BinaryOutput___release(output_);
Reading___release(value_);
}
void BinaryFormat__Reading_write_sensor(BinaryFormat__Reading* self, Reading* value_, BinaryOutput* output_) {
{
BinaryFormat__Integer* format_ = spite_singleton_BinaryFormat__Integer();
BinaryFormat__Integer_write(format_, (value_)->sensor_, BinaryOutput___retain(output_));
spite_folded_BinaryFormat__Integer___release(format_);
}
BinaryOutput___release(output_);
Reading___release(value_);
}
void BinaryFormat__Reading_write_level(BinaryFormat__Reading* self, Reading* value_, BinaryOutput* output_) {
{
BinaryFormat__Float* format_ = spite_singleton_BinaryFormat__Float();
BinaryFormat__Float_write(format_, (value_)->level_, BinaryOutput___retain(output_));
spite_folded_BinaryFormat__Float___release(format_);
}
BinaryOutput___release(output_);
Reading___release(value_);
}
void BinaryFormat__Reading_write_label(BinaryFormat__Reading* self, Reading* value_, BinaryOutput* output_) {
{
BinaryFormat__String* format_ = spite_singleton_BinaryFormat__String();
BinaryFormat__String_write(format_, SpiteString___retain((value_)->label_), BinaryOutput___retain(output_));
spite_folded_BinaryFormat__String___release(format_);
}
BinaryOutput___release(output_);
Reading___release(value_);
}
Reading* BinaryReader__Reading_read(BinaryReader__Reading* self, List_Byte* bytes_) {
Reading* read_value_ = BinaryReader__Reading_read_from(self, List_Byte___retain(bytes_), 0);
if (!(((self->position_ == List_Byte_count(bytes_))))) {
Reading___release(read_value_);
List_Byte___release(bytes_);
return 0;
}
Reading* spite_temp_188 = Reading___retain(read_value_);
Reading___release(read_value_);
List_Byte___release(bytes_);
return spite_temp_188;
}
Reading* BinaryReader__Reading_read_from(BinaryReader__Reading* self, List_Byte* bytes_, int32_t start_) {
if (!(((start_ >= 0)))) {
spite_failed_3(start_, self);
}
BinaryInput* spite_temp_189 = self->input_;
(spite_temp_189)->address_ = (bytes_)->items_;
BinaryInput* spite_temp_190 = self->input_;
(spite_temp_190)->count_ = List_Byte_count(bytes_);
Reading* spite_temp_191 = BinaryReader__Reading_read_input(self, start_);
List_Byte___release(bytes_);
return spite_temp_191;
}
static SPITE_CRASH_REPORT void spite_failed_3(int32_t start_, BinaryReader__Reading* self) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_38(), stderr);
fputs("\tstart=", stderr);
{ SpiteString spite_temp_192 = SpiteInteger_to_string(start_); fwrite(spite_string_bytes(&spite_temp_192), 1, (size_t)spite_string_length(spite_temp_192), stderr); SpiteString___release(spite_temp_192); }
fputs("\tposition=", stderr);
{ SpiteString spite_temp_193 = SpiteInteger_to_string(self->position_); spite_crash_text(spite_string_bytes(&spite_temp_193), spite_string_length(spite_temp_193)); SpiteString___release(spite_temp_193); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
Reading* BinaryReader__Reading_read_input(BinaryReader__Reading* self, int32_t start_) {
if (!(((start_ < (self->input_)->count_)))) {
return 0;
}
BinaryInput* spite_temp_194 = self->input_;
(spite_temp_194)->position_ = start_;
BinaryInput* spite_temp_195 = self->input_;
(spite_temp_195)->failed_ = false;
Reading* read_value_ = BinaryFormat__Reading_read(self->format_, BinaryInput___retain(self->input_));
if (!(((!((self->input_)->failed_))))) {
Reading___release(read_value_);
return 0;
}
self->position_ = (self->input_)->position_;
Reading* spite_temp_196 = Reading___retain(read_value_);
Reading___release(read_value_);
return spite_temp_196;
}
Console_Printable List_Console_Printable_get_at(List_Console_Printable* self, int32_t index_) {
if ((((index_ >= 0))) && (((index_ < self->item_count_)))) {
Console_Printable spite_temp_197 = TypedMemory__Console_Printable_read_value(self->values_, self->items_, index_);
return spite_temp_197;
}
Console_Printable spite_temp_198 = SPITE_TAGGED_NULL;
return spite_temp_198;
}
void List_Console_Printable_drop(List_Console_Printable* self) {
List_Console_Printable_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_199 = SPITE_ALLOCATOR_List_Console_Printable(self, spite_singleton_Memory_Heap); int64_t spite_temp_200 = self->items_; if (((SpiteHeader*)(spite_temp_199))->class_id == 93) { Memory_Arena_free(((Memory_Arena*)spite_temp_199), spite_temp_200); } else if (((SpiteHeader*)(spite_temp_199))->class_id == 94) { Memory_Heap_free(((Memory_Heap*)spite_temp_199), spite_temp_200); } });
}
}
Console_Printable TypedMemory__Console_Printable_read_value(TypedMemory__Console_Printable* self, int64_t address_, int32_t index_) {
return Console_Printable___retain(((Console_Printable*)(intptr_t)address_)[index_]);
}
void BinaryFormat__Integer_write(BinaryFormat__Integer* self, int32_t value_, BinaryOutput* output_) {
{
{
{
{
{
BinaryOutput_write_integer(output_, value_);
}
}
}
}
}
BinaryOutput___release(output_);
}
void BinaryFormat__Float_write(BinaryFormat__Float* self, float value_, BinaryOutput* output_) {
{
{
{
{
{
{
{
{
{
BinaryOutput_write_float(output_, value_);
}
}
}
}
}
}
}
}
}
BinaryOutput___release(output_);
}
void BinaryFormat__String_write(BinaryFormat__String* self, SpiteString value_, BinaryOutput* output_) {
{
{
{
{
{
{
{
{
{
{
{
BinaryOutput_write_text(output_, SpiteString___retain(value_));
}
}
}
}
}
}
}
}
}
}
}
BinaryOutput___release(output_);
SpiteString___release(value_);
}
Reading* BinaryFormat__Reading_read(BinaryFormat__Reading* self, BinaryInput* input_) {
{
{
{
{
{
{
{
{
{
{
{
{
{
{
{
{
Reading* created_ = Reading___default();
BinaryFormat__Reading_read_attributes(self, Reading___retain(created_), BinaryInput___retain(input_));
Reading* spite_temp_201 = Reading___retain(created_);
Reading___release(created_);
BinaryInput___release(input_);
return spite_temp_201;
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
void BinaryFormat__Reading_read_attributes(BinaryFormat__Reading* self, Reading* created_, BinaryInput* input_) {
BinaryFormat__Reading_read_sensor(self, Reading___retain(created_), BinaryInput___retain(input_));
BinaryFormat__Reading_read_level(self, Reading___retain(created_), BinaryInput___retain(input_));
BinaryFormat__Reading_read_label(self, Reading___retain(created_), BinaryInput___retain(input_));
BinaryInput___release(input_);
Reading___release(created_);
}
void BinaryFormat__Reading_read_sensor(BinaryFormat__Reading* self, Reading* created_, BinaryInput* input_) {
{
BinaryFormat__Integer* format_ = spite_singleton_BinaryFormat__Integer();
(created_)->sensor_ = BinaryFormat__Integer_read(format_, BinaryInput___retain(input_));
spite_folded_BinaryFormat__Integer___release(format_);
}
BinaryInput___release(input_);
Reading___release(created_);
}
void BinaryFormat__Reading_read_level(BinaryFormat__Reading* self, Reading* created_, BinaryInput* input_) {
{
BinaryFormat__Float* format_ = spite_singleton_BinaryFormat__Float();
(created_)->level_ = BinaryFormat__Float_read(format_, BinaryInput___retain(input_));
spite_folded_BinaryFormat__Float___release(format_);
}
BinaryInput___release(input_);
Reading___release(created_);
}
void BinaryFormat__Reading_read_label(BinaryFormat__Reading* self, Reading* created_, BinaryInput* input_) {
{
BinaryFormat__String* format_ = spite_singleton_BinaryFormat__String();
SpiteString spite_temp_202 = BinaryFormat__String_read(format_, BinaryInput___retain(input_));
SpiteString___release((created_)->label_);
(created_)->label_ = spite_temp_202;
spite_folded_BinaryFormat__String___release(format_);
}
BinaryInput___release(input_);
Reading___release(created_);
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
int32_t BinaryFormat__Integer_read(BinaryFormat__Integer* self, BinaryInput* input_) {
{
{
{
{
{
{
{
int32_t spite_temp_203 = BinaryInput_read_integer(input_);
BinaryInput___release(input_);
return spite_temp_203;
}
}
}
}
}
}
}
}
float BinaryFormat__Float_read(BinaryFormat__Float* self, BinaryInput* input_) {
{
{
{
{
{
{
{
{
{
{
{
float spite_temp_204 = BinaryInput_read_float(input_);
BinaryInput___release(input_);
return spite_temp_204;
}
}
}
}
}
}
}
}
}
}
}
}
SpiteString BinaryFormat__String_read(BinaryFormat__String* self, BinaryInput* input_) {
{
{
{
{
{
{
{
{
{
{
{
{
{
SpiteString spite_temp_205 = BinaryInput_read_text(input_);
BinaryInput___release(input_);
return spite_temp_205;
}
}
}
}
}
}
}
}
}
}
}
}
}
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
{(const void*)&spite_singleton_Memory_Heap, "-\t-", "spite_singleton_Memory_Heap", 0},
{(const void*)&Console_Printable___retain, "-\t-", "Console_Printable___retain", 0},
{(const void*)&spite_singleton_Build, "-\t-", "spite_singleton_Build", 0},
{(const void*)&spite_singleton_Console_teardown, "-\t-", "spite_singleton_Console_teardown", 0},
{(const void*)&spite_singleton_Console, "-\t-", "spite_singleton_Console", 0},
{(const void*)&spite_singleton_TypedMemory__Long, "-\t-", "spite_singleton_TypedMemory__Long", 0},
{(const void*)&spite_singleton_TypedMemory__Byte, "-\t-", "spite_singleton_TypedMemory__Byte", 0},
{(const void*)&spite_singleton_Clock_teardown, "-\t-", "spite_singleton_Clock_teardown", 0},
{(const void*)&spite_singleton_Clock, "-\t-", "spite_singleton_Clock", 0},
{(const void*)&spite_singleton_BinaryFormat__Reading, "-\t-", "spite_singleton_BinaryFormat__Reading", 0},
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
{(const void*)&BinaryInput___init, "-\t-", "BinaryInput___init", 0},
{(const void*)&BinaryInput___allocate, "-\t-", "BinaryInput___allocate", 0},
{(const void*)&BinaryInput___make, "-\t-", "BinaryInput___make", 0},
{(const void*)&BinaryInput___retain, "-\t-", "BinaryInput___retain", 0},
{(const void*)&BinaryInput___release, "-\t-", "BinaryInput___release", 0},
{(const void*)&BinaryInput___free, "-\t-", "BinaryInput___free", 0},
{(const void*)&BinaryOutput___init, "-\t-", "BinaryOutput___init", 0},
{(const void*)&BinaryOutput___allocate, "-\t-", "BinaryOutput___allocate", 0},
{(const void*)&BinaryOutput___make, "-\t-", "BinaryOutput___make", 0},
{(const void*)&BinaryOutput___retain, "-\t-", "BinaryOutput___retain", 0},
{(const void*)&BinaryOutput___release, "-\t-", "BinaryOutput___release", 0},
{(const void*)&BinaryOutput___free, "-\t-", "BinaryOutput___free", 0},
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
{(const void*)&Naive___retain, "-\t-", "Naive___retain", 0},
{(const void*)&Naive___release, "-\t-", "Naive___release", 0},
{(const void*)&Naive___free, "-\t-", "Naive___free", 0},
{(const void*)&spite_tagged_SpiteInteger, "-\t-", "spite_tagged_SpiteInteger", 0},
{(const void*)&Reading___init, "-\t-", "Reading___init", 0},
{(const void*)&Reading___allocate, "-\t-", "Reading___allocate", 0},
{(const void*)&Reading___make, "-\t-", "Reading___make", 0},
{(const void*)&Reading___retain, "-\t-", "Reading___retain", 0},
{(const void*)&Reading___release, "-\t-", "Reading___release", 0},
{(const void*)&Reading___free, "-\t-", "Reading___free", 0},
{(const void*)&List_Long___init, "-\t-", "List_Long___init", 0},
{(const void*)&List_Long___allocate, "-\t-", "List_Long___allocate", 0},
{(const void*)&List_Long___make, "-\t-", "List_Long___make", 0},
{(const void*)&List_Long___retain, "-\t-", "List_Long___retain", 0},
{(const void*)&List_Long___release, "-\t-", "List_Long___release", 0},
{(const void*)&List_Long___free, "-\t-", "List_Long___free", 0},
{(const void*)&List_Byte___init, "-\t-", "List_Byte___init", 0},
{(const void*)&List_Byte___allocate, "-\t-", "List_Byte___allocate", 0},
{(const void*)&List_Byte___make, "-\t-", "List_Byte___make", 0},
{(const void*)&List_Byte___retain, "-\t-", "List_Byte___retain", 0},
{(const void*)&List_Byte___release, "-\t-", "List_Byte___release", 0},
{(const void*)&List_Byte___free, "-\t-", "List_Byte___free", 0},
{(const void*)&BinaryWriter__Reading___init, "-\t-", "BinaryWriter__Reading___init", 0},
{(const void*)&BinaryWriter__Reading___allocate, "-\t-", "BinaryWriter__Reading___allocate", 0},
{(const void*)&BinaryWriter__Reading___make, "-\t-", "BinaryWriter__Reading___make", 0},
{(const void*)&BinaryWriter__Reading___release, "-\t-", "BinaryWriter__Reading___release", 0},
{(const void*)&BinaryWriter__Reading___free, "-\t-", "BinaryWriter__Reading___free", 0},
{(const void*)&BinaryReader__Reading___init, "-\t-", "BinaryReader__Reading___init", 0},
{(const void*)&BinaryReader__Reading___allocate, "-\t-", "BinaryReader__Reading___allocate", 0},
{(const void*)&BinaryReader__Reading___make, "-\t-", "BinaryReader__Reading___make", 0},
{(const void*)&BinaryReader__Reading___release, "-\t-", "BinaryReader__Reading___release", 0},
{(const void*)&BinaryReader__Reading___free, "-\t-", "BinaryReader__Reading___free", 0},
{(const void*)&List_Console_Printable___init, "-\t-", "List_Console_Printable___init", 0},
{(const void*)&List_Console_Printable___retain, "-\t-", "List_Console_Printable___retain", 0},
{(const void*)&List_Console_Printable___release, "-\t-", "List_Console_Printable___release", 0},
{(const void*)&List_Console_Printable___free, "-\t-", "List_Console_Printable___free", 0},
{(const void*)&spite_singleton_BinaryFormat__Integer, "-\t-", "spite_singleton_BinaryFormat__Integer", 0},
{(const void*)&spite_singleton_BinaryFormat__Float, "-\t-", "spite_singleton_BinaryFormat__Float", 0},
{(const void*)&spite_singleton_BinaryFormat__String, "-\t-", "spite_singleton_BinaryFormat__String", 0},
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
{(const void*)&BinaryInput_fail, "library/binary_input.spite\tBinaryInput", "fail", 9},
{(const void*)&BinaryInput_take, "library/binary_input.spite\tBinaryInput", "take", 14},
{(const void*)&BinaryInput_read_byte, "library/binary_input.spite\tBinaryInput", "read_byte", 23},
{(const void*)&BinaryInput_read_integer, "library/binary_input.spite\tBinaryInput", "read_integer", 44},
{(const void*)&BinaryInput_read_float, "library/binary_input.spite\tBinaryInput", "read_float", 58},
{(const void*)&BinaryInput_read_count, "library/binary_input.spite\tBinaryInput", "read_count", 72},
{(const void*)&BinaryInput_read_text, "library/binary_input.spite\tBinaryInput", "read_text", 88},
{(const void*)&BinaryOutput_BinaryOutput, "library/binary_output.spite\tBinaryOutput", "BinaryOutput", 5},
{(const void*)&BinaryOutput_room, "library/binary_output.spite\tBinaryOutput", "room", 9},
{(const void*)&spite_failed_1, "-\t-", "spite_failed_1", 0},
{(const void*)&BinaryOutput_write_byte, "library/binary_output.spite\tBinaryOutput", "write_byte", 18},
{(const void*)&BinaryOutput_write_integer, "library/binary_output.spite\tBinaryOutput", "write_integer", 30},
{(const void*)&BinaryOutput_write_float, "library/binary_output.spite\tBinaryOutput", "write_float", 40},
{(const void*)&BinaryOutput_write_count, "library/binary_output.spite\tBinaryOutput", "write_count", 50},
{(const void*)&BinaryOutput_write_text, "library/binary_output.spite\tBinaryOutput", "write_text", 59},
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
{(const void*)&SpiteInteger_bits_and, "bootstrap/source/generation/prelude.spite\tInteger", "bits_and", 4},
{(const void*)&SpiteLong_to_string, "library/long.spite\tLong", "to_string", 3},
{(const void*)&SpiteString_length, "library/string.spite\tString", "length", 4},
{(const void*)&SpiteString_to_string, "library/string.spite\tString", "to_string", 184},
{(const void*)&SpiteMemory_Address_text, "library/memory/address.spite\tMemory.Address", "text", 3},
{(const void*)&Memory_Arena_allocate, "library/memory/arena.spite\tMemory.Arena", "allocate", 11},
{(const void*)&Memory_Arena_free, "library/memory/arena.spite\tMemory.Arena", "free", 21},
{(const void*)&Memory_Arena_start_block, "library/memory/arena.spite\tMemory.Arena", "start_block", 23},
{(const void*)&Memory_Heap_allocate, "bootstrap/source/generation/prelude.spite\tMemory.Heap", "allocate", 1},
{(const void*)&Memory_Heap_resize, "bootstrap/source/generation/prelude.spite\tMemory.Heap", "resize", 2},
{(const void*)&Memory_Heap_free, "bootstrap/source/generation/prelude.spite\tMemory.Heap", "free", 3},
{(const void*)&Naive_Naive, "benchmarks/a_binary_schema_is_a_constant/naive/naive.spite\tNaive", "Naive", 6},
{(const void*)&Naive_received_headers, "benchmarks/a_binary_schema_is_a_constant/naive/naive.spite\tNaive", "received_headers", 23},
{(const void*)&Naive_is_current, "benchmarks/a_binary_schema_is_a_constant/naive/naive.spite\tNaive", "is_current", 37},
{(const void*)&Naive_sent_and_read_back, "benchmarks/a_binary_schema_is_a_constant/naive/naive.spite\tNaive", "sent_and_read_back", 41},
{(const void*)&spite_failed_2, "-\t-", "spite_failed_2", 0},
{(const void*)&List_Long_append, "library/list.spite\tList", "append", 17},
{(const void*)&List_Long_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&List_Long_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_Long_make_room, "library/list.spite\tList", "make_room", 849},
{(const void*)&List_Long__grow, "library/list.spite\tList", "_grow", 855},
{(const void*)&List_Long__resized, "library/list.spite\tList", "_resized", 865},
{(const void*)&List_Long_count_is_current_for_naive, "library/list.spite\tList", "count_is_current_for_naive", 0},
{(const void*)&TypedMemory__Long_read_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "read_value", 1},
{(const void*)&TypedMemory__Long_write_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "write_value", 2},
{(const void*)&TypedMemory__Long_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&TypedMemory__Long_value_bytes, "bootstrap/source/generation/prelude.spite\tTypedMemory", "value_bytes", 4},
{(const void*)&List_Byte_count, "library/list.spite\tList", "count", 9},
{(const void*)&List_Byte_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_Byte_reserve, "library/list.spite\tList", "reserve", 837},
{(const void*)&List_Byte__resized, "library/list.spite\tList", "_resized", 865},
{(const void*)&TypedMemory__Byte_value_bytes, "bootstrap/source/generation/prelude.spite\tTypedMemory", "value_bytes", 4},
{(const void*)&BinaryWriter__Reading_write, "library/binary_writer.spite\tBinaryWriter", "write", 5},
{(const void*)&BinaryWriter__Reading_append_to, "library/binary_writer.spite\tBinaryWriter", "append_to", 11},
{(const void*)&BinaryFormat__Reading_write, "library/binary_format.spite\tBinaryFormat", "write", 5},
{(const void*)&BinaryFormat__Reading_write_attributes, "library/binary_format.spite\tBinaryFormat", "write_attributes", 103},
{(const void*)&BinaryFormat__Reading_write_sensor, "library/binary_format.spite\tBinaryFormat", "write_sensor", 0},
{(const void*)&BinaryFormat__Reading_write_level, "library/binary_format.spite\tBinaryFormat", "write_level", 0},
{(const void*)&BinaryFormat__Reading_write_label, "library/binary_format.spite\tBinaryFormat", "write_label", 0},
{(const void*)&BinaryReader__Reading_read, "library/binary_reader.spite\tBinaryReader", "read", 7},
{(const void*)&BinaryReader__Reading_read_from, "library/binary_reader.spite\tBinaryReader", "read_from", 13},
{(const void*)&spite_failed_3, "-\t-", "spite_failed_3", 0},
{(const void*)&BinaryReader__Reading_read_input, "library/binary_reader.spite\tBinaryReader", "read_input", 20},
{(const void*)&List_Console_Printable_get_at, "library/list.spite\tList", "get_at", 41},
{(const void*)&List_Console_Printable_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&TypedMemory__Console_Printable_read_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "read_value", 1},
{(const void*)&BinaryFormat__Integer_write, "library/binary_format.spite\tBinaryFormat", "write", 5},
{(const void*)&BinaryFormat__Float_write, "library/binary_format.spite\tBinaryFormat", "write", 5},
{(const void*)&BinaryFormat__String_write, "library/binary_format.spite\tBinaryFormat", "write", 5},
{(const void*)&BinaryFormat__Reading_read, "library/binary_format.spite\tBinaryFormat", "read", 110},
{(const void*)&BinaryFormat__Reading_read_attributes, "library/binary_format.spite\tBinaryFormat", "read_attributes", 234},
{(const void*)&BinaryFormat__Reading_read_sensor, "library/binary_format.spite\tBinaryFormat", "read_sensor", 0},
{(const void*)&BinaryFormat__Reading_read_level, "library/binary_format.spite\tBinaryFormat", "read_level", 0},
{(const void*)&BinaryFormat__Reading_read_label, "library/binary_format.spite\tBinaryFormat", "read_label", 0},
{(const void*)&List_Console_Printable_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&TypedMemory__Console_Printable_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&BinaryFormat__Integer_read, "library/binary_format.spite\tBinaryFormat", "read", 110},
{(const void*)&BinaryFormat__Float_read, "library/binary_format.spite\tBinaryFormat", "read", 110},
{(const void*)&BinaryFormat__String_read, "library/binary_format.spite\tBinaryFormat", "read", 110},
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
