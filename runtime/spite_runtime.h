/* Spite compiler runtime prelude, pasted at the top of every generated C
 * file. Self-contained: no
 * function here calls a monomorphized List<T>/Dictionary<T>/Nullable<T>
 * function, so this whole block can be emitted before any of those are
 * generated. Nothing here is `static`, matching the rest of the generated
 * output (an unused `static` function is flagged by `-Wunused-function`
 * under `-Wall`; an unused ordinary one is not). */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
/* Excludes <winsock.h> from <windows.h> so a REPL-mode build (spite_repl.h,
 * included after this file) can safely include <winsock2.h> itself without
 * the classic "winsock.h already included" conflict. */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <fcntl.h>
#include <io.h>
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#endif

/* ---- allocation counters (--debug-memory) ---- */

#ifdef SPITE_DEBUG_MEMORY
static int64_t spite_debug_allocations = 0;
static int64_t spite_debug_frees = 0;

/* A simple growable table of every pointer this program currently owns
 * (B4 in PLAN.md): consulted by spite_debug_free/spite_debug_realloc to
 * catch a double free or a free/realloc of a pointer this allocator never
 * handed out (or already took back). Either one means an ownership mistake
 * slipped past the compiler's move/drop checks, so this aborts immediately
 * with a clear message instead of silently corrupting the heap -- the
 * whole point of running under --debug-memory. A linear scan per
 * free/realloc is fine here: this exists for tests and debugging, not for
 * a program's normal, optimized build.
 *
 * Each record also keeps the pointer's requested size, so the REPL's
 * `memory` command (milestone 6a) can report live bytes, not just a live
 * count -- purely additive: `spite_debug_allocations`/`spite_debug_frees`
 * and their end-of-program printf are unchanged. */
typedef struct {
    void* pointer;
    int64_t size;
} SpiteDebugRecord;
static SpiteDebugRecord* spite_debug_live_records = 0;
static int64_t spite_debug_live_count = 0;
static int64_t spite_debug_live_capacity = 0;
static int64_t spite_debug_live_bytes = 0;

static void spite_debug_track(void* pointer, int64_t size) {
    if (pointer == 0) return;
    if (spite_debug_live_count == spite_debug_live_capacity) {
        int64_t new_capacity = spite_debug_live_capacity == 0 ? 64 : spite_debug_live_capacity * 2;
        spite_debug_live_records = (SpiteDebugRecord*)realloc(spite_debug_live_records, (size_t)new_capacity * sizeof(SpiteDebugRecord));
        spite_debug_live_capacity = new_capacity;
    }
    spite_debug_live_records[spite_debug_live_count].pointer = pointer;
    spite_debug_live_records[spite_debug_live_count].size = size;
    spite_debug_live_count = spite_debug_live_count + 1;
    spite_debug_live_bytes = spite_debug_live_bytes + size;
}

/* Removes `pointer` from the live table if present, reporting whether it
 * was found there at all. */
static bool spite_debug_untrack(void* pointer) {
    for (int64_t index = 0; index < spite_debug_live_count; index = index + 1) {
        if (spite_debug_live_records[index].pointer == pointer) {
            spite_debug_live_bytes = spite_debug_live_bytes - spite_debug_live_records[index].size;
            spite_debug_live_records[index] = spite_debug_live_records[spite_debug_live_count - 1];
            spite_debug_live_count = spite_debug_live_count - 1;
            return true;
        }
    }
    return false;
}

/* Checks whether `pointer` is still live, without removing it -- used only
 * by `spite_debug_report_leaks`' by-class summary at the very end of the
 * program. */
static bool spite_debug_is_live(void* pointer) {
    for (int64_t index = 0; index < spite_debug_live_count; index = index + 1) {
        if (spite_debug_live_records[index].pointer == pointer) return true;
    }
    return false;
}

static void* spite_debug_realloc(void* pointer, size_t size) {
    if (pointer == 0) {
        spite_debug_allocations = spite_debug_allocations + 1;
        void* result = realloc(pointer, size);
        spite_debug_track(result, (int64_t)size);
        return result;
    }
    if (!spite_debug_untrack(pointer)) {
        fprintf(stderr, "spite: --debug-memory: reallocating a pointer that was already freed (or never allocated) -- an ownership bug\n");
        abort();
    }
    void* result = realloc(pointer, size);
    spite_debug_track(result, (int64_t)size);
    return result;
}

