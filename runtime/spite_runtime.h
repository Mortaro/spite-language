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
#include <dlfcn.h>
#endif

/* ---- allocation counters (--debug-memory) ---- */

/* Under --debug-memory the live table is Spite (library/allocation_table.spite): these are the functions the
 * compiler emits to call it, defined after the classes. The table's own memory comes from the C allocator
 * directly, so tracking never tracks itself. Without --debug-memory an allocation stays one realloc and a
 * counter, which is the floor every Spite object is built on. */
#ifdef SPITE_DEBUG_MEMORY
void* spite_debug_realloc(void* pointer, size_t size);
void spite_debug_free(void* pointer);
void spite_debug_register_object(void* pointer, int32_t class_id);
void spite_debug_set_class_names(const char* const* names, int64_t count);
void spite_debug_report(void);
int64_t spite_live_allocation_count(void);
#define SPITE_REALLOC(pointer, size) spite_debug_realloc(pointer, size)
#define SPITE_FREE(pointer) spite_debug_free(pointer)
#define SPITE_MALLOC(size) spite_debug_realloc(0, size)
#else
static int64_t spite_live_allocations = 0;
static void* spite_counted_realloc(void* pointer, size_t size) {
    void* result = realloc(pointer, size);
    if (pointer == 0 && result != 0) spite_live_allocations = spite_live_allocations + 1;
    return result;
}
static void spite_counted_free(void* pointer) {
    if (pointer == 0) return;
    spite_live_allocations = spite_live_allocations - 1;
    free(pointer);
}
static int64_t spite_live_allocation_count(void) { return spite_live_allocations; }
#define SPITE_REALLOC(pointer, size) spite_counted_realloc(pointer, size)
#define SPITE_FREE(pointer) spite_counted_free(pointer)
#define SPITE_MALLOC(size) spite_counted_realloc(0, size)
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
 * header only holds what is shared: the header layout itself. */

typedef struct SpiteHeader {
    int32_t ref_count;
    int32_t class_id;
} SpiteHeader;


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

SpiteString* SpiteString_slice(SpiteString* self, int64_t start, int64_t end) {
    int64_t clamped_start = start < 0 ? 0 : (start > self->length ? self->length : start);
    int64_t clamped_end = end < 0 ? 0 : (end > self->length ? self->length : end);
    if (clamped_end <= clamped_start) return spite_string_from_bytes("", 0);
    return spite_string_from_bytes(self->data + clamped_start, clamped_end - clamped_start);
}

int64_t SpiteString_code_at(SpiteString* self, int64_t index) {
    if (index < 0 || index >= self->length) return 0;
    return (int64_t)(unsigned char)self->data[index];
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

/* ---- File/Directory/Process support: portable-C leaf helpers used by the
 * File/Directory/Process/Program built-in classes (runtime/system_bodies.h).
 * Directory listing and process spawning
 * branch on `_WIN32`; everything else is plain stdio/stdlib. */
