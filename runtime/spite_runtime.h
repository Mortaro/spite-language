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

SpiteString* SpiteString_concat(SpiteString* left, SpiteString* right);
bool SpiteString_equals(SpiteString* left, SpiteString* right);
bool SpiteString_less(SpiteString* left, SpiteString* right);
bool SpiteString_greater(SpiteString* left, SpiteString* right);
SpiteString* SpiteString_slice(SpiteString* text, int64_t start, int64_t end);
int64_t SpiteString_length(SpiteString* self) {
    return self->length;
}

int64_t SpiteString_code_at(SpiteString* self, int64_t index) {
    if (index < 0 || index >= self->length) return 0;
    return (int64_t)(unsigned char)self->data[index];
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