static void spite_debug_free(void* pointer) {
    if (pointer == 0) return;
    if (!spite_debug_untrack(pointer)) {
        fprintf(stderr, "spite: --debug-memory: double free (or freeing an unknown pointer) -- an ownership bug\n");
        abort();
    }
    spite_debug_frees = spite_debug_frees + 1;
    free(pointer);
}
#define SPITE_REALLOC(pointer, size) spite_debug_realloc(pointer, size)
#define SPITE_FREE(pointer) spite_debug_free(pointer)
#define SPITE_MALLOC(size) spite_debug_realloc(0, size)
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
 * Passing, assigning, storing and returning share the same object -- a copy
 * is only ever made by an explicit `copy()`/`deep_copy()` call (manual.md
 * section 10).
 *
 * Each concrete type gets its own small, readable, generated
 * `{Type}_retain`/`{Type}_release` pair; this
 * header only holds what is shared: the header layout itself, and (under
 * `--debug-memory`) a class-id -> name table used to report which classes'
 * objects leaked when allocations and frees do not balance. */

typedef struct SpiteHeader {
    int32_t ref_count;
    int32_t class_id;
} SpiteHeader;

#ifdef SPITE_DEBUG_MEMORY
/* A side table mapping every heap object's pointer to its class id, kept
 * only so `spite_debug_report_leaks` (called once, right before `main`
 * returns, when allocations and frees do not balance) can print how many
 * still-live objects belong to each class. Populated by every generated
 * `{Type}_retain`-adjacent "new object" constructor via
 * `spite_debug_register_object`; never removed (this table only exists for
 * one end-of-program report, so there is nothing to reclaim). */
typedef struct {
    void* pointer;
    int32_t class_id;
} SpiteDebugClassRecord;
static SpiteDebugClassRecord* spite_debug_class_records = 0;
static int64_t spite_debug_class_record_count = 0;
static int64_t spite_debug_class_record_capacity = 0;
static const char* const* spite_debug_class_names = 0;
static int64_t spite_debug_class_name_count = 0;

static void spite_debug_set_class_names(const char* const* names, int64_t count) {
    spite_debug_class_names = names;
    spite_debug_class_name_count = count;
}

static void spite_debug_register_object(void* pointer, int32_t class_id) {
    if (pointer == 0) return;
    if (spite_debug_class_record_count == spite_debug_class_record_capacity) {
        int64_t new_capacity = spite_debug_class_record_capacity == 0 ? 64 : spite_debug_class_record_capacity * 2;
        spite_debug_class_records = (SpiteDebugClassRecord*)realloc(spite_debug_class_records, (size_t)new_capacity * sizeof(SpiteDebugClassRecord));
        spite_debug_class_record_capacity = new_capacity;
    }
    spite_debug_class_records[spite_debug_class_record_count].pointer = pointer;
    spite_debug_class_records[spite_debug_class_record_count].class_id = class_id;
    spite_debug_class_record_count = spite_debug_class_record_count + 1;
}

/* Called once, right before `main` returns, only when allocations and frees
 * do not balance: names which classes' objects are still live. A raw buffer
 * (a List<T>'s items array, a String's byte buffer, ...) has no class id of
 * its own and is not attributed to any class here -- only whole heap
 * objects (registered via `spite_debug_register_object`) are, which is
 * still enough to point at the leaking class in the common case (an
 * un-dropped cycle keeps its own object alive, which is what leaks it). */
static void spite_debug_report_leaks(void) {
    if (spite_debug_allocations == spite_debug_frees) return;
    fprintf(stderr, "spite: --debug-memory: %lld allocation(s) leaked\n", (long long)(spite_debug_allocations - spite_debug_frees));
    if (spite_debug_class_names == 0) return;
    for (int64_t class_index = 0; class_index < spite_debug_class_name_count; class_index = class_index + 1) {
        int64_t leaked_count = 0;
        for (int64_t record_index = 0; record_index < spite_debug_class_record_count; record_index = record_index + 1) {
            if (spite_debug_class_records[record_index].class_id != (int32_t)class_index) continue;
            if (spite_debug_is_live(spite_debug_class_records[record_index].pointer)) leaked_count = leaked_count + 1;
        }
        if (leaked_count > 0) fprintf(stderr, "  %s: %lld leaked object(s)\n", spite_debug_class_names[class_index], (long long)leaked_count);
    }
}
#endif

