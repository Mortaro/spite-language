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
typedef struct Console Console;
typedef struct Dictionary Dictionary;
typedef struct DynamicLibrary DynamicLibrary;
typedef struct List List;
typedef struct Memory_Arena Memory_Arena;
typedef struct Memory_Heap Memory_Heap;
typedef struct Naive Naive;
typedef struct Arena Arena;
typedef struct Hero Hero;
typedef struct Monster Monster;
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
static SpiteString spite_lit_1 = SPITE_STATIC_STRING("", 0);
typedef void* Spite_Allocator;
struct Launcher {
SpiteHeader header;
Build* build_;
};
typedef struct List_Console_Printable List_Console_Printable;
typedef struct TypedMemory__Console_Printable TypedMemory__Console_Printable;
typedef struct SpiteBox_SpiteLong { SpiteHeader header; int64_t value; } SpiteBox_SpiteLong;
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
struct Console {
SpiteHeader header;
Memory_Heap* heap_;
DynamicLibrary* library_;
int64_t input_;
};
static SpiteString spite_lit_2 = SPITE_STATIC_STRING("\n", 1);
static SpiteString spite_lit_3 = SPITE_STATIC_STRING(" ", 1);
#define spite_site_1() "library/console.spite:56 in Console._write_values"

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
static SpiteString spite_lit_4 = SPITE_STATIC_STRING("0", 1);
#define spite_site_2() "library/long.spite:17 in Long.to_string"
#define spite_site_3() "library/long.spite:18 in Long.to_string"
#define spite_site_4() "library/long.spite:22 in Long.to_string"
#define spite_site_5() "library/long.spite:26 in Long.to_string"
#define SpiteLong_to_unsigned_integer(self) ((uint32_t)(self))
#define SpiteLong_to_unsigned_long(self) ((uint64_t)(self))
#define SpiteShort_to_integer(self) ((int32_t)(self))
#define SpiteShort_to_long(self) ((int64_t)(self))
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
#define spite_site_6() "library/memory/arena.spite:12 in Memory.Arena.allocate"
#define spite_site_7() "library/memory/arena.spite:13 in Memory.Arena.allocate"
#define spite_site_8() "library/memory/arena.spite:17 in Memory.Arena.allocate"
#define spite_site_9() "library/memory/arena.spite:25 in Memory.Arena.start_block"
#define spite_site_10() "library/memory/arena.spite:26 in Memory.Arena.start_block"
struct Memory_Heap {
SpiteHeader header;
};
struct Naive {
SpiteHeader header;
Console* console_;
};
typedef struct List_Monster List_Monster;
typedef struct TypedMemory__Monster TypedMemory__Monster;
typedef struct List_Hero List_Hero;
typedef struct TypedMemory__Hero TypedMemory__Hero;
static SpiteBox_SpiteString spite_lit_5_box = { { 0, -1 }, SPITE_STATIC_STRING("rounds", 6) };
typedef struct SpiteBox_SpiteInteger { SpiteHeader header; int32_t value; } SpiteBox_SpiteInteger;
static SpiteBox_SpiteString spite_lit_6_box = { { 0, -1 }, SPITE_STATIC_STRING("alive", 5) };
struct Arena {
SpiteHeader header;
};
#define spite_site_11() "benchmarks/a_reload_compiles_only_the_classes_that_changed/naive/arena.spite:5 in Arena.fight"
#define spite_site_12() "benchmarks/a_reload_compiles_only_the_classes_that_changed/naive/arena.spite:13 in Arena.strike_all"
#define spite_site_13() "spite.crash\t16190ad8"
struct Hero {
SpiteHeader header;
int32_t strength_;
};
#define spite_site_14() "benchmarks/a_reload_compiles_only_the_classes_that_changed/naive/hero.spite:8 in Hero.damage"
struct Monster {
SpiteHeader header;
int32_t health_;
};
#define spite_site_15() "benchmarks/a_reload_compiles_only_the_classes_that_changed/naive/monster.spite:8 in Monster.hurt"
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
struct List_Monster {
SpiteHeader header;
Memory_Heap* heap_;
TypedMemory__Monster* values_;
int64_t items_;
int32_t item_count_;
int32_t capacity_;
};
struct TypedMemory__Monster {
SpiteHeader header;
};
struct List_Hero {
SpiteHeader header;
Memory_Heap* heap_;
TypedMemory__Hero* values_;
int64_t items_;
int32_t item_count_;
int32_t capacity_;
};
struct TypedMemory__Hero {
SpiteHeader header;
};
#define SpiteByte_to_unsigned_integer(self) ((uint32_t)(self))
#define SpiteDouble_to_float(self) ((float)(self))
#define SpiteInteger_to_short(self) ((int16_t)(self))
#define SpiteLong_to_double(self) ((double)(self))
#define spite_site_16() "library/list.spite:20 in List.append"
#define spite_site_17() "library/list.spite:856 in List._grow"
#define spite_site_18() "library/list.spite:861 in List._grow"
static SpiteString spite_symbol_1 = { (int64_t)0x797469746e656469ULL, (int64_t)0x0700000000000000ULL };
Memory_Heap* spite_singleton_Memory_Heap(void);
Console_Printable Console_Printable___retain(Console_Printable self);
void Console_Printable___release(Console_Printable self);
Build* spite_singleton_Build(void);
Console* spite_singleton_Console(void);
DynamicLibrary* spite_foreign_library_1(void);
void Launcher___init(Launcher* self);
Launcher* Launcher___allocate(void);
static inline void Launcher___release(Launcher* self);
void Launcher___free(Launcher* self);
void Launcher_Launcher(Launcher* self);
static void spite_overflowed(const char* operation, const char* type, const char* symbol, int64_t left, int64_t right, const char* where) __attribute__((noreturn, cold));
TypedMemory__Console_Printable* spite_singleton_TypedMemory__Console_Printable(void);
static List_Console_Printable* List_Console_Printable___framed(List_Console_Printable* self, int64_t items, int32_t count);
void spite_string_box_release(void* self);
static void spite_narrowed(int64_t value, const char* from, const char* to, const char* where) __attribute__((noreturn, cold));
static void spite_outside_list(const char* read, const char* where) __attribute__((noreturn, cold));
void Build___release(Build* self);
void Console___init(Console* self);
Console* Console___allocate(void);
Console* Console___make(void);
void Console___release(Console* self);
void Console_print(Console* self, List_Console_Printable* values_);
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
TypedMemory__Monster* spite_singleton_TypedMemory__Monster(void);
TypedMemory__Hero* spite_singleton_TypedMemory__Hero(void);
static Arena* Arena___framed(Arena* self);
static Arena* Arena___make_into(Arena* self);
int32_t Arena_fight___held_0_1(Arena* self, List_Monster* monsters_, List_Hero* heroes_);
int32_t List_Monster_count_is_alive(List_Monster* self);
static inline SpiteTagged spite_tagged_SpiteInteger(int32_t value);
void Arena___init(Arena* self);
int32_t Arena_fight___held_0_1(Arena* self, List_Monster* monsters_, List_Hero* heroes_);
bool List_Monster_any_is_alive(List_Monster* self);
void Arena_strike_all___held_0_1(Arena* self, List_Monster* monsters_, List_Hero* heroes_);
static SPITE_CRASH_REPORT void spite_failed_1(int32_t index_);
void Hero___init(Hero* self);
Hero* Hero___allocate(void);
Hero* Hero___make(int32_t starting_strength_);
static inline Hero* Hero___retain(Hero* self);
static inline void Hero___release(Hero* self);
void Hero___free(Hero* self);
void Hero_Hero(Hero* self, int32_t starting_strength_);
int32_t Hero_damage(Hero* self);
void Monster___init(Monster* self);
Monster* Monster___allocate(void);
Monster* Monster___make(int32_t starting_health_);
static inline Monster* Monster___retain(Monster* self);
static inline void Monster___release(Monster* self);
void Monster___free(Monster* self);
void Monster_Monster(Monster* self, int32_t starting_health_);
void Monster_hurt(Monster* self, int32_t amount_);
bool Monster_is_alive(Monster* self);
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
void List_Monster___init(List_Monster* self);
List_Monster* List_Monster___allocate(void);
List_Monster* List_Monster___make(void);
static inline void List_Monster___release(List_Monster* self);
void List_Monster___free(List_Monster* self);
void List_Monster_drop(List_Monster* self);
int32_t List_Monster_count(List_Monster* self);
void List_Monster_append(List_Monster* self, Monster* value_);
void List_Monster_clear(List_Monster* self);
void List_Monster_drop(List_Monster* self);
void List_Monster_make_room(List_Monster* self);
void List_Monster__grow(List_Monster* self);
int64_t List_Monster__resized(List_Monster* self, int64_t old_bytes_, int64_t new_bytes_);
int32_t List_Monster_count_is_alive(List_Monster* self);
bool List_Monster_any_is_alive(List_Monster* self);
void TypedMemory__Monster___release(TypedMemory__Monster* self);
void TypedMemory__Monster_write_value(TypedMemory__Monster* self, int64_t address_, int32_t index_, Monster* value_);
void TypedMemory__Monster_release_value(TypedMemory__Monster* self, int64_t address_, int32_t index_);
int64_t TypedMemory__Monster_value_bytes(TypedMemory__Monster* self);
void List_Hero___init(List_Hero* self);
List_Hero* List_Hero___allocate(void);
List_Hero* List_Hero___make(void);
static inline void List_Hero___release(List_Hero* self);
void List_Hero___free(List_Hero* self);
void List_Hero_drop(List_Hero* self);
void List_Hero_append(List_Hero* self, Hero* value_);
void List_Hero_clear(List_Hero* self);
void List_Hero_drop(List_Hero* self);
void List_Hero_make_room(List_Hero* self);
void List_Hero__grow(List_Hero* self);
int64_t List_Hero__resized(List_Hero* self, int64_t old_bytes_, int64_t new_bytes_);
void TypedMemory__Hero___release(TypedMemory__Hero* self);
void TypedMemory__Hero_write_value(TypedMemory__Hero* self, int64_t address_, int32_t index_, Hero* value_);
void TypedMemory__Hero_release_value(TypedMemory__Hero* self, int64_t address_, int32_t index_);
int64_t TypedMemory__Hero_value_bytes(TypedMemory__Hero* self);
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
#define SPITE_ALLOCATOR_List_Monster(object, heap) ((void)(object), ((Spite_Allocator)heap()))
#define SPITE_ALLOCATOR_List_Hero(object, heap) ((void)(object), ((Spite_Allocator)heap()))
static __typeof__(&TypedMemory__Console_Printable___release) spite_folded_TypedMemory__Console_Printable___release = ((__typeof__(&TypedMemory__Console_Printable___release))&Memory_Heap___release);
static __typeof__(&TypedMemory__Monster___release) spite_folded_TypedMemory__Monster___release = ((__typeof__(&TypedMemory__Monster___release))&Memory_Heap___release);
static __typeof__(&TypedMemory__Hero___release) spite_folded_TypedMemory__Hero___release = ((__typeof__(&TypedMemory__Hero___release))&Memory_Heap___release);
static __typeof__(&List_Monster_count) spite_folded_List_Monster_count = ((__typeof__(&List_Monster_count))&List_Console_Printable_count);
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
static Hero* Hero___pool_free = 0;
static char* Hero___pool_next = 0;
static char* Hero___pool_end = 0;
static size_t Hero___pool_count = 0;
static void Hero___pool_grow(void) {
if (Hero___pool_count == 0) { Hero___pool_count = 16; } else if (Hero___pool_count * sizeof(Hero) < 262144) { Hero___pool_count = Hero___pool_count * 2; }
char* chunk = (char*)SPITE_MALLOC(Hero___pool_count * sizeof(Hero) + 63);
if (chunk == 0) { fflush(stdout); fputs("spite: out of memory making an object\n", stderr); exit(1); }
Hero___pool_next = (char*)(((uintptr_t)chunk + 63) & ~(uintptr_t)63);
Hero___pool_end = Hero___pool_next + Hero___pool_count * sizeof(Hero);
}
static inline Hero* Hero___pool_take(void) {
Hero* self = Hero___pool_free;
if (self != 0) { Hero___pool_free = *(Hero**)self; return self; }
if (Hero___pool_next == Hero___pool_end) Hero___pool_grow();
self = (Hero*)Hero___pool_next;
Hero___pool_next = Hero___pool_next + sizeof(Hero);
return self;
}
static inline void Hero___pool_give(Hero* self) {
*(Hero**)self = Hero___pool_free;
Hero___pool_free = self;
}
static Monster* Monster___pool_free = 0;
static char* Monster___pool_next = 0;
static char* Monster___pool_end = 0;
static size_t Monster___pool_count = 0;
static void Monster___pool_grow(void) {
if (Monster___pool_count == 0) { Monster___pool_count = 16; } else if (Monster___pool_count * sizeof(Monster) < 262144) { Monster___pool_count = Monster___pool_count * 2; }
char* chunk = (char*)SPITE_MALLOC(Monster___pool_count * sizeof(Monster) + 63);
if (chunk == 0) { fflush(stdout); fputs("spite: out of memory making an object\n", stderr); exit(1); }
Monster___pool_next = (char*)(((uintptr_t)chunk + 63) & ~(uintptr_t)63);
Monster___pool_end = Monster___pool_next + Monster___pool_count * sizeof(Monster);
}
static inline Monster* Monster___pool_take(void) {
Monster* self = Monster___pool_free;
if (self != 0) { Monster___pool_free = *(Monster**)self; return self; }
if (Monster___pool_next == Monster___pool_end) Monster___pool_grow();
self = (Monster*)Monster___pool_next;
Monster___pool_next = Monster___pool_next + sizeof(Monster);
return self;
}
static inline void Monster___pool_give(Monster* self) {
*(Monster**)self = Monster___pool_free;
Monster___pool_free = self;
}
Memory_Heap* spite_singleton_Memory_Heap(void) {
static Memory_Heap spite_object = { { 1, 96 } };
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
static void spite_outside_list(const char* read, const char* where) {
fflush(stdout);
fprintf(stderr, "spite: '%s' is outside its list: a bound proves only the top of an index, and this one is below 0 or the list changed, at %s\n", read, where);
exit(1);
}
void Build___release(Build* self) { (void)self; }
void Console___init(Console* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->library_ = spite_foreign_library_1();
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
void Memory_Heap___release(Memory_Heap* self) { (void)self; }
void Naive___init(Naive* self) {
self->console_ = spite_singleton_Console();
}
Naive* Naive___allocate(void) {
Naive* self = Naive___pool_take();
self->header.ref_count = 1;
self->header.class_id = 111;
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
#ifdef SPITE_TRACKS_Naive
spite_untrack_Naive(self);
#endif
#ifdef SPITE_WEAK_Naive
spite_weak_object_freed(self);
#endif
Naive___pool_give(self);
}
TypedMemory__Monster* spite_singleton_TypedMemory__Monster(void) {
static TypedMemory__Monster spite_object = { { 1, 171 } };
return &spite_object;
}
TypedMemory__Hero* spite_singleton_TypedMemory__Hero(void) {
static TypedMemory__Hero spite_object = { { 1, 173 } };
return &spite_object;
}
static Arena* Arena___framed(Arena* self) {
self->header.ref_count = SPITE_FRAMED_COUNT;
self->header.class_id = 112;
return self;
}
static Arena* Arena___make_into(Arena* self) {
Arena___framed(self);
Arena___init(self);
return self;
}
static inline SpiteTagged spite_tagged_SpiteInteger(int32_t value) {
SpiteTagged tagged;
tagged.tag = 174;
tagged.plain = 1;
tagged.value.bits = 0;
memcpy(&tagged.value, &value, sizeof(value));
return tagged;
}
void Arena___init(Arena* self) {
}
void Hero___init(Hero* self) {
self->strength_ = 0;
}
Hero* Hero___allocate(void) {
Hero* self = Hero___pool_take();
self->header.ref_count = 1;
self->header.class_id = 113;
Hero___init(self);
#ifdef SPITE_TRACKS_Hero
spite_track_Hero(self);
#endif
return self;
}
Hero* Hero___make(int32_t starting_strength_) {
Hero* self = Hero___allocate();
Hero_Hero(self, starting_strength_);
return self;
}
static inline Hero* Hero___retain(Hero* self) {
if (self != 0) SPITE_COUNT_UP(self->header.ref_count);
return self;
}
static inline void Hero___release(Hero* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Hero___free(self);
}
void Hero___free(Hero* self) {
#ifdef SPITE_TRACKS_Hero
spite_untrack_Hero(self);
#endif
#ifdef SPITE_WEAK_Hero
spite_weak_object_freed(self);
#endif
Hero___pool_give(self);
}
void Monster___init(Monster* self) {
self->health_ = 0;
}
Monster* Monster___allocate(void) {
Monster* self = Monster___pool_take();
self->header.ref_count = 1;
self->header.class_id = 114;
Monster___init(self);
#ifdef SPITE_TRACKS_Monster
spite_track_Monster(self);
#endif
return self;
}
Monster* Monster___make(int32_t starting_health_) {
Monster* self = Monster___allocate();
Monster_Monster(self, starting_health_);
return self;
}
static inline Monster* Monster___retain(Monster* self) {
if (self != 0) SPITE_COUNT_UP(self->header.ref_count);
return self;
}
static inline void Monster___release(Monster* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Monster___free(self);
}
void Monster___free(Monster* self) {
#ifdef SPITE_TRACKS_Monster
spite_untrack_Monster(self);
#endif
#ifdef SPITE_WEAK_Monster
spite_weak_object_freed(self);
#endif
Monster___pool_give(self);
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
void List_Monster___init(List_Monster* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->values_ = spite_singleton_TypedMemory__Monster();
self->items_ = ((int64_t)(0));
self->item_count_ = 0;
self->capacity_ = 0;
}
List_Monster* List_Monster___allocate(void) {
List_Monster* self = (List_Monster*)SPITE_MALLOC(sizeof(List_Monster));
self->header.ref_count = 1;
self->header.class_id = 170;
List_Monster___init(self);
#ifdef SPITE_TRACKS_List_Monster
spite_track_List_Monster(self);
#endif
return self;
}
List_Monster* List_Monster___make(void) {
List_Monster* self = List_Monster___allocate();
return self;
}
static inline void List_Monster___release(List_Monster* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
List_Monster___free(self);
}
void List_Monster___free(List_Monster* self) {
List_Monster_drop(self);
Memory_Heap___release(self->heap_);
spite_folded_TypedMemory__Monster___release(self->values_);
#ifdef SPITE_TRACKS_List_Monster
spite_untrack_List_Monster(self);
#endif
#ifdef SPITE_WEAK_List_Monster
spite_weak_object_freed(self);
#endif
SPITE_FREE(self);
}
void List_Hero___init(List_Hero* self) {
self->heap_ = spite_singleton_Memory_Heap();
self->values_ = spite_singleton_TypedMemory__Hero();
self->items_ = ((int64_t)(0));
self->item_count_ = 0;
self->capacity_ = 0;
}
List_Hero* List_Hero___allocate(void) {
List_Hero* self = (List_Hero*)SPITE_MALLOC(sizeof(List_Hero));
self->header.ref_count = 1;
self->header.class_id = 172;
List_Hero___init(self);
#ifdef SPITE_TRACKS_List_Hero
spite_track_List_Hero(self);
#endif
return self;
}
List_Hero* List_Hero___make(void) {
List_Hero* self = List_Hero___allocate();
return self;
}
static inline void List_Hero___release(List_Hero* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
List_Hero___free(self);
}
void List_Hero___free(List_Hero* self) {
List_Hero_drop(self);
Memory_Heap___release(self->heap_);
spite_folded_TypedMemory__Hero___release(self->values_);
#ifdef SPITE_TRACKS_List_Hero
spite_untrack_List_Hero(self);
#endif
#ifdef SPITE_WEAK_List_Hero
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
if ((self).tag == 155) return SpiteLong_to_string(SPITE_TAGGED_VALUE(self, int64_t));
if ((self).tag == 174) return SpiteInteger_to_string(SPITE_TAGGED_VALUE(self, int32_t));
fputs("spite.crash\tPrintable.to_string was called on a value of a class it was not compiled for\n", stderr);
abort();
}
static int32_t spite_foreign_library_1_lock = 0;
DynamicLibrary* spite_foreign_library_1(void) {
DynamicLibrary* found = SPITE_SINGLETON_FOUND(spite_foreign_library_1_cache);
if (found != 0) return found;
SPITE_LOCK(spite_foreign_library_1_lock);
if (spite_foreign_library_1_cache == 0) {
DynamicLibrary* made = DynamicLibrary___make(((SpiteString)SPITE_STATIC_STRING("ucrtbase.dll", 12)), spite_symbol_1, ((SpiteString)SPITE_STATIC_STRING("", 0)));
spite_foreign_library_1_tracked = spite_singleton_tracked();
(void)&DynamicLibrary_find_symbol;















SPITE_SINGLETON_PUBLISH(spite_foreign_library_1_cache, made);
}
SPITE_UNLOCK(spite_foreign_library_1_lock);
return spite_foreign_library_1_cache;
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
void Console_print(Console* self, List_Console_Printable* values_) {
Console__write_values(self, List_Console_Printable___retain(values_), Console_Stream_output);
Console__write_output(self, spite_lit_2);
Console__flush(self);
List_Console_Printable___release(values_);
}
void Console__write_values(Console* self, List_Console_Printable* values_, Console_Stream stream_) {
int32_t index_ = 0;
while (((index_ < List_Console_Printable_count(values_)))) {
if (((index_ > 0))) {
Console__write_to(self, spite_lit_3, stream_);
}
SpiteString text_ = ({ Console_Printable spite_temp_1 = ({ Console_Printable spite_temp_2 = List_Console_Printable_get_at(values_, index_); if (__builtin_expect(!(SPITE_TAGGED_PRESENT(spite_temp_2)), 0)) spite_outside_list("values[index]", spite_site_1()); spite_temp_2; }); SpiteString spite_temp_3 = Console_Printable___call_to_string(spite_temp_1); Console_Printable___release(spite_temp_1); spite_temp_3; });
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
SpiteString spite_temp_4 = SpiteString___retain(file_);
SpiteString___release(self->file_name_);
self->file_name_ = spite_temp_4;
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
SpiteString spite_temp_5 = SpiteLong_to_string(wide_);
return spite_temp_5;
}
SpiteString SpiteLong_to_string(int64_t self) {
if (((self == SpiteInteger_to_long(0)))) {
SpiteString spite_temp_6 = spite_lit_4;
return spite_temp_6;
}
Memory_Heap* heap_ = spite_singleton_Memory_Heap();
int64_t buffer_bytes_ = SpiteInteger_to_long(24);
int64_t spite_temp_7[32];
int64_t spite_temp_8 = buffer_bytes_;
int64_t address_ = spite_temp_8 <= 256 ? (int64_t)(intptr_t)spite_temp_7 : Memory_Heap_allocate(heap_, spite_temp_8);
int64_t position_ = buffer_bytes_;
int64_t rest_ = self;
while (((rest_ != SpiteInteger_to_long(0)))) {
int64_t digit_ = (rest_ % SpiteInteger_to_long(10));
if (((digit_ < SpiteInteger_to_long(0)))) {
digit_ = (-(digit_));
}
position_ = ({ int64_t spite_temp_9 = position_; int64_t spite_temp_10 = SpiteInteger_to_long(1); int64_t spite_temp_11; if (__builtin_expect(__builtin_sub_overflow(spite_temp_9, spite_temp_10, &spite_temp_11), 0)) spite_overflowed("position - 1", "a Long", "-", (int64_t)spite_temp_9, (int64_t)spite_temp_10, spite_site_2()); spite_temp_11; });
SpiteMemory_Address_write_byte(address_, position_, ({ int64_t spite_temp_12 = (digit_ + SpiteInteger_to_long(48)); if (__builtin_expect(spite_temp_12 < 0 || spite_temp_12 > UINT8_MAX, 0)) spite_narrowed((int64_t)spite_temp_12, "a Long", "a Byte", spite_site_3()); (uint8_t)spite_temp_12; }));
rest_ = (rest_ / SpiteInteger_to_long(10));
}
if (((self < SpiteInteger_to_long(0)))) {
position_ = ({ int64_t spite_temp_13 = position_; int64_t spite_temp_14 = SpiteInteger_to_long(1); int64_t spite_temp_15; if (__builtin_expect(__builtin_sub_overflow(spite_temp_13, spite_temp_14, &spite_temp_15), 0)) spite_overflowed("position - 1", "a Long", "-", (int64_t)spite_temp_13, (int64_t)spite_temp_14, spite_site_4()); spite_temp_15; });
SpiteMemory_Address_write_byte(address_, position_, SpiteInteger_to_byte(45));
}
int64_t first_digit_ = (address_ + ((int64_t)(position_)));
SpiteString text_ = SpiteMemory_Address_text(first_digit_, ({ int64_t spite_temp_16 = buffer_bytes_; int64_t spite_temp_17 = position_; int64_t spite_temp_18; if (__builtin_expect(__builtin_sub_overflow(spite_temp_16, spite_temp_17, &spite_temp_18), 0)) spite_overflowed("buffer_bytes - position", "a Long", "-", (int64_t)spite_temp_16, (int64_t)spite_temp_17, spite_site_5()); spite_temp_18; }));
if (address_ != (int64_t)(intptr_t)spite_temp_7) Memory_Heap_free(heap_, address_);
SpiteString spite_temp_19 = SpiteString___retain(text_);
SpiteString___release(text_);
Memory_Heap___release(heap_);
return spite_temp_19;
}
SpiteString SpiteString_to_string(SpiteString self) {
SpiteString spite_temp_20 = SpiteString___retain(self);
return spite_temp_20;
}
SpiteString SpiteMemory_Address_text(int64_t self, int64_t length_) {
return spite_string_from_bytes((const char*)(intptr_t)self, length_);
}
int64_t Memory_Arena_allocate(Memory_Arena* self, int64_t bytes_) {
int64_t rounded_ = ({ int64_t spite_temp_21 = (({ int64_t spite_temp_22 = bytes_; int64_t spite_temp_23 = SpiteInteger_to_long(15); int64_t spite_temp_24; if (__builtin_expect(__builtin_add_overflow(spite_temp_22, spite_temp_23, &spite_temp_24), 0)) spite_overflowed("bytes + 15", "a Long", "+", (int64_t)spite_temp_22, (int64_t)spite_temp_23, spite_site_6()); spite_temp_24; }) / SpiteInteger_to_long(16)); int64_t spite_temp_25 = SpiteInteger_to_long(16); int64_t spite_temp_26; if (__builtin_expect(__builtin_mul_overflow(spite_temp_21, spite_temp_25, &spite_temp_26), 0)) spite_overflowed("(bytes + 15) / 16 * 16", "a Long", "*", (int64_t)spite_temp_21, (int64_t)spite_temp_25, spite_site_6()); spite_temp_26; });
if (((((self->_block_ == ((int64_t)(0)))) || ((({ int64_t spite_temp_27 = self->_used_; int64_t spite_temp_28 = rounded_; int64_t spite_temp_29; if (__builtin_expect(__builtin_add_overflow(spite_temp_27, spite_temp_28, &spite_temp_29), 0)) spite_overflowed("_used + rounded", "a Long", "+", (int64_t)spite_temp_27, (int64_t)spite_temp_28, spite_site_7()); spite_temp_29; }) > self->_end_))))) {
Memory_Arena_start_block(self, rounded_);
}
int64_t address_ = (self->_block_ + ((int64_t)(self->_used_)));
self->_used_ = ({ int64_t spite_temp_30 = self->_used_; int64_t spite_temp_31 = rounded_; int64_t spite_temp_32; if (__builtin_expect(__builtin_add_overflow(spite_temp_30, spite_temp_31, &spite_temp_32), 0)) spite_overflowed("_used + rounded", "a Long", "+", (int64_t)spite_temp_30, (int64_t)spite_temp_31, spite_site_8()); spite_temp_32; });
int64_t spite_temp_33 = address_;
return spite_temp_33;
}
void Memory_Arena_free(Memory_Arena* self, int64_t _address_) {
}
void Memory_Arena_start_block(Memory_Arena* self, int64_t at_least_) {
int64_t size_ = self->_block_bytes_;
if (((({ int64_t spite_temp_34 = at_least_; int64_t spite_temp_35 = SpiteInteger_to_long(16); int64_t spite_temp_36; if (__builtin_expect(__builtin_add_overflow(spite_temp_34, spite_temp_35, &spite_temp_36), 0)) spite_overflowed("at_least + 16", "a Long", "+", (int64_t)spite_temp_34, (int64_t)spite_temp_35, spite_site_9()); spite_temp_36; }) > size_))) {
size_ = ({ int64_t spite_temp_37 = at_least_; int64_t spite_temp_38 = SpiteInteger_to_long(16); int64_t spite_temp_39; if (__builtin_expect(__builtin_add_overflow(spite_temp_37, spite_temp_38, &spite_temp_39), 0)) spite_overflowed("at_least + 16", "a Long", "+", (int64_t)spite_temp_37, (int64_t)spite_temp_38, spite_site_10()); spite_temp_39; });
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
List_Monster* monsters_ = List_Monster___make();
List_Hero* heroes_ = List_Hero___make();
int32_t index_ = 0;
while (((index_ < 100))) {
Monster* monster_ = Monster___make((index_ % 40));
List_Monster_append(monsters_, Monster___retain(monster_));
Hero* hero_ = Hero___make((index_ % 7));
List_Hero_append(heroes_, Hero___retain(hero_));
index_ = (index_ + 1);
Hero___release(hero_);
Monster___release(monster_);
}
Arena spite_slot_1;
Arena* arena_ = Arena___make_into(&spite_slot_1);
int32_t rounds_ = Arena_fight___held_0_1(arena_, monsters_, heroes_);
int32_t alive_ = List_Monster_count_is_alive(monsters_);
List_Console_Printable spite_framed_1; Console_Printable spite_framed_1_items[4]; int32_t spite_framed_1_count = 0;
Console_print(self->console_, ({ spite_framed_1_items[0] = spite_tagged_object(0, ((void*)&spite_lit_5_box)); spite_framed_1_items[1] = spite_tagged_SpiteInteger(rounds_); spite_framed_1_items[2] = spite_tagged_object(0, ((void*)&spite_lit_6_box)); spite_framed_1_items[3] = spite_tagged_SpiteInteger(alive_); spite_framed_1_count = 4; List_Console_Printable___framed(&spite_framed_1, (int64_t)(intptr_t)spite_framed_1_items, 4); }));
for (int32_t spite_index = 0; spite_index < spite_framed_1_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_1_items[spite_index]); }
List_Hero___release(heroes_);
List_Monster___release(monsters_);
}
int32_t Arena_fight___held_0_1(Arena* self, List_Monster* monsters_, List_Hero* heroes_) {
int32_t rounds_ = 0;
while ((List_Monster_any_is_alive(monsters_))) {
Arena_strike_all___held_0_1(self, monsters_, heroes_);
rounds_ = ({ int32_t spite_temp_40 = rounds_; int32_t spite_temp_41 = 1; int32_t spite_temp_42; if (__builtin_expect(__builtin_add_overflow(spite_temp_40, spite_temp_41, &spite_temp_42), 0)) spite_overflowed("rounds + 1", "an Integer", "+", (int64_t)spite_temp_40, (int64_t)spite_temp_41, spite_site_11()); spite_temp_42; });
}
int32_t spite_temp_43 = rounds_;
return spite_temp_43;
}
void Arena_strike_all___held_0_1(Arena* self, List_Monster* monsters_, List_Hero* heroes_) {
int32_t index_ = 0;
while (((index_ < spite_folded_List_Monster_count(monsters_)))) {
Monster* monster_ = ({ List_Monster* spite_temp_44 = monsters_; int32_t spite_temp_45 = index_; if (__builtin_expect(spite_temp_45 < 0 || spite_temp_45 >= (spite_temp_44)->item_count_, 0)) spite_outside_list("monsters[index]", spite_site_12()); ((Monster**)(intptr_t)(spite_temp_44)->items_)[spite_temp_45]; });
Hero* hero_ = ({ List_Hero* spite_temp_46 = heroes_; int32_t spite_temp_47 = index_; (spite_temp_47 < 0 || spite_temp_47 >= (spite_temp_46)->item_count_) ? 0 : ((Hero**)(intptr_t)(spite_temp_46)->items_)[spite_temp_47]; });
if (!(((hero_) != 0))) {
spite_failed_1(index_);
}
int32_t amount_ = Hero_damage(hero_);
Monster_hurt(monster_, amount_);
index_ = (index_ + 1);
}
}
static SPITE_CRASH_REPORT void spite_failed_1(int32_t index_) {
spite_crash_begin();
fflush(stdout);
fputs(spite_site_13(), stderr);
fputs("\thero is null", stderr);
fputs("\tindex=", stderr);
{ SpiteString spite_temp_48 = SpiteInteger_to_string(index_); spite_crash_text(spite_string_bytes(&spite_temp_48), spite_string_length(spite_temp_48)); SpiteString___release(spite_temp_48); }
fputs("\n", stderr);
spite_report_assert_trace();
exit(1);
}
void Hero_Hero(Hero* self, int32_t starting_strength_) {
self->strength_ = starting_strength_;
}
int32_t Hero_damage(Hero* self) {
int32_t spite_temp_49 = ({ int32_t spite_temp_50 = self->strength_; int32_t spite_temp_51 = 1; int32_t spite_temp_52; if (__builtin_expect(__builtin_add_overflow(spite_temp_50, spite_temp_51, &spite_temp_52), 0)) spite_overflowed("strength + 1", "an Integer", "+", (int64_t)spite_temp_50, (int64_t)spite_temp_51, spite_site_14()); spite_temp_52; });
return spite_temp_49;
}
void Monster_Monster(Monster* self, int32_t starting_health_) {
self->health_ = starting_health_;
}
void Monster_hurt(Monster* self, int32_t amount_) {
self->health_ = ({ int32_t spite_temp_53 = self->health_; int32_t spite_temp_54 = amount_; int32_t spite_temp_55; if (__builtin_expect(__builtin_sub_overflow(spite_temp_53, spite_temp_54, &spite_temp_55), 0)) spite_overflowed("health - amount", "an Integer", "-", (int64_t)spite_temp_53, (int64_t)spite_temp_54, spite_site_15()); spite_temp_55; });
}
bool Monster_is_alive(Monster* self) {
bool spite_temp_56 = (self->health_ > 0);
return spite_temp_56;
}
int32_t List_Console_Printable_count(List_Console_Printable* self) {
int32_t spite_temp_57 = self->item_count_;
return spite_temp_57;
}
Console_Printable List_Console_Printable_get_at(List_Console_Printable* self, int32_t index_) {
if ((((index_ >= 0))) && (((index_ < self->item_count_)))) {
Console_Printable spite_temp_58 = TypedMemory__Console_Printable_read_value(self->values_, self->items_, index_);
return spite_temp_58;
}
Console_Printable spite_temp_59 = SPITE_TAGGED_NULL;
return spite_temp_59;
}
void List_Console_Printable_drop(List_Console_Printable* self) {
List_Console_Printable_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_60 = SPITE_ALLOCATOR_List_Console_Printable(self, spite_singleton_Memory_Heap); int64_t spite_temp_61 = self->items_; if (((SpiteHeader*)(spite_temp_60))->class_id == 95) { Memory_Arena_free(((Memory_Arena*)spite_temp_60), spite_temp_61); } else if (((SpiteHeader*)(spite_temp_60))->class_id == 96) { Memory_Heap_free(((Memory_Heap*)spite_temp_60), spite_temp_61); } });
}
}
Console_Printable TypedMemory__Console_Printable_read_value(TypedMemory__Console_Printable* self, int64_t address_, int32_t index_) {
return Console_Printable___retain(((Console_Printable*)(intptr_t)address_)[index_]);
}
void List_Monster_append(List_Monster* self, Monster* value_) {
List_Monster_make_room(self);
TypedMemory__Monster_write_value(self->values_, self->items_, self->item_count_, Monster___retain(value_));
self->item_count_ = ({ int32_t spite_temp_62 = self->item_count_; int32_t spite_temp_63 = 1; int32_t spite_temp_64; if (__builtin_expect(__builtin_add_overflow(spite_temp_62, spite_temp_63, &spite_temp_64), 0)) spite_overflowed("item_count + 1", "an Integer", "+", (int64_t)spite_temp_62, (int64_t)spite_temp_63, spite_site_16()); spite_temp_64; });
Monster___release(value_);
}
void List_Monster_drop(List_Monster* self) {
List_Monster_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_65 = SPITE_ALLOCATOR_List_Monster(self, spite_singleton_Memory_Heap); int64_t spite_temp_66 = self->items_; if (((SpiteHeader*)(spite_temp_65))->class_id == 95) { Memory_Arena_free(((Memory_Arena*)spite_temp_65), spite_temp_66); } else if (((SpiteHeader*)(spite_temp_65))->class_id == 96) { Memory_Heap_free(((Memory_Heap*)spite_temp_65), spite_temp_66); } });
}
}
void List_Monster_make_room(List_Monster* self) {
if (((self->item_count_ == self->capacity_))) {
List_Monster__grow(self);
}
}
void List_Monster__grow(List_Monster* self) {
int32_t grown_ = ({ int32_t spite_temp_67 = self->capacity_; int32_t spite_temp_68 = 2; int32_t spite_temp_69; if (__builtin_expect(__builtin_mul_overflow(spite_temp_67, spite_temp_68, &spite_temp_69), 0)) spite_overflowed("capacity * 2", "an Integer", "*", (int64_t)spite_temp_67, (int64_t)spite_temp_68, spite_site_17()); spite_temp_69; });
if (((self->capacity_ == 0))) {
grown_ = 4;
}
int64_t bytes_ = TypedMemory__Monster_value_bytes(self->values_);
self->items_ = List_Monster__resized(self, ({ int64_t spite_temp_70 = bytes_; int64_t spite_temp_71 = SpiteInteger_to_long(self->capacity_); int64_t spite_temp_72; if (__builtin_expect(__builtin_mul_overflow(spite_temp_70, spite_temp_71, &spite_temp_72), 0)) spite_overflowed("bytes * capacity", "a Long", "*", (int64_t)spite_temp_70, (int64_t)spite_temp_71, spite_site_18()); spite_temp_72; }), ({ int64_t spite_temp_73 = bytes_; int64_t spite_temp_74 = SpiteInteger_to_long(grown_); int64_t spite_temp_75; if (__builtin_expect(__builtin_mul_overflow(spite_temp_73, spite_temp_74, &spite_temp_75), 0)) spite_overflowed("bytes * grown", "a Long", "*", (int64_t)spite_temp_73, (int64_t)spite_temp_74, spite_site_18()); spite_temp_75; }));
self->capacity_ = grown_;
}
int64_t List_Monster__resized(List_Monster* self, int64_t old_bytes_, int64_t new_bytes_) {
if ((({ Spite_Allocator spite_temp_76 = SPITE_ALLOCATOR_List_Monster(self, spite_singleton_Memory_Heap); bool spite_temp_77 = (((SpiteHeader*)(spite_temp_76))->class_id == 96); spite_temp_77; }))) {
int64_t spite_temp_78 = Memory_Heap_resize(self->heap_, self->items_, new_bytes_);
return spite_temp_78;
}
int64_t moved_ = ({ Spite_Allocator spite_temp_79 = SPITE_ALLOCATOR_List_Monster(self, spite_singleton_Memory_Heap); int64_t spite_temp_80 = new_bytes_; int64_t spite_temp_81 = 0; if (((SpiteHeader*)(spite_temp_79))->class_id == 95) { spite_temp_81 = Memory_Arena_allocate(((Memory_Arena*)spite_temp_79), spite_temp_80); } else if (((SpiteHeader*)(spite_temp_79))->class_id == 96) { spite_temp_81 = Memory_Heap_allocate(((Memory_Heap*)spite_temp_79), spite_temp_80); } spite_temp_81; });
if (((self->items_ != ((int64_t)(0))))) {
SpiteMemory_Address_copy_to(self->items_, moved_, old_bytes_);
({ Spite_Allocator spite_temp_82 = SPITE_ALLOCATOR_List_Monster(self, spite_singleton_Memory_Heap); int64_t spite_temp_83 = self->items_; if (((SpiteHeader*)(spite_temp_82))->class_id == 95) { Memory_Arena_free(((Memory_Arena*)spite_temp_82), spite_temp_83); } else if (((SpiteHeader*)(spite_temp_82))->class_id == 96) { Memory_Heap_free(((Memory_Heap*)spite_temp_82), spite_temp_83); } });
}
int64_t spite_temp_84 = moved_;
return spite_temp_84;
}
int32_t List_Monster_count_is_alive(List_Monster* self) {
int32_t counted_ = 0;
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
Monster* item_ = ((Monster**)(intptr_t)self->items_)[index_];
if ((Monster_is_alive(item_))) {
counted_ = (counted_ + 1);
}
index_ = (index_ + 1);
}
int32_t spite_temp_85 = counted_;
return spite_temp_85;
}
bool List_Monster_any_is_alive(List_Monster* self) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
Monster* item_ = ((Monster**)(intptr_t)self->items_)[index_];
if ((Monster_is_alive(item_))) {
bool spite_temp_86 = true;
return spite_temp_86;
}
index_ = (index_ + 1);
}
bool spite_temp_87 = false;
return spite_temp_87;
}
void TypedMemory__Monster_write_value(TypedMemory__Monster* self, int64_t address_, int32_t index_, Monster* value_) {
((Monster**)(intptr_t)address_)[index_] = value_;
}
int64_t TypedMemory__Monster_value_bytes(TypedMemory__Monster* self) {
return (int64_t)sizeof(Monster*);
}
void List_Hero_append(List_Hero* self, Hero* value_) {
List_Hero_make_room(self);
TypedMemory__Hero_write_value(self->values_, self->items_, self->item_count_, Hero___retain(value_));
self->item_count_ = ({ int32_t spite_temp_88 = self->item_count_; int32_t spite_temp_89 = 1; int32_t spite_temp_90; if (__builtin_expect(__builtin_add_overflow(spite_temp_88, spite_temp_89, &spite_temp_90), 0)) spite_overflowed("item_count + 1", "an Integer", "+", (int64_t)spite_temp_88, (int64_t)spite_temp_89, spite_site_16()); spite_temp_90; });
Hero___release(value_);
}
void List_Hero_drop(List_Hero* self) {
List_Hero_clear(self);
if (((self->items_ != ((int64_t)(0))))) {
({ Spite_Allocator spite_temp_91 = SPITE_ALLOCATOR_List_Hero(self, spite_singleton_Memory_Heap); int64_t spite_temp_92 = self->items_; if (((SpiteHeader*)(spite_temp_91))->class_id == 95) { Memory_Arena_free(((Memory_Arena*)spite_temp_91), spite_temp_92); } else if (((SpiteHeader*)(spite_temp_91))->class_id == 96) { Memory_Heap_free(((Memory_Heap*)spite_temp_91), spite_temp_92); } });
}
}
void List_Hero_make_room(List_Hero* self) {
if (((self->item_count_ == self->capacity_))) {
List_Hero__grow(self);
}
}
void List_Hero__grow(List_Hero* self) {
int32_t grown_ = ({ int32_t spite_temp_93 = self->capacity_; int32_t spite_temp_94 = 2; int32_t spite_temp_95; if (__builtin_expect(__builtin_mul_overflow(spite_temp_93, spite_temp_94, &spite_temp_95), 0)) spite_overflowed("capacity * 2", "an Integer", "*", (int64_t)spite_temp_93, (int64_t)spite_temp_94, spite_site_17()); spite_temp_95; });
if (((self->capacity_ == 0))) {
grown_ = 4;
}
int64_t bytes_ = TypedMemory__Hero_value_bytes(self->values_);
self->items_ = List_Hero__resized(self, ({ int64_t spite_temp_96 = bytes_; int64_t spite_temp_97 = SpiteInteger_to_long(self->capacity_); int64_t spite_temp_98; if (__builtin_expect(__builtin_mul_overflow(spite_temp_96, spite_temp_97, &spite_temp_98), 0)) spite_overflowed("bytes * capacity", "a Long", "*", (int64_t)spite_temp_96, (int64_t)spite_temp_97, spite_site_18()); spite_temp_98; }), ({ int64_t spite_temp_99 = bytes_; int64_t spite_temp_100 = SpiteInteger_to_long(grown_); int64_t spite_temp_101; if (__builtin_expect(__builtin_mul_overflow(spite_temp_99, spite_temp_100, &spite_temp_101), 0)) spite_overflowed("bytes * grown", "a Long", "*", (int64_t)spite_temp_99, (int64_t)spite_temp_100, spite_site_18()); spite_temp_101; }));
self->capacity_ = grown_;
}
int64_t List_Hero__resized(List_Hero* self, int64_t old_bytes_, int64_t new_bytes_) {
if ((({ Spite_Allocator spite_temp_102 = SPITE_ALLOCATOR_List_Hero(self, spite_singleton_Memory_Heap); bool spite_temp_103 = (((SpiteHeader*)(spite_temp_102))->class_id == 96); spite_temp_103; }))) {
int64_t spite_temp_104 = Memory_Heap_resize(self->heap_, self->items_, new_bytes_);
return spite_temp_104;
}
int64_t moved_ = ({ Spite_Allocator spite_temp_105 = SPITE_ALLOCATOR_List_Hero(self, spite_singleton_Memory_Heap); int64_t spite_temp_106 = new_bytes_; int64_t spite_temp_107 = 0; if (((SpiteHeader*)(spite_temp_105))->class_id == 95) { spite_temp_107 = Memory_Arena_allocate(((Memory_Arena*)spite_temp_105), spite_temp_106); } else if (((SpiteHeader*)(spite_temp_105))->class_id == 96) { spite_temp_107 = Memory_Heap_allocate(((Memory_Heap*)spite_temp_105), spite_temp_106); } spite_temp_107; });
if (((self->items_ != ((int64_t)(0))))) {
SpiteMemory_Address_copy_to(self->items_, moved_, old_bytes_);
({ Spite_Allocator spite_temp_108 = SPITE_ALLOCATOR_List_Hero(self, spite_singleton_Memory_Heap); int64_t spite_temp_109 = self->items_; if (((SpiteHeader*)(spite_temp_108))->class_id == 95) { Memory_Arena_free(((Memory_Arena*)spite_temp_108), spite_temp_109); } else if (((SpiteHeader*)(spite_temp_108))->class_id == 96) { Memory_Heap_free(((Memory_Heap*)spite_temp_108), spite_temp_109); } });
}
int64_t spite_temp_110 = moved_;
return spite_temp_110;
}
void TypedMemory__Hero_write_value(TypedMemory__Hero* self, int64_t address_, int32_t index_, Hero* value_) {
((Hero**)(intptr_t)address_)[index_] = value_;
}
int64_t TypedMemory__Hero_value_bytes(TypedMemory__Hero* self) {
return (int64_t)sizeof(Hero*);
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
void List_Monster_clear(List_Monster* self) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
TypedMemory__Monster_release_value(self->values_, self->items_, index_);
index_ = (index_ + 1);
}
self->item_count_ = 0;
}
void TypedMemory__Monster_release_value(TypedMemory__Monster* self, int64_t address_, int32_t index_) {
Monster___release(((Monster**)(intptr_t)address_)[index_]);
}
void List_Hero_clear(List_Hero* self) {
int32_t index_ = 0;
while (((index_ < self->item_count_))) {
TypedMemory__Hero_release_value(self->values_, self->items_, index_);
index_ = (index_ + 1);
}
self->item_count_ = 0;
}
void TypedMemory__Hero_release_value(TypedMemory__Hero* self, int64_t address_, int32_t index_) {
Hero___release(((Hero**)(intptr_t)address_)[index_]);
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
{(const void*)&Launcher___pool_grow, "-\t-", "Launcher___pool_grow", 0},
{(const void*)&Launcher___pool_take, "-\t-", "Launcher___pool_take", 0},
{(const void*)&Launcher___pool_give, "-\t-", "Launcher___pool_give", 0},
{(const void*)&Naive___pool_grow, "-\t-", "Naive___pool_grow", 0},
{(const void*)&Naive___pool_take, "-\t-", "Naive___pool_take", 0},
{(const void*)&Naive___pool_give, "-\t-", "Naive___pool_give", 0},
{(const void*)&Hero___pool_grow, "-\t-", "Hero___pool_grow", 0},
{(const void*)&Hero___pool_take, "-\t-", "Hero___pool_take", 0},
{(const void*)&Hero___pool_give, "-\t-", "Hero___pool_give", 0},
{(const void*)&Monster___pool_grow, "-\t-", "Monster___pool_grow", 0},
{(const void*)&Monster___pool_take, "-\t-", "Monster___pool_take", 0},
{(const void*)&Monster___pool_give, "-\t-", "Monster___pool_give", 0},
{(const void*)&spite_singleton_Memory_Heap, "-\t-", "spite_singleton_Memory_Heap", 0},
{(const void*)&Console_Printable___retain, "-\t-", "Console_Printable___retain", 0},
{(const void*)&spite_singleton_Build, "-\t-", "spite_singleton_Build", 0},
{(const void*)&spite_singleton_Console_teardown, "-\t-", "spite_singleton_Console_teardown", 0},
{(const void*)&spite_singleton_Console, "-\t-", "spite_singleton_Console", 0},
{(const void*)&Launcher___init, "-\t-", "Launcher___init", 0},
{(const void*)&Launcher___allocate, "-\t-", "Launcher___allocate", 0},
{(const void*)&Launcher___release, "-\t-", "Launcher___release", 0},
{(const void*)&Launcher___free, "-\t-", "Launcher___free", 0},
{(const void*)&spite_overflowed, "-\t-", "spite_overflowed", 0},
{(const void*)&spite_singleton_TypedMemory__Console_Printable, "-\t-", "spite_singleton_TypedMemory__Console_Printable", 0},
{(const void*)&List_Console_Printable___framed, "-\t-", "List_Console_Printable___framed", 0},
{(const void*)&spite_string_box_release, "-\t-", "spite_string_box_release", 0},
{(const void*)&spite_narrowed, "-\t-", "spite_narrowed", 0},
{(const void*)&spite_outside_list, "-\t-", "spite_outside_list", 0},
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
{(const void*)&spite_string_block, "-\t-", "spite_string_block", 0},
{(const void*)&spite_string_held, "-\t-", "spite_string_held", 0},
{(const void*)&SpiteString___retain, "-\t-", "SpiteString___retain", 0},
{(const void*)&SpiteString___release, "-\t-", "SpiteString___release", 0},
{(const void*)&spite_string_from_bytes, "-\t-", "spite_string_from_bytes", 0},
{(const void*)&Naive___init, "-\t-", "Naive___init", 0},
{(const void*)&Naive___allocate, "-\t-", "Naive___allocate", 0},
{(const void*)&Naive___release, "-\t-", "Naive___release", 0},
{(const void*)&Naive___free, "-\t-", "Naive___free", 0},
{(const void*)&spite_singleton_TypedMemory__Monster, "-\t-", "spite_singleton_TypedMemory__Monster", 0},
{(const void*)&spite_singleton_TypedMemory__Hero, "-\t-", "spite_singleton_TypedMemory__Hero", 0},
{(const void*)&Arena___framed, "-\t-", "Arena___framed", 0},
{(const void*)&Arena___make_into, "-\t-", "Arena___make_into", 0},
{(const void*)&spite_tagged_SpiteInteger, "-\t-", "spite_tagged_SpiteInteger", 0},
{(const void*)&Arena___init, "-\t-", "Arena___init", 0},
{(const void*)&Hero___init, "-\t-", "Hero___init", 0},
{(const void*)&Hero___allocate, "-\t-", "Hero___allocate", 0},
{(const void*)&Hero___make, "-\t-", "Hero___make", 0},
{(const void*)&Hero___retain, "-\t-", "Hero___retain", 0},
{(const void*)&Hero___release, "-\t-", "Hero___release", 0},
{(const void*)&Hero___free, "-\t-", "Hero___free", 0},
{(const void*)&Monster___init, "-\t-", "Monster___init", 0},
{(const void*)&Monster___allocate, "-\t-", "Monster___allocate", 0},
{(const void*)&Monster___make, "-\t-", "Monster___make", 0},
{(const void*)&Monster___retain, "-\t-", "Monster___retain", 0},
{(const void*)&Monster___release, "-\t-", "Monster___release", 0},
{(const void*)&Monster___free, "-\t-", "Monster___free", 0},
{(const void*)&List_Console_Printable___init, "-\t-", "List_Console_Printable___init", 0},
{(const void*)&List_Console_Printable___retain, "-\t-", "List_Console_Printable___retain", 0},
{(const void*)&List_Console_Printable___release, "-\t-", "List_Console_Printable___release", 0},
{(const void*)&List_Console_Printable___free, "-\t-", "List_Console_Printable___free", 0},
{(const void*)&List_Monster___init, "-\t-", "List_Monster___init", 0},
{(const void*)&List_Monster___allocate, "-\t-", "List_Monster___allocate", 0},
{(const void*)&List_Monster___make, "-\t-", "List_Monster___make", 0},
{(const void*)&List_Monster___release, "-\t-", "List_Monster___release", 0},
{(const void*)&List_Monster___free, "-\t-", "List_Monster___free", 0},
{(const void*)&List_Hero___init, "-\t-", "List_Hero___init", 0},
{(const void*)&List_Hero___allocate, "-\t-", "List_Hero___allocate", 0},
{(const void*)&List_Hero___make, "-\t-", "List_Hero___make", 0},
{(const void*)&List_Hero___release, "-\t-", "List_Hero___release", 0},
{(const void*)&List_Hero___free, "-\t-", "List_Hero___free", 0},
{(const void*)&Console_Printable___release, "-\t-", "Console_Printable___release", 0},
{(const void*)&Console_Printable___call_to_string, "-\t-", "Console_Printable___call_to_string", 0},
{(const void*)&spite_foreign_library_1, "-\t-", "spite_foreign_library_1", 0},
{(const void*)&spite_singleton_check_circle, "-\t-", "spite_singleton_check_circle", 0},
{(const void*)&spite_singleton_making, "-\t-", "spite_singleton_making", 0},
{(const void*)&spite_singleton_made, "-\t-", "spite_singleton_made", 0},
{(const void*)&spite_singleton_tracked, "-\t-", "spite_singleton_tracked", 0},
{(const void*)&spite_singleton_created, "-\t-", "spite_singleton_created", 0},
{(const void*)&spite_singleton_used_after_exit, "-\t-", "spite_singleton_used_after_exit", 0},
{(const void*)&spite_singleton_free_later, "-\t-", "spite_singleton_free_later", 0},
{(const void*)&spite_singletons_destroy, "-\t-", "spite_singletons_destroy", 0},
{(const void*)&Launcher_Launcher, "launcher/launcher.spite\tLauncher", "Launcher", 3},
{(const void*)&Console_print, "library/console.spite\tConsole", "print", 18},
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
{(const void*)&Naive_Naive, "benchmarks/a_reload_compiles_only_the_classes_that_changed/naive/naive.spite\tNaive", "Naive", 3},
{(const void*)&Arena_fight___held_0_1, "benchmarks/a_reload_compiles_only_the_classes_that_changed/naive/arena.spite\tArena", "fight", 1},
{(const void*)&Arena_strike_all___held_0_1, "benchmarks/a_reload_compiles_only_the_classes_that_changed/naive/arena.spite\tArena", "strike_all", 10},
{(const void*)&spite_failed_1, "-\t-", "spite_failed_1", 0},
{(const void*)&Hero_Hero, "benchmarks/a_reload_compiles_only_the_classes_that_changed/naive/hero.spite\tHero", "Hero", 3},
{(const void*)&Hero_damage, "benchmarks/a_reload_compiles_only_the_classes_that_changed/naive/hero.spite\tHero", "damage", 7},
{(const void*)&Monster_Monster, "benchmarks/a_reload_compiles_only_the_classes_that_changed/naive/monster.spite\tMonster", "Monster", 3},
{(const void*)&Monster_hurt, "benchmarks/a_reload_compiles_only_the_classes_that_changed/naive/monster.spite\tMonster", "hurt", 7},
{(const void*)&Monster_is_alive, "benchmarks/a_reload_compiles_only_the_classes_that_changed/naive/monster.spite\tMonster", "is_alive", 11},
{(const void*)&List_Console_Printable_count, "library/list.spite\tList", "count", 9},
{(const void*)&List_Console_Printable_get_at, "library/list.spite\tList", "get_at", 41},
{(const void*)&List_Console_Printable_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&TypedMemory__Console_Printable_read_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "read_value", 1},
{(const void*)&List_Monster_append, "library/list.spite\tList", "append", 17},
{(const void*)&List_Monster_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_Monster_make_room, "library/list.spite\tList", "make_room", 849},
{(const void*)&List_Monster__grow, "library/list.spite\tList", "_grow", 855},
{(const void*)&List_Monster__resized, "library/list.spite\tList", "_resized", 865},
{(const void*)&List_Monster_count_is_alive, "library/list.spite\tList", "count_is_alive", 0},
{(const void*)&List_Monster_any_is_alive, "library/list.spite\tList", "any_is_alive", 0},
{(const void*)&TypedMemory__Monster_write_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "write_value", 2},
{(const void*)&TypedMemory__Monster_value_bytes, "bootstrap/source/generation/prelude.spite\tTypedMemory", "value_bytes", 4},
{(const void*)&List_Hero_append, "library/list.spite\tList", "append", 17},
{(const void*)&List_Hero_drop, "library/list.spite\tList", "drop", 830},
{(const void*)&List_Hero_make_room, "library/list.spite\tList", "make_room", 849},
{(const void*)&List_Hero__grow, "library/list.spite\tList", "_grow", 855},
{(const void*)&List_Hero__resized, "library/list.spite\tList", "_resized", 865},
{(const void*)&TypedMemory__Hero_write_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "write_value", 2},
{(const void*)&TypedMemory__Hero_value_bytes, "bootstrap/source/generation/prelude.spite\tTypedMemory", "value_bytes", 4},
{(const void*)&List_Console_Printable_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&TypedMemory__Console_Printable_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&List_Monster_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&TypedMemory__Monster_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&List_Hero_clear, "library/list.spite\tList", "clear", 124},
{(const void*)&TypedMemory__Hero_release_value, "bootstrap/source/generation/prelude.spite\tTypedMemory", "release_value", 3},
{(const void*)&main, "-\t-", "main", 0},
{0, 0, 0, 0}
};
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



if (spite_foreign_library_1_tracked) DynamicLibrary___destroy(spite_foreign_library_1_cache);





return 0;
}