/* Every generated `_release` calls this exactly once, right before freeing a
 * heap object whose reference count reached zero -- kept here (rather than
 * only relying on `SPITE_FREE`'s own live-pointer table) so `spite_debug_free`
 * still catches a double free the same way it always has. */

/* ---- SpiteString: an immutable, reference-counted byte buffer with a
 * length. A literal is compiled into a `static` C global (never allocated,
 * never freed -- see codegen's `renderStringValue`) with `is_static = true`;
 * every other SpiteString is a heap object, retained on every store and
 * released at scope exit/overwrite/container removal, freed the moment its
 * count reaches zero. Sharing a String's buffer between two references is
 * unobservable (Strings are immutable), which is exactly why reference
 * counting it is sound and `copy()` never needs to do anything special for
 * one. */

typedef struct SpiteString {
    SpiteHeader header;
    const char* data;
    int64_t length;
    bool is_static;
} SpiteString;

/* Wraps a compile-time-constant C string as a `static`-storage, never-freed
 * SpiteString: used only to build the `static SpiteString spite_lit_N = ...;`
 * globals codegen emits for every String literal (see `renderStringValue`),
 * and for a handful of runtime constants (`""`, `"true"`, ...) declared with
 * `static` storage the same way. */
#define SPITE_STATIC_STRING(text, len) { { 0, -1 }, (text), (len), true }

SpiteString* spite_string_from_bytes(const char* bytes, int64_t length) {
    SpiteString* value = (SpiteString*)SPITE_MALLOC(sizeof(SpiteString));
    char* buffer = (char*)SPITE_MALLOC((size_t)length + 1);
    if (length > 0) memcpy(buffer, bytes, (size_t)length);
    buffer[length] = '\0';
    value->header.ref_count = 1;
    value->header.class_id = 0; /* String is always --debug-memory class id 0 */
    value->data = buffer;
    value->length = length;
    value->is_static = false;
#ifdef SPITE_DEBUG_MEMORY
    spite_debug_register_object(value, 0);
#endif
    return value;
}

SpiteString* spite_string_from_cstring_owned(const char* text) {
    return spite_string_from_bytes(text, (int64_t)strlen(text));
}

/* Takes ownership of an already-allocated, NUL-terminated buffer. */
SpiteString* spite_string_take(char* buffer, int64_t length) {
    SpiteString* value = (SpiteString*)SPITE_MALLOC(sizeof(SpiteString));
    value->header.ref_count = 1;
    value->header.class_id = 0; /* String is always --debug-memory class id 0 */
    value->data = buffer;
    value->length = length;
    value->is_static = false;
#ifdef SPITE_DEBUG_MEMORY
    spite_debug_register_object(value, 0);
#endif
    return value;
}

SpiteString* spite_string_from_cstring_static(const char* text) {
    return spite_string_from_cstring_owned(text);
}

SpiteString* SpiteString_retain(SpiteString* self) {
    if (self != 0 && !self->is_static) self->header.ref_count = self->header.ref_count + 1;
    return self;
}

void SpiteString_release(SpiteString* self) {
    if (self == 0 || self->is_static) return;
    self->header.ref_count = self->header.ref_count - 1;
    if (self->header.ref_count <= 0) {
        if (self->data != 0) SPITE_FREE((void*)self->data);
        SPITE_FREE(self);
    }
}

SpiteString* SpiteString_concat(SpiteString* left, SpiteString* right) {
    int64_t total = left->length + right->length;
    char* buffer = (char*)SPITE_MALLOC((size_t)total + 1);
    if (left->length > 0) memcpy(buffer, left->data, (size_t)left->length);
    if (right->length > 0) memcpy(buffer + left->length, right->data, (size_t)right->length);
    buffer[total] = '\0';
    return spite_string_take(buffer, total);
}

bool SpiteString_equals(SpiteString* a, SpiteString* b) {
    if (a == b) return true;
    if (a->length != b->length) return false;
    if (a->length == 0) return true;
    return memcmp(a->data, b->data, (size_t)a->length) == 0;
}

bool SpiteString_less(SpiteString* a, SpiteString* b) {
    int64_t shortest = a->length < b->length ? a->length : b->length;
    int comparison = shortest > 0 ? memcmp(a->data, b->data, (size_t)shortest) : 0;
    if (comparison != 0) return comparison < 0;
    return a->length < b->length;
}

bool SpiteString_greater(SpiteString* a, SpiteString* b) {
    return SpiteString_less(b, a);
}

int64_t SpiteString_length(SpiteString* self) {
    return self->length;
}

bool SpiteString_is_empty(SpiteString* self) {
    return self->length == 0;
}

SpiteString* SpiteString_slice(SpiteString* self, int64_t start, int64_t end) {
    int64_t clamped_start = start < 0 ? 0 : (start > self->length ? self->length : start);
    int64_t clamped_end = end < 0 ? 0 : (end > self->length ? self->length : end);
    if (clamped_end <= clamped_start) return spite_string_from_bytes("", 0);
    return spite_string_from_bytes(self->data + clamped_start, clamped_end - clamped_start);
}

SpiteString* SpiteString_character_at(SpiteString* self, int64_t index) {
    if (index < 0 || index >= self->length) return spite_string_from_bytes("", 0);
    return spite_string_from_bytes(self->data + index, 1);
}

int64_t SpiteString_code_at(SpiteString* self, int64_t index) {
    if (index < 0 || index >= self->length) return 0;
    return (int64_t)(unsigned char)self->data[index];
}

int64_t SpiteString_index_of(SpiteString* self, SpiteString* text) {
    if (text->length == 0) return 0;
    if (text->length > self->length) return -1;
    int64_t last_start = self->length - text->length;
    for (int64_t start = 0; start <= last_start; start = start + 1) {
        if (memcmp(self->data + start, text->data, (size_t)text->length) == 0) return start;
    }
    return -1;
}

bool SpiteString_contains(SpiteString* self, SpiteString* text) {
    return SpiteString_index_of(self, text) >= 0;
}

bool SpiteString_starts_with(SpiteString* self, SpiteString* text) {
    if (text->length > self->length) return false;
    if (text->length == 0) return true;
    return memcmp(self->data, text->data, (size_t)text->length) == 0;
}

bool SpiteString_ends_with(SpiteString* self, SpiteString* text) {
    if (text->length > self->length) return false;
    if (text->length == 0) return true;
    return memcmp(self->data + (self->length - text->length), text->data, (size_t)text->length) == 0;
}

/* Replaces every occurrence of `from` with `to` (a no-op copy when `from` is
 * empty, since an empty needle has no unambiguous replacement positions). */
SpiteString* SpiteString_replace(SpiteString* self, SpiteString* from, SpiteString* to) {
    if (from->length == 0) return spite_string_from_bytes(self->data, self->length);
    int64_t capacity = self->length + 1;
    char* buffer = (char*)SPITE_MALLOC((size_t)capacity);
    int64_t used = 0;
    int64_t index = 0;
    while (index < self->length) {
        if (index + from->length <= self->length && memcmp(self->data + index, from->data, (size_t)from->length) == 0) {
            int64_t needed = used + to->length + 1;
            if (needed > capacity) {
                capacity = needed * 2;
                buffer = (char*)SPITE_REALLOC(buffer, (size_t)capacity);
            }
            if (to->length > 0) memcpy(buffer + used, to->data, (size_t)to->length);
            used = used + to->length;
            index = index + from->length;
        } else {
            int64_t needed = used + 2;
            if (needed > capacity) {
                capacity = needed * 2;
                buffer = (char*)SPITE_REALLOC(buffer, (size_t)capacity);
            }
            buffer[used] = self->data[index];
            used = used + 1;
            index = index + 1;
        }
    }
    buffer[used] = '\0';
    return spite_string_take(buffer, used);
}

SpiteString* SpiteString_trim(SpiteString* self) {
    int64_t start = 0;
    int64_t end = self->length;
    while (start < end && isspace((unsigned char)self->data[start])) start = start + 1;
    while (end > start && isspace((unsigned char)self->data[end - 1])) end = end - 1;
    return spite_string_from_bytes(self->data + start, end - start);
}

SpiteString* SpiteString_upper(SpiteString* self) {
    char* buffer = (char*)SPITE_MALLOC((size_t)self->length + 1);
    for (int64_t index = 0; index < self->length; index = index + 1) {
        buffer[index] = (char)toupper((unsigned char)self->data[index]);
    }
    buffer[self->length] = '\0';
    return spite_string_take(buffer, self->length);
}

SpiteString* SpiteString_lower(SpiteString* self) {
    char* buffer = (char*)SPITE_MALLOC((size_t)self->length + 1);
    for (int64_t index = 0; index < self->length; index = index + 1) {
        buffer[index] = (char)tolower((unsigned char)self->data[index]);
    }
    buffer[self->length] = '\0';
    return spite_string_take(buffer, self->length);
}

int64_t SpiteString_to_int(SpiteString* self) {
    if (self->length == 0) return 0;
    char* end = 0;
    long long value = strtoll(self->data, &end, 10);
    if (end == self->data) return 0;
    return (int64_t)value;
}

double SpiteString_to_float(SpiteString* self) {
    if (self->length == 0) return 0.0;
    char* end = 0;
    double value = strtod(self->data, &end);
    if (end == self->data) return 0.0;
    return value;
}

SpiteString* spite_string_from_int(int64_t value) {
    char temporary[32];
    int written = snprintf(temporary, sizeof(temporary), "%lld", (long long)value);
    return spite_string_from_bytes(temporary, (int64_t)written);
}

/* `Byte`/`UnsignedShort`/`UnsignedInt`/`UnsignedLong` format unsigned, so a
 * large value never reads as negative the way a plain `int64_t` cast would. */
SpiteString* spite_string_from_unsigned(uint64_t value) {
    char temporary[32];
    int written = snprintf(temporary, sizeof(temporary), "%llu", (unsigned long long)value);
    return spite_string_from_bytes(temporary, (int64_t)written);
}

static bool spite_text_has_exponent(const char* text) {
    for (const char* cursor = text; *cursor != '\0'; cursor = cursor + 1) {
        if (*cursor == 'e' || *cursor == 'E') return true;
    }
    return false;
}

/* Shortest-round-trip formatting (manual.md's numeric types section,
 * "(proposed by Claude, unconfirmed)"): tries increasing precision until
 * parsing the formatted text back gives the exact same bits, so `Float`
 * being 32-bit does not make an ordinary value like 0.1 print as
 * "0.100000001" -- the shortest decimal that round-trips it is just "0.1",
 * exactly like it printed when `Float` was 64-bit. A low-precision `%g`
 * candidate that happens to round-trip only in scientific notation (e.g.
 * "1e+01" for 10) is skipped in favor of the fixed-notation candidate that
 * appears once precision exceeds the value's exponent -- scientific
 * notation is only actually used, as a last resort, for a magnitude that
 * never gets a fixed-notation candidate within the precision that fully
 * round-trips it. */
SpiteString* spite_string_from_float32(float value) {
    char temporary[64];
    int last_written = 0;
    for (int precision = 1; precision <= 9; precision = precision + 1) {
        last_written = snprintf(temporary, sizeof(temporary), "%.*g", precision, (double)value);
        if (spite_text_has_exponent(temporary)) continue;
        if (strtof(temporary, 0) == value) return spite_string_from_bytes(temporary, (int64_t)last_written);
    }
    last_written = snprintf(temporary, sizeof(temporary), "%.9g", (double)value);
    return spite_string_from_bytes(temporary, (int64_t)last_written);
}

SpiteString* spite_string_from_float64(double value) {
    char temporary[64];
    int last_written = 0;
    for (int precision = 1; precision <= 17; precision = precision + 1) {
        last_written = snprintf(temporary, sizeof(temporary), "%.*g", precision, value);
        if (spite_text_has_exponent(temporary)) continue;
        if (strtod(temporary, 0) == value) return spite_string_from_bytes(temporary, (int64_t)last_written);
    }
    last_written = snprintf(temporary, sizeof(temporary), "%.17g", value);
    return spite_string_from_bytes(temporary, (int64_t)last_written);
}

static SpiteString spite_static_string_true = SPITE_STATIC_STRING("true", 4);
static SpiteString spite_static_string_false = SPITE_STATIC_STRING("false", 5);
static SpiteString spite_static_string_empty = SPITE_STATIC_STRING("", 0);

SpiteString* spite_string_from_bool(bool value) {
    return value ? &spite_static_string_true : &spite_static_string_false;
}

/* Reads one line from stdin (without the trailing newline), growing a
 * malloc'd buffer as needed. `*ok` is false only at end of file with
 * nothing read at all. */
SpiteString* spite_console_read_line(bool* ok) {
    int64_t capacity = 128;
    int64_t used = 0;
    char* buffer = (char*)SPITE_MALLOC((size_t)capacity);
    int character = fgetc(stdin);
    if (character == EOF) {
        SPITE_FREE(buffer);
        *ok = false;
        return &spite_static_string_empty;
    }
    while (character != EOF && character != '\n') {
        if (used + 1 >= capacity) {
            capacity = capacity * 2;
            buffer = (char*)SPITE_REALLOC(buffer, (size_t)capacity);
        }
        buffer[used] = (char)character;
        used = used + 1;
        character = fgetc(stdin);
    }
    if (used > 0 && buffer[used - 1] == '\r') used = used - 1;
    buffer[used] = '\0';
    *ok = true;
    return spite_string_take(buffer, used);
}

/* ---- File/Directory/Process support: portable-C leaf helpers used by the
 * File/Directory/Process/Program built-in classes (runtime/system_bodies.h).
 * Directory listing and process spawning
 * branch on `_WIN32`; everything else is plain stdio/stdlib. */

FILE* spite_file_open(SpiteString* path, const char* mode) {
    return fopen(path->data, mode);
}

bool spite_file_exists(SpiteString* path) {
    FILE* handle = fopen(path->data, "rb");
    if (handle == 0) return false;
    fclose(handle);
    return true;
}

bool spite_file_remove(SpiteString* path) {
    return remove(path->data) == 0;
}

/* Reads the whole file into a fresh owned buffer; `*ok` reports whether the
 * file could be opened at all. */
SpiteString* spite_file_read_all(SpiteString* path, bool* ok) {
    FILE* handle = fopen(path->data, "rb");
    if (handle == 0) {
        *ok = false;
        return &spite_static_string_empty;
    }
    fseek(handle, 0, SEEK_END);
    long size = ftell(handle);
    fseek(handle, 0, SEEK_SET);
    if (size < 0) size = 0;
    char* buffer = (char*)SPITE_MALLOC((size_t)size + 1);
    size_t read_count = size > 0 ? fread(buffer, 1, (size_t)size, handle) : 0;
    buffer[read_count] = '\0';
    fclose(handle);
    *ok = true;
    return spite_string_take(buffer, (int64_t)read_count);
}

bool spite_file_write_all(SpiteString* path, SpiteString* text, const char* mode) {
    FILE* handle = fopen(path->data, mode);
    if (handle == 0) return false;
    size_t written = text->length > 0 ? fwrite(text->data, 1, (size_t)text->length, handle) : 0;
    fclose(handle);
    return written == (size_t)text->length;
}

bool spite_directory_exists(SpiteString* path) {
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(path->data);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat info;
    return stat(path->data, &info) == 0 && S_ISDIR(info.st_mode);
#endif
}

bool spite_directory_create(SpiteString* path) {
#ifdef _WIN32
    return _mkdir(path->data) == 0 || spite_directory_exists(path);
#else
    return mkdir(path->data, 0777) == 0 || spite_directory_exists(path);
#endif
}

/* Comparator for qsort: sorts the C-string entry names alphabetically, so
 * `Directory.files()`/`.folders()` return a stable, sorted `List<String>`. */
int spite_compare_cstring_pointer(const void* left, const void* right) {
    const char* const* left_text = (const char* const*)left;
    const char* const* right_text = (const char* const*)right;
    return strcmp(*left_text, *right_text);
}

/* Collects the immediate entries of `path` whose kind (file/directory)
 * matches `want_directories`, sorted alphabetically. `*count` receives how
 * many entries were found; the caller owns the returned array of owned,
 * NUL-terminated, malloc'd C strings (and the array itself). */
char** spite_directory_list(SpiteString* path, bool want_directories, int64_t* count) {
    char** names = 0;
    int64_t capacity = 0;
    int64_t used = 0;
#ifdef _WIN32
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*", path->data);
    WIN32_FIND_DATAA entry;
    HANDLE handle = FindFirstFileA(pattern, &entry);
    if (handle != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(entry.cFileName, ".") == 0 || strcmp(entry.cFileName, "..") == 0) continue;
            bool is_directory = (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            if (is_directory != want_directories) continue;
            if (used == capacity) {
                capacity = capacity == 0 ? 8 : capacity * 2;
                names = (char**)SPITE_REALLOC(names, (size_t)capacity * sizeof(char*));
            }
            size_t length = strlen(entry.cFileName);
            char* copy = (char*)SPITE_MALLOC(length + 1);
            memcpy(copy, entry.cFileName, length + 1);
            names[used] = copy;
            used = used + 1;
        } while (FindNextFileA(handle, &entry));
        FindClose(handle);
    }
#else
    DIR* directory = opendir(path->data);
    if (directory != 0) {
        struct dirent* entry;
        while ((entry = readdir(directory)) != 0) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", path->data, entry->d_name);
            struct stat info;
            if (stat(full_path, &info) != 0) continue;
            bool is_directory = S_ISDIR(info.st_mode);
            if (is_directory != want_directories) continue;
            if (used == capacity) {
                capacity = capacity == 0 ? 8 : capacity * 2;
                names = (char**)SPITE_REALLOC(names, (size_t)capacity * sizeof(char*));
            }
            size_t length = strlen(entry->d_name);
            char* copy = (char*)SPITE_MALLOC(length + 1);
            memcpy(copy, entry->d_name, length + 1);
            names[used] = copy;
            used = used + 1;
        }
        closedir(directory);
    }
#endif
    if (used > 1) qsort(names, (size_t)used, sizeof(char*), spite_compare_cstring_pointer);
    *count = used;
    return names;
}

/* `Program().sleep(milliseconds)` (milestone 6a, for a `$serve` loop that
 * ticks on an interval instead of spinning): pauses the calling thread. */
void spite_sleep_milliseconds(int64_t milliseconds) {
    if (milliseconds <= 0) return;
#ifdef _WIN32
    Sleep((DWORD)milliseconds);
#else
    struct timespec duration;
    duration.tv_sec = milliseconds / 1000;
    duration.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&duration, 0);
#endif
}

/* Runs `command` (already containing its arguments, shell-quoted) and
 * captures stdout+stderr merged. `*exit_code` receives the process's exit
 * status. `_popen`/`popen` is the portable, minimal way to spawn a process
 * and read its output on both Windows and POSIX. */
SpiteString* spite_process_run(const char* command, int* exit_code) {
#ifdef _WIN32
    FILE* pipe = _popen(command, "r");
#else
    FILE* pipe = popen(command, "r");
#endif
    if (pipe == 0) {
        *exit_code = -1;
        return &spite_static_string_empty;
    }
    int64_t capacity = 4096;
    int64_t used = 0;
    char* buffer = (char*)SPITE_MALLOC((size_t)capacity);
    size_t read_count;
    char chunk[4096];
    while ((read_count = fread(chunk, 1, sizeof(chunk), pipe)) > 0) {
        if (used + (int64_t)read_count + 1 > capacity) {
            capacity = (used + (int64_t)read_count + 1) * 2;
            buffer = (char*)SPITE_REALLOC(buffer, (size_t)capacity);
        }
        memcpy(buffer + used, chunk, read_count);
        used = used + (int64_t)read_count;
    }
    buffer[used] = '\0';
#ifdef _WIN32
    int status = _pclose(pipe);
#else
    int status = pclose(pipe);
#endif
    *exit_code = status;
    return spite_string_take(buffer, used);
}
