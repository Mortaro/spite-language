/* Spite REPL runtime, embedded into generated C only in REPL modes
 * (--repl / --repl-port), included right after spite_runtime.h.
 * Self-contained except for
 * the reflection tables themselves (spite_reflect_classes and friends),
 * which are `extern` here and defined by generated code once every class
 * has finished resolving.
 *
 * Design: every piece of program-specific information (which attributes a
 * class has, their offsets, which functions are callable, ...) is plain
 * data emitted by the the driver codegen using the real C `offsetof`/`sizeof`
 * operators. This file is one interpreter, written once, that walks that
 * data at run time -- nothing here is generated per program. */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET SpiteSocketHandle;
#define SPITE_INVALID_SOCKET INVALID_SOCKET
#else
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int SpiteSocketHandle;
#define SPITE_INVALID_SOCKET (-1)
#endif

/* ---- reflection kinds and tables (data only) ---- */

typedef enum {
    SPITE_REFLECT_INT8,
    SPITE_REFLECT_INT16,
    SPITE_REFLECT_INT32,
    SPITE_REFLECT_INT64,
    SPITE_REFLECT_UINT8,
    SPITE_REFLECT_UINT16,
    SPITE_REFLECT_UINT32,
    SPITE_REFLECT_UINT64,
    SPITE_REFLECT_FLOAT32,
    SPITE_REFLECT_FLOAT64,
    SPITE_REFLECT_BOOL,
    SPITE_REFLECT_STRING,
    SPITE_REFLECT_ENUM,
    SPITE_REFLECT_CLASS,
    SPITE_REFLECT_LIST,
    SPITE_REFLECT_DICTIONARY,
    SPITE_REFLECT_NULLABLE,
    SPITE_REFLECT_UNION,
    SPITE_REFLECT_NONE
} SpiteReflectKind;

bool spite_repl_kind_is_numeric(SpiteReflectKind kind) {
    return kind >= SPITE_REFLECT_INT8 && kind <= SPITE_REFLECT_FLOAT64;
}

bool spite_repl_kind_is_literal_assignable(SpiteReflectKind kind) {
    return spite_repl_kind_is_numeric(kind) || kind == SPITE_REFLECT_BOOL || kind == SPITE_REFLECT_STRING || kind == SPITE_REFLECT_ENUM;
}

/* An attribute (field) of a reflected class: `offset` is `offsetof(Class,
 * field)`; `via_pointer` is true whenever the C storage at that offset is
 * itself a pointer to the value rather than the value in place -- every
 * class/List<T>/Dictionary<T>/String attribute, and a Nullable<T> of one of
 * those (D1, milestone 9a: every one of those is a heap object referred to
 * only by pointer -- manual.md section 10) -- false for a still-embedded
 * scalar/enum/union or a Nullable<T> wrapping one of those. */
typedef struct {
    const char* name;
    const char* type_name;
    size_t offset;
    SpiteReflectKind kind;
    int32_t type_index;
    bool via_pointer;
} SpiteReflectAttribute;

/* One request's already-tokenized literal arguments, handed to a function
 * thunk: `tokens[i]` is the raw (unquoted) text of the i-th argument. */
typedef struct {
    const char* const* tokens;
    int32_t count;
} SpiteReplArguments;

/* D1: `String` is a heap object referred to by pointer (`SpiteString*`),
 * like every other reference type -- so is every rendered/thunk result
 * below, always an owned reference the caller drops. */
typedef SpiteString* (*SpiteReflectThunk)(void* self, const SpiteReplArguments* arguments);

typedef struct {
    const char* name;
    int32_t parameter_count;
    const char* const* parameter_type_names;
    const char* return_type_name;
    SpiteReflectThunk thunk;
} SpiteReflectFunction;

typedef struct {
    const char* qualified_name;
    int32_t attribute_count;
    const SpiteReflectAttribute* attributes;
    int32_t function_count;
    const SpiteReflectFunction* functions;
} SpiteReflectClassType;

typedef struct {
    const char* name;
    int32_t value_count;
    const char* const* value_names;
} SpiteReflectEnumType;

typedef struct {
    const char* label;
    SpiteReflectKind element_kind;
    int32_t element_type_index;
    bool element_via_pointer;
    size_t element_size;
    size_t items_offset;
    size_t count_offset;
} SpiteReflectListType;

typedef struct {
    const char* label;
    SpiteReflectKind element_kind;
    int32_t element_type_index;
    bool element_via_pointer;
    size_t element_size;
    size_t keys_offset;
    size_t values_offset;
    size_t count_offset;
} SpiteReflectDictionaryType;

typedef struct {
    const char* label;
    SpiteReflectKind element_kind;
    int32_t element_type_index;
    bool element_via_pointer;
    size_t has_value_offset;
    size_t value_offset;
} SpiteReflectNullableType;

typedef struct {
    const char* name;
    int32_t member_count;
    const int32_t* member_class_indices;
    size_t tag_offset;
    size_t value_offset;
} SpiteReflectUnionType;

extern const SpiteReflectClassType spite_reflect_classes[];
extern const int32_t spite_reflect_class_count;
extern const SpiteReflectEnumType spite_reflect_enums[];
extern const int32_t spite_reflect_enum_count;
extern const SpiteReflectListType spite_reflect_lists[];
extern const int32_t spite_reflect_list_count;
extern const SpiteReflectDictionaryType spite_reflect_dictionaries[];
extern const int32_t spite_reflect_dictionary_count;
extern const SpiteReflectNullableType spite_reflect_nullables[];
extern const int32_t spite_reflect_nullable_count;
extern const SpiteReflectUnionType spite_reflect_unions[];
extern const int32_t spite_reflect_union_count;

/* The entry instance, rooted at the name `program` in every REPL command
 * (manual.md section 13). Set once, right after the entry instance's
 * fields are initialized (before the entry constructor even runs), so a
 * `--repl-port` client connecting mid-construction sees a stable address
 * (documented data race: manual.md section 13's security/threading note). */
void* spite_repl_program_pointer = 0;
int32_t spite_repl_program_class_index = -1;

/* ---- generic scalar read/write/render by kind (no codegen needed) ---- */

int64_t spite_repl_read_int(SpiteReflectKind kind, const void* address) {
    switch (kind) {
        case SPITE_REFLECT_INT8: return (int64_t)*(const int8_t*)address;
        case SPITE_REFLECT_INT16: return (int64_t)*(const int16_t*)address;
        case SPITE_REFLECT_INT32: return (int64_t)*(const int32_t*)address;
        case SPITE_REFLECT_INT64: return *(const int64_t*)address;
        case SPITE_REFLECT_UINT8: return (int64_t)*(const uint8_t*)address;
        case SPITE_REFLECT_UINT16: return (int64_t)*(const uint16_t*)address;
        case SPITE_REFLECT_UINT32: return (int64_t)*(const uint32_t*)address;
        case SPITE_REFLECT_UINT64: return (int64_t)*(const uint64_t*)address;
        case SPITE_REFLECT_ENUM: return (int64_t)*(const int*)address;
        default: return 0;
    }
}

uint64_t spite_repl_read_unsigned(SpiteReflectKind kind, const void* address) {
    switch (kind) {
        case SPITE_REFLECT_UINT8: return (uint64_t)*(const uint8_t*)address;
        case SPITE_REFLECT_UINT16: return (uint64_t)*(const uint16_t*)address;
        case SPITE_REFLECT_UINT32: return (uint64_t)*(const uint32_t*)address;
        case SPITE_REFLECT_UINT64: return *(const uint64_t*)address;
        default: return 0;
    }
}

void spite_repl_write_int(SpiteReflectKind kind, void* address, int64_t value) {
    switch (kind) {
        case SPITE_REFLECT_INT8: *(int8_t*)address = (int8_t)value; return;
        case SPITE_REFLECT_INT16: *(int16_t*)address = (int16_t)value; return;
        case SPITE_REFLECT_INT32: *(int32_t*)address = (int32_t)value; return;
        case SPITE_REFLECT_INT64: *(int64_t*)address = value; return;
        case SPITE_REFLECT_UINT8: *(uint8_t*)address = (uint8_t)value; return;
        case SPITE_REFLECT_UINT16: *(uint16_t*)address = (uint16_t)value; return;
        case SPITE_REFLECT_UINT32: *(uint32_t*)address = (uint32_t)value; return;
        case SPITE_REFLECT_UINT64: *(uint64_t*)address = (uint64_t)value; return;
        case SPITE_REFLECT_ENUM: *(int*)address = (int)value; return;
        default: return;
    }
}

void spite_repl_write_float(SpiteReflectKind kind, void* address, double value) {
    if (kind == SPITE_REFLECT_FLOAT32) *(float*)address = (float)value;
    else if (kind == SPITE_REFLECT_FLOAT64) *(double*)address = value;
}

/* Parses `text` (already unquoted) according to `kind` and writes it at
 * `address`. Enum values are handled separately (spite_repl_write_enum_from_text)
 * since they need the enum's own name table. */
bool spite_repl_write_scalar_from_text(SpiteReflectKind kind, void* address, const char* text) {
    switch (kind) {
        case SPITE_REFLECT_INT8: case SPITE_REFLECT_INT16: case SPITE_REFLECT_INT32: case SPITE_REFLECT_INT64:
            spite_repl_write_int(kind, address, strtoll(text, 0, 10));
            return true;
        case SPITE_REFLECT_UINT8: case SPITE_REFLECT_UINT16: case SPITE_REFLECT_UINT32: case SPITE_REFLECT_UINT64:
            spite_repl_write_int(kind, address, (int64_t)strtoull(text, 0, 10));
            return true;
        case SPITE_REFLECT_FLOAT32: case SPITE_REFLECT_FLOAT64:
            spite_repl_write_float(kind, address, strtod(text, 0));
            return true;
        case SPITE_REFLECT_BOOL:
            *(bool*)address = (strcmp(text, "true") == 0);
            return true;
        case SPITE_REFLECT_STRING: {
            /* D1: a String attribute/element slot always *stores* a
             * `SpiteString*` (it is a heap object like every other
             * reference type now) -- `address` is the slot's own address,
             * so this is a pointer-to-pointer: release whatever it
             * currently points to, then point it at a freshly made
             * String. */
            SpiteString** slot = (SpiteString**)address;
            SpiteString_release(*slot);
            *slot = spite_string_from_cstring_owned(text);
            return true;
        }
        default:
            return false;
    }
}

/* ---- parsing one already-tokenized call argument into a real C value
 * (used by the per-function thunks the compiler emits) ---- */

int64_t spite_repl_parse_int_token(const char* token) { return strtoll(token, 0, 10); }
uint64_t spite_repl_parse_unsigned_token(const char* token) { return strtoull(token, 0, 10); }
double spite_repl_parse_float_token(const char* token) { return strtod(token, 0); }
bool spite_repl_parse_bool_token(const char* token) { return strcmp(token, "true") == 0; }
SpiteString* spite_repl_parse_string_token(const char* token) { return spite_string_from_cstring_owned(token); }

int spite_repl_parse_enum_token(int32_t enum_type_index, const char* token) {
    if (enum_type_index < 0 || enum_type_index >= spite_reflect_enum_count) return 0;
    const SpiteReflectEnumType* enum_type = &spite_reflect_enums[enum_type_index];
    for (int32_t index = 0; index < enum_type->value_count; index = index + 1) {
        if (strcmp(enum_type->value_names[index], token) == 0) return index;
    }
    return 0;
}

bool spite_repl_write_enum_from_text(int32_t enum_type_index, void* address, const char* text) {
    if (enum_type_index < 0 || enum_type_index >= spite_reflect_enum_count) return false;
    const SpiteReflectEnumType* enum_type = &spite_reflect_enums[enum_type_index];
    for (int32_t index = 0; index < enum_type->value_count; index = index + 1) {
        if (strcmp(enum_type->value_names[index], text) == 0) {
            *(int*)address = index;
            return true;
        }
    }
    return false;
}

const char* spite_repl_type_name_of(SpiteReflectKind kind, int32_t type_index) {
    switch (kind) {
        case SPITE_REFLECT_INT8: return "Tiny";
        case SPITE_REFLECT_INT16: return "Short";
        case SPITE_REFLECT_INT32: return "Int";
        case SPITE_REFLECT_INT64: return "Long";
        case SPITE_REFLECT_UINT8: return "Byte";
        case SPITE_REFLECT_UINT16: return "UnsignedShort";
        case SPITE_REFLECT_UINT32: return "UnsignedInt";
        case SPITE_REFLECT_UINT64: return "UnsignedLong";
        case SPITE_REFLECT_FLOAT32: return "Float";
        case SPITE_REFLECT_FLOAT64: return "Double";
        case SPITE_REFLECT_BOOL: return "Bool";
        case SPITE_REFLECT_STRING: return "String";
        case SPITE_REFLECT_ENUM: return (type_index >= 0 && type_index < spite_reflect_enum_count) ? spite_reflect_enums[type_index].name : "Enum";
        case SPITE_REFLECT_CLASS: return (type_index >= 0 && type_index < spite_reflect_class_count) ? spite_reflect_classes[type_index].qualified_name : "Class";
        case SPITE_REFLECT_LIST: return "List";
        case SPITE_REFLECT_DICTIONARY: return "Dictionary";
        case SPITE_REFLECT_NULLABLE: return "Nullable";
        case SPITE_REFLECT_UNION: return (type_index >= 0 && type_index < spite_reflect_union_count) ? spite_reflect_unions[type_index].name : "Union";
        default: return "";
    }
}

/* ---- rendering (always returns an owned SpiteString* the caller releases) ---- */

/* D1: there is no more free "wrap this text as a static, never-freed
 * String" constructor (`SPITE_STATIC_STRING` is a compile-time-only macro
 * for a `static`-storage global, not something a runtime `(text, length)`
 * pair can produce) -- every piece of rendered text here is instead a
 * small, ordinary, owned allocation, immediately released once whatever
 * concatenated it is done (`spite_repl_append_static`/`append_owned`
 * below), the same discipline every other Spite String value already
 * follows. */
SpiteString* spite_repl_static(const char* text, int64_t length) {
    return spite_string_from_bytes(text, length);
}

SpiteString* spite_repl_render(SpiteReflectKind kind, int32_t type_index, void* address, int depth);

SpiteString* spite_repl_render_scalar(SpiteReflectKind kind, const void* address) {
    switch (kind) {
        case SPITE_REFLECT_INT8: case SPITE_REFLECT_INT16: case SPITE_REFLECT_INT32: case SPITE_REFLECT_INT64:
            return spite_string_from_int(spite_repl_read_int(kind, address));
        case SPITE_REFLECT_UINT8: case SPITE_REFLECT_UINT16: case SPITE_REFLECT_UINT32: case SPITE_REFLECT_UINT64:
            return spite_string_from_unsigned(spite_repl_read_unsigned(kind, address));
        case SPITE_REFLECT_FLOAT32:
            return spite_string_from_float32(*(const float*)address);
        case SPITE_REFLECT_FLOAT64:
            return spite_string_from_float64(*(const double*)address);
        case SPITE_REFLECT_BOOL:
            return spite_string_from_bool(*(const bool*)address);
        case SPITE_REFLECT_STRING: {
            /* `address` has already been resolved to the real, heap-
             * allocated String's own address by the caller (every String
             * attribute/element is `via_pointer` now -- D1). */
            const SpiteString* value = (const SpiteString*)address;
            return spite_string_from_bytes(value->data, value->length);
        }
        default:
            return spite_repl_static("", 0);
    }
}

SpiteString* spite_repl_render_enum(int32_t type_index, const void* address) {
    if (type_index < 0 || type_index >= spite_reflect_enum_count) return spite_repl_static("", 0);
    const SpiteReflectEnumType* enum_type = &spite_reflect_enums[type_index];
    int value = *(const int*)address;
    if (value < 0 || value >= enum_type->value_count) return spite_string_from_cstring_owned("?");
    return spite_string_from_cstring_owned(enum_type->value_names[value]);
}

/* Concatenates `left` (owned, released) and a piece of text into a fresh
 * owned SpiteString* -- used throughout rendering to build up text one
 * piece at a time following the same single-owner discipline every other
 * Spite String value uses. */
SpiteString* spite_repl_append_static(SpiteString* left, const char* text, int64_t length) {
    SpiteString* right = spite_repl_static(text, length);
    SpiteString* result = SpiteString_concat(left, right);
    SpiteString_release(left);
    SpiteString_release(right);
    return result;
}

SpiteString* spite_repl_append_owned(SpiteString* left, SpiteString* right) {
    SpiteString* result = SpiteString_concat(left, right);
    SpiteString_release(left);
    SpiteString_release(right);
    return result;
}

/* "ClassName { attribute: value, ... }" at depth 0 (the top level being
 * printed); "ClassName {...}" at any deeper depth (a nested class
 * attribute is never expanded further -- manual.md section 13). */
SpiteString* spite_repl_render_class(int32_t class_index, void* address, int depth) {
    if (class_index < 0 || class_index >= spite_reflect_class_count || address == 0) return spite_repl_static("null", 4);
    const SpiteReflectClassType* class_type = &spite_reflect_classes[class_index];
    if (depth > 0) {
        SpiteString* result = spite_string_from_cstring_owned(class_type->qualified_name);
        return spite_repl_append_static(result, " {...}", 6);
    }
    SpiteString* result = spite_string_from_cstring_owned(class_type->qualified_name);
    result = spite_repl_append_static(result, " { ", 3);
    for (int32_t index = 0; index < class_type->attribute_count; index = index + 1) {
        const SpiteReflectAttribute* attribute = &class_type->attributes[index];
        void* slot = (char*)address + attribute->offset;
        void* resolved = attribute->via_pointer ? (slot != 0 ? *(void**)slot : 0) : slot;
        SpiteString* rendered_value = spite_repl_render(attribute->kind, attribute->type_index, resolved, depth + 1);
        result = spite_repl_append_static(result, attribute->name, (int64_t)strlen(attribute->name));
        result = spite_repl_append_static(result, ": ", 2);
        result = spite_repl_append_owned(result, rendered_value);
        if (index + 1 < class_type->attribute_count) result = spite_repl_append_static(result, ", ", 2);
    }
    return spite_repl_append_static(result, " }", 2);
}

SpiteString* spite_repl_render_list(int32_t list_index, void* address) {
    if (list_index < 0 || list_index >= spite_reflect_list_count || address == 0) return spite_string_from_cstring_owned("List(0)");
    const SpiteReflectListType* list_type = &spite_reflect_lists[list_index];
    int64_t count = *(int64_t*)((char*)address + list_type->count_offset);
    char buffer[160];
    int written = snprintf(buffer, sizeof(buffer), "List<%s>(%lld)", list_type->label, (long long)count);
    return spite_string_from_bytes(buffer, (int64_t)written);
}

SpiteString* spite_repl_render_dictionary(int32_t dictionary_index, void* address) {
    if (dictionary_index < 0 || dictionary_index >= spite_reflect_dictionary_count || address == 0) return spite_string_from_cstring_owned("Dictionary(0)");
    const SpiteReflectDictionaryType* dictionary_type = &spite_reflect_dictionaries[dictionary_index];
    int64_t count = *(int64_t*)((char*)address + dictionary_type->count_offset);
    char buffer[160];
    int written = snprintf(buffer, sizeof(buffer), "Dictionary<%s>(%lld)", dictionary_type->label, (long long)count);
    return spite_string_from_bytes(buffer, (int64_t)written);
}

/* Nullable is transparent: rendering passes straight through to the held
 * value at the *same* depth, never counting as a nesting level of its own
 * (manual.md section 13). */
SpiteString* spite_repl_render_nullable(int32_t nullable_index, void* address, int depth) {
    if (nullable_index < 0 || nullable_index >= spite_reflect_nullable_count || address == 0) return spite_repl_static("null", 4);
    const SpiteReflectNullableType* nullable_type = &spite_reflect_nullables[nullable_index];
    bool has_value = *(bool*)((char*)address + nullable_type->has_value_offset);
    if (!has_value) return spite_repl_static("null", 4);
    void* slot = (char*)address + nullable_type->value_offset;
    void* resolved = nullable_type->element_via_pointer ? (slot != 0 ? *(void**)slot : 0) : slot;
    return spite_repl_render(nullable_type->element_kind, nullable_type->element_type_index, resolved, depth);
}

SpiteString* spite_repl_render_union(int32_t union_index, void* address, int depth) {
    if (union_index < 0 || union_index >= spite_reflect_union_count || address == 0) return spite_repl_static("null", 4);
    const SpiteReflectUnionType* union_type = &spite_reflect_unions[union_index];
    int tag = *(int*)((char*)address + union_type->tag_offset);
    if (tag < 0 || tag >= union_type->member_count) return spite_string_from_cstring_owned("?");
    int32_t class_index = union_type->member_class_indices[tag];
    /* D1: a union's active member is always a class instance referred to
     * by pointer (never embedded by value) -- the "value" slot at
     * `union_type->value_offset` directly *stores* that pointer, so it
     * must be dereferenced once more to reach the member's own address,
     * exactly like any other `via_pointer` class attribute. */
    void* member_address = *(void**)((char*)address + union_type->value_offset);
    return spite_repl_render_class(class_index, member_address, depth);
}

SpiteString* spite_repl_render(SpiteReflectKind kind, int32_t type_index, void* address, int depth) {
    switch (kind) {
        case SPITE_REFLECT_ENUM: return address != 0 ? spite_repl_render_enum(type_index, address) : spite_repl_static("null", 4);
        case SPITE_REFLECT_CLASS: return spite_repl_render_class(type_index, address, depth);
        case SPITE_REFLECT_LIST: return spite_repl_render_list(type_index, address);
        case SPITE_REFLECT_DICTIONARY: return spite_repl_render_dictionary(type_index, address);
        case SPITE_REFLECT_NULLABLE: return spite_repl_render_nullable(type_index, address, depth);
        case SPITE_REFLECT_UNION: return spite_repl_render_union(type_index, address, depth);
        default: return address != 0 ? spite_repl_render_scalar(kind, address) : spite_repl_static("null", 4);
    }
}

/* ---- class/function lookup ---- */

const SpiteReflectAttribute* spite_repl_find_attribute(const SpiteReflectClassType* class_type, const char* name) {
    for (int32_t index = 0; index < class_type->attribute_count; index = index + 1) {
        if (strcmp(class_type->attributes[index].name, name) == 0) return &class_type->attributes[index];
    }
    return 0;
}

const SpiteReflectFunction* spite_repl_find_function(const SpiteReflectClassType* class_type, const char* name) {
    for (int32_t index = 0; index < class_type->function_count; index = index + 1) {
        if (strcmp(class_type->functions[index].name, name) == 0) return &class_type->functions[index];
    }
    return 0;
}

void spite_repl_append_available_attributes(char* buffer, size_t buffer_size, const SpiteReflectClassType* class_type) {
    buffer[0] = '\0';
    for (int32_t index = 0; index < class_type->attribute_count; index = index + 1) {
        if (index != 0) strncat(buffer, ", ", buffer_size - strlen(buffer) - 1);
        strncat(buffer, class_type->attributes[index].name, buffer_size - strlen(buffer) - 1);
    }
}

/* ---- path evaluation ---- */

typedef struct {
    SpiteReflectKind kind;
    int32_t type_index;
    /* Always already resolved (a Heap<T> pointer has already been
     * dereferenced): the real address of the value, or 0 for an empty
     * Nullable/Union. */
    void* address;
} SpiteReplValue;

SpiteReplValue spite_repl_root_value(void) {
    SpiteReplValue value;
    value.kind = SPITE_REFLECT_CLASS;
    value.type_index = spite_repl_program_class_index;
    value.address = spite_repl_program_pointer;
    return value;
}

/* Peels off Nullable/Union layers (manual.md section 13: "walking through a
 * non-null Nullable/Heap is transparent; a union shows its active member
 * and walks into it"), leaving whatever is inside (often, but not always, a
 * class). Fails only when a Nullable is empty or a union has no readable
 * active member. */
bool spite_repl_unwrap_transparent(SpiteReplValue value, SpiteReplValue* out, char* error_message, size_t error_message_size) {
    while (true) {
        if (value.kind == SPITE_REFLECT_NULLABLE) {
            if (value.type_index < 0 || value.type_index >= spite_reflect_nullable_count || value.address == 0) {
                snprintf(error_message, error_message_size, "value is null");
                return false;
            }
            const SpiteReflectNullableType* nullable_type = &spite_reflect_nullables[value.type_index];
            bool has_value = *(bool*)((char*)value.address + nullable_type->has_value_offset);
            if (!has_value) {
                snprintf(error_message, error_message_size, "value is null");
                return false;
            }
            void* slot = (char*)value.address + nullable_type->value_offset;
            void* resolved = nullable_type->element_via_pointer ? (slot != 0 ? *(void**)slot : 0) : slot;
            value.kind = nullable_type->element_kind;
            value.type_index = nullable_type->element_type_index;
            value.address = resolved;
            continue;
        }
        if (value.kind == SPITE_REFLECT_UNION) {
            if (value.type_index < 0 || value.type_index >= spite_reflect_union_count || value.address == 0) {
                snprintf(error_message, error_message_size, "value is null");
                return false;
            }
            const SpiteReflectUnionType* union_type = &spite_reflect_unions[value.type_index];
            int tag = *(int*)((char*)value.address + union_type->tag_offset);
            if (tag < 0 || tag >= union_type->member_count) {
                snprintf(error_message, error_message_size, "union has no active member");
                return false;
            }
            value.kind = SPITE_REFLECT_CLASS;
            value.type_index = union_type->member_class_indices[tag];
            /* D1: the active member is always a class instance referred to
             * by pointer (never embedded by value) -- the "value" slot
             * *stores* that pointer, so it needs one more dereference to
             * reach the member's own address (see `spite_repl_render_union`'s
             * identical fix/comment). */
            value.address = *(void**)((char*)value.address + union_type->value_offset);
            continue;
        }
        /* D1: a reference-kind value (class/List/Dictionary/String) is a
         * plain pointer that can itself be null -- either because it
         * collapsed here from a `Nullable<Reference>` attribute/element
         * (which no longer has its own separate has_value flag to check
         * above -- see `reflectDescriptorFor`'s doc comment) or because an
         * empty union member slot resolved to one. Treated as "null"
         * uniformly here so every navigation function below (`step_member`/
         * `step_index`/`call`) never has to dereference it.
         */
        if ((value.kind == SPITE_REFLECT_CLASS || value.kind == SPITE_REFLECT_LIST || value.kind == SPITE_REFLECT_DICTIONARY || value.kind == SPITE_REFLECT_STRING) && value.address == 0) {
            snprintf(error_message, error_message_size, "value is null");
            return false;
        }
        break;
    }
    *out = value;
    return true;
}

bool spite_repl_step_member(SpiteReplValue current, const char* name, SpiteReplValue* out, char* error_message, size_t error_message_size) {
    SpiteReplValue unwrapped;
    if (!spite_repl_unwrap_transparent(current, &unwrapped, error_message, error_message_size)) return false;
    if (unwrapped.kind != SPITE_REFLECT_CLASS) {
        snprintf(error_message, error_message_size, "'%s' is not a class instance", name);
        return false;
    }
    if (unwrapped.type_index < 0 || unwrapped.type_index >= spite_reflect_class_count) {
        snprintf(error_message, error_message_size, "internal error: unknown class");
        return false;
    }
    const SpiteReflectClassType* class_type = &spite_reflect_classes[unwrapped.type_index];
    const SpiteReflectAttribute* attribute = spite_repl_find_attribute(class_type, name);
    if (attribute == 0) {
        char available[256];
        spite_repl_append_available_attributes(available, sizeof(available), class_type);
        snprintf(error_message, error_message_size, "unknown attribute '%s' on %s (available: %s)", name, class_type->qualified_name, available);
        return false;
    }
    void* slot = (char*)unwrapped.address + attribute->offset;
    void* resolved = attribute->via_pointer ? (slot != 0 ? *(void**)slot : 0) : slot;
    out->kind = attribute->kind;
    out->type_index = attribute->type_index;
    out->address = resolved;
    return true;
}

bool spite_repl_step_index(SpiteReplValue current, const char* literal_text, SpiteReplValue* out, char* error_message, size_t error_message_size) {
    SpiteReplValue unwrapped;
    if (!spite_repl_unwrap_transparent(current, &unwrapped, error_message, error_message_size)) return false;
    if (unwrapped.kind == SPITE_REFLECT_LIST) {
        const SpiteReflectListType* list_type = &spite_reflect_lists[unwrapped.type_index];
        int64_t count = *(int64_t*)((char*)unwrapped.address + list_type->count_offset);
        int64_t index = strtoll(literal_text, 0, 10);
        if (index < 0 || index >= count) {
            snprintf(error_message, error_message_size, "index %lld out of range (0..%lld)", (long long)index, (long long)(count - 1));
            return false;
        }
        void* items = *(void**)((char*)unwrapped.address + list_type->items_offset);
        void* slot = (char*)items + index * (int64_t)list_type->element_size;
        void* resolved = list_type->element_via_pointer ? (slot != 0 ? *(void**)slot : 0) : slot;
        out->kind = list_type->element_kind;
        out->type_index = list_type->element_type_index;
        out->address = resolved;
        return true;
    }
    if (unwrapped.kind == SPITE_REFLECT_DICTIONARY) {
        const SpiteReflectDictionaryType* dictionary_type = &spite_reflect_dictionaries[unwrapped.type_index];
        int64_t count = *(int64_t*)((char*)unwrapped.address + dictionary_type->count_offset);
        /* D1: `String` is a heap object referred to by pointer -- a
         * Dictionary's `keys` field is a `SpiteString**` (an array of
         * `SpiteString*`), not an array of `SpiteString` values in place. */
        SpiteString** keys = *(SpiteString***)((char*)unwrapped.address + dictionary_type->keys_offset);
        int64_t found_index = -1;
        int64_t literal_length = (int64_t)strlen(literal_text);
        for (int64_t index = 0; index < count; index = index + 1) {
            if (literal_length == keys[index]->length && memcmp(keys[index]->data, literal_text, (size_t)literal_length) == 0) {
                found_index = index;
                break;
            }
        }
        if (found_index < 0) {
            char available[256];
            available[0] = '\0';
            for (int64_t index = 0; index < count && index < 20; index = index + 1) {
                if (index != 0) strncat(available, ", ", sizeof(available) - strlen(available) - 1);
                strncat(available, keys[index]->data, sizeof(available) - strlen(available) - 1);
            }
            snprintf(error_message, error_message_size, "unknown key '%s' (available: %s)", literal_text, available);
            return false;
        }
        void* values = *(void**)((char*)unwrapped.address + dictionary_type->values_offset);
        void* slot = (char*)values + found_index * (int64_t)dictionary_type->element_size;
        void* resolved = dictionary_type->element_via_pointer ? (slot != 0 ? *(void**)slot : 0) : slot;
        out->kind = dictionary_type->element_kind;
        out->type_index = dictionary_type->element_type_index;
        out->address = resolved;
        return true;
    }
    snprintf(error_message, error_message_size, "value is not indexable (expected a List or Dictionary)");
    return false;
}

/* ---- tokenizing (paths/calls/literals are a small enough grammar that a
 * single hand-rolled cursor, rather than a separate token array, is enough) ---- */

typedef struct {
    const char* cursor;
} SpiteReplParser;

void spite_repl_skip_spaces(SpiteReplParser* parser) {
    while (*parser->cursor == ' ' || *parser->cursor == '\t') parser->cursor = parser->cursor + 1;
}

bool spite_repl_is_identifier_start(char character) {
    return isalpha((unsigned char)character) || character == '_';
}

bool spite_repl_is_identifier_character(char character) {
    return isalnum((unsigned char)character) || character == '_';
}

bool spite_repl_parse_identifier(SpiteReplParser* parser, char* buffer, size_t buffer_size) {
    spite_repl_skip_spaces(parser);
    if (!spite_repl_is_identifier_start(*parser->cursor)) return false;
    size_t length = 0;
    while (spite_repl_is_identifier_character(*parser->cursor) && length + 1 < buffer_size) {
        buffer[length] = *parser->cursor;
        length = length + 1;
        parser->cursor = parser->cursor + 1;
    }
    buffer[length] = '\0';
    return true;
}

/* Extracts the raw text of one literal (a quoted String, a quoted enum
 * value, or a bare number/true/false), unquoted, advancing past it. The
 * literal's *meaning* depends entirely on the attribute/parameter kind it
 * is eventually written into -- this only ever extracts text. */
bool spite_repl_parse_literal_token(SpiteReplParser* parser, char* buffer, size_t buffer_size) {
    spite_repl_skip_spaces(parser);
    char first = *parser->cursor;
    if (first == '"' || first == '\'') {
        char quote = first;
        parser->cursor = parser->cursor + 1;
        size_t length = 0;
        while (*parser->cursor != quote && *parser->cursor != '\0') {
            char character = *parser->cursor;
            if (character == '\\' && *(parser->cursor + 1) != '\0') {
                parser->cursor = parser->cursor + 1;
                character = *parser->cursor;
                if (character == 'n') character = '\n';
                else if (character == 't') character = '\t';
            }
            if (length + 1 < buffer_size) {
                buffer[length] = character;
                length = length + 1;
            }
            parser->cursor = parser->cursor + 1;
        }
        if (*parser->cursor == quote) parser->cursor = parser->cursor + 1;
        buffer[length] = '\0';
        return true;
    }
    size_t length = 0;
    if (*parser->cursor == '-') {
        buffer[length] = *parser->cursor;
        length = length + 1;
        parser->cursor = parser->cursor + 1;
    }
    while ((isalnum((unsigned char)*parser->cursor) || *parser->cursor == '.') && length + 1 < buffer_size) {
        buffer[length] = *parser->cursor;
        length = length + 1;
        parser->cursor = parser->cursor + 1;
    }
    if (length == 0) return false;
    buffer[length] = '\0';
    return true;
}

/* ---- top-level command result ---- */

typedef struct {
    bool ok;
    bool should_exit;
    SpiteString* value;
    SpiteString* type_name;
    SpiteString* error;
} SpiteReplCommandResult;

SpiteReplCommandResult spite_repl_make_error(const char* message) {
    SpiteReplCommandResult result;
    result.ok = false;
    result.should_exit = false;
    result.value = spite_repl_static("", 0);
    result.type_name = spite_repl_static("", 0);
    result.error = spite_string_from_cstring_owned(message);
    return result;
}

SpiteReplCommandResult spite_repl_make_success(SpiteString* value, const char* type_name) {
    SpiteReplCommandResult result;
    result.ok = true;
    result.should_exit = false;
    result.value = value;
    result.type_name = spite_string_from_cstring_owned(type_name);
    result.error = spite_repl_static("", 0);
    return result;
}

void spite_repl_drop_result(SpiteReplCommandResult* result) {
    SpiteString_release(result->value);
    SpiteString_release(result->type_name);
    SpiteString_release(result->error);
}

/* Calls a List<T>/Dictionary<T> built-in (count/keys/has) or a reflected
 * class function, given its already-tokenized literal arguments. */
SpiteReplCommandResult spite_repl_call(SpiteReplValue current, const char* name, char arguments_text[][128], int32_t argument_count) {
    char error_message[256];
    SpiteReplValue unwrapped;
    if (!spite_repl_unwrap_transparent(current, &unwrapped, error_message, sizeof(error_message))) return spite_repl_make_error(error_message);

    if (unwrapped.kind == SPITE_REFLECT_LIST) {
        const SpiteReflectListType* list_type = &spite_reflect_lists[unwrapped.type_index];
        int64_t count = *(int64_t*)((char*)unwrapped.address + list_type->count_offset);
        if (strcmp(name, "count") == 0 && argument_count == 0) return spite_repl_make_success(spite_string_from_int(count), "Int");
        return spite_repl_make_error("not callable");
    }
    if (unwrapped.kind == SPITE_REFLECT_DICTIONARY) {
        const SpiteReflectDictionaryType* dictionary_type = &spite_reflect_dictionaries[unwrapped.type_index];
        int64_t count = *(int64_t*)((char*)unwrapped.address + dictionary_type->count_offset);
        if (strcmp(name, "count") == 0 && argument_count == 0) return spite_repl_make_success(spite_string_from_int(count), "Int");
        if (strcmp(name, "keys") == 0 && argument_count == 0) {
            char buffer[64];
            int written = snprintf(buffer, sizeof(buffer), "List<String>(%lld)", (long long)count);
            return spite_repl_make_success(spite_string_from_bytes(buffer, (int64_t)written), "List");
        }
        if (strcmp(name, "has") == 0 && argument_count == 1) {
            SpiteString** keys = *(SpiteString***)((char*)unwrapped.address + dictionary_type->keys_offset);
            int64_t literal_length = (int64_t)strlen(arguments_text[0]);
            bool found = false;
            for (int64_t index = 0; index < count; index = index + 1) {
                if (literal_length == keys[index]->length && memcmp(keys[index]->data, arguments_text[0], (size_t)literal_length) == 0) {
                    found = true;
                    break;
                }
            }
            return spite_repl_make_success(spite_string_from_bool(found), "Bool");
        }
        return spite_repl_make_error("not callable");
    }
    if (unwrapped.kind == SPITE_REFLECT_CLASS) {
        const SpiteReflectClassType* class_type = &spite_reflect_classes[unwrapped.type_index];
        const SpiteReflectFunction* function = spite_repl_find_function(class_type, name);
        if (function == 0) return spite_repl_make_error("not callable");
        if (function->parameter_count != argument_count) {
            char message[128];
            snprintf(message, sizeof(message), "'%s' expects %d argument(s), got %d", name, function->parameter_count, argument_count);
            return spite_repl_make_error(message);
        }
        const char* argument_pointers[16];
        int32_t usable_count = argument_count < 16 ? argument_count : 16;
        for (int32_t index = 0; index < usable_count; index = index + 1) argument_pointers[index] = arguments_text[index];
        SpiteReplArguments repl_arguments;
        repl_arguments.tokens = argument_pointers;
        repl_arguments.count = argument_count;
        SpiteString* rendered = function->thunk(unwrapped.address, &repl_arguments);
        return spite_repl_make_success(rendered, function->return_type_name);
    }
    return spite_repl_make_error("not callable");
}

SpiteReplCommandResult spite_repl_evaluate_expression(SpiteReplParser* parser) {
    char identifier[64];
    if (!spite_repl_parse_identifier(parser, identifier, sizeof(identifier))) return spite_repl_make_error("expected an expression");
    if (strcmp(identifier, "program") != 0) {
        char message[96];
        snprintf(message, sizeof(message), "unknown name '%s' (expressions start with 'program')", identifier);
        return spite_repl_make_error(message);
    }

    SpiteReplValue current = spite_repl_root_value();
    char error_message[256];

    while (true) {
        spite_repl_skip_spaces(parser);
        char next = *parser->cursor;
        if (next == '.') {
            parser->cursor = parser->cursor + 1;
            char name[64];
            if (!spite_repl_parse_identifier(parser, name, sizeof(name))) return spite_repl_make_error("expected an attribute or function name after '.'");
            spite_repl_skip_spaces(parser);
            if (*parser->cursor == '(') {
                parser->cursor = parser->cursor + 1;
                char arguments_text[16][128];
                int32_t argument_count = 0;
                spite_repl_skip_spaces(parser);
                if (*parser->cursor != ')') {
                    while (true) {
                        if (argument_count >= 16 || !spite_repl_parse_literal_token(parser, arguments_text[argument_count], sizeof(arguments_text[0]))) {
                            return spite_repl_make_error("expected an argument literal");
                        }
                        argument_count = argument_count + 1;
                        spite_repl_skip_spaces(parser);
                        if (*parser->cursor == ',') {
                            parser->cursor = parser->cursor + 1;
                            continue;
                        }
                        break;
                    }
                }
                spite_repl_skip_spaces(parser);
                if (*parser->cursor != ')') return spite_repl_make_error("expected ')'");
                parser->cursor = parser->cursor + 1;
                spite_repl_skip_spaces(parser);
                if (*parser->cursor != '\0') return spite_repl_make_error("a function call must be the last part of an expression");
                return spite_repl_call(current, name, arguments_text, argument_count);
            }
            SpiteReplValue next_value;
            if (!spite_repl_step_member(current, name, &next_value, error_message, sizeof(error_message))) return spite_repl_make_error(error_message);
            current = next_value;
            continue;
        }
        if (next == '[') {
            parser->cursor = parser->cursor + 1;
            char literal_text[128];
            if (!spite_repl_parse_literal_token(parser, literal_text, sizeof(literal_text))) return spite_repl_make_error("expected an index or key inside '['");
            spite_repl_skip_spaces(parser);
            if (*parser->cursor != ']') return spite_repl_make_error("expected ']'");
            parser->cursor = parser->cursor + 1;
            SpiteReplValue next_value;
            if (!spite_repl_step_index(current, literal_text, &next_value, error_message, sizeof(error_message))) return spite_repl_make_error(error_message);
            current = next_value;
            continue;
        }
        break;
    }

    spite_repl_skip_spaces(parser);
    if (*parser->cursor != '\0') {
        char message[160];
        snprintf(message, sizeof(message), "unexpected text: %s", parser->cursor);
        return spite_repl_make_error(message);
    }
    SpiteString* rendered = spite_repl_render(current.kind, current.type_index, current.address, 0);
    return spite_repl_make_success(rendered, spite_repl_type_name_of(current.kind, current.type_index));
}

/* Evaluates the path on the left of an `=`, stopping one segment short so
 * the final `.attribute`/`[index]` can be written instead of read. */
SpiteReplCommandResult spite_repl_execute_assignment(const char* left_text, const char* right_text) {
    SpiteReplParser parser;
    parser.cursor = left_text;
    char identifier[64];
    if (!spite_repl_parse_identifier(&parser, identifier, sizeof(identifier)) || strcmp(identifier, "program") != 0) {
        return spite_repl_make_error("assignment target must start with 'program'");
    }

    SpiteReplValue current = spite_repl_root_value();
    char error_message[256];
    char pending_kind = 0; /* 'm' (member) or 'i' (index) */
    char pending_name[64];
    char pending_index_text[128];

    while (true) {
        spite_repl_skip_spaces(&parser);
        char next = *parser.cursor;
        if (next == '\0') break;
        if (next == '.') {
            parser.cursor = parser.cursor + 1;
            char name[64];
            if (!spite_repl_parse_identifier(&parser, name, sizeof(name))) return spite_repl_make_error("expected an attribute name after '.'");
            spite_repl_skip_spaces(&parser);
            if (*parser.cursor == '\0') {
                pending_kind = 'm';
                strncpy(pending_name, name, sizeof(pending_name) - 1);
                pending_name[sizeof(pending_name) - 1] = '\0';
                break;
            }
            SpiteReplValue next_value;
            if (!spite_repl_step_member(current, name, &next_value, error_message, sizeof(error_message))) return spite_repl_make_error(error_message);
            current = next_value;
            continue;
        }
        if (next == '[') {
            parser.cursor = parser.cursor + 1;
            char literal_text[128];
            if (!spite_repl_parse_literal_token(&parser, literal_text, sizeof(literal_text))) return spite_repl_make_error("expected an index or key inside '['");
            spite_repl_skip_spaces(&parser);
            if (*parser.cursor != ']') return spite_repl_make_error("expected ']'");
            parser.cursor = parser.cursor + 1;
            spite_repl_skip_spaces(&parser);
            if (*parser.cursor == '\0') {
                pending_kind = 'i';
                strncpy(pending_index_text, literal_text, sizeof(pending_index_text) - 1);
                pending_index_text[sizeof(pending_index_text) - 1] = '\0';
                break;
            }
            SpiteReplValue next_value;
            if (!spite_repl_step_index(current, literal_text, &next_value, error_message, sizeof(error_message))) return spite_repl_make_error(error_message);
            current = next_value;
            continue;
        }
        return spite_repl_make_error("unsupported assignment target");
    }

    if (pending_kind == 0) return spite_repl_make_error("assignment target must end with an attribute or index");

    SpiteReplValue unwrapped_parent;
    if (!spite_repl_unwrap_transparent(current, &unwrapped_parent, error_message, sizeof(error_message))) return spite_repl_make_error(error_message);

    char literal_text[128];
    {
        SpiteReplParser value_parser;
        value_parser.cursor = right_text;
        if (!spite_repl_parse_literal_token(&value_parser, literal_text, sizeof(literal_text))) return spite_repl_make_error("expected a literal value on the right of '='");
    }

    if (pending_kind == 'm') {
        if (unwrapped_parent.kind != SPITE_REFLECT_CLASS) return spite_repl_make_error("assignment target is not a class instance");
        const SpiteReflectClassType* class_type = &spite_reflect_classes[unwrapped_parent.type_index];
        const SpiteReflectAttribute* attribute = spite_repl_find_attribute(class_type, pending_name);
        if (attribute == 0) {
            char available[256];
            spite_repl_append_available_attributes(available, sizeof(available), class_type);
            char message[320];
            snprintf(message, sizeof(message), "unknown attribute '%s' on %s (available: %s)", pending_name, class_type->qualified_name, available);
            return spite_repl_make_error(message);
        }
        if (!spite_repl_kind_is_literal_assignable(attribute->kind)) {
            char message[128];
            snprintf(message, sizeof(message), "type mismatch: '%s' is not a scalar/String/enum attribute", pending_name);
            return spite_repl_make_error(message);
        }
        char setter_name[80];
        snprintf(setter_name, sizeof(setter_name), "set_%s", pending_name);
        const SpiteReflectFunction* setter = spite_repl_find_function(class_type, setter_name);
        if (setter != 0 && setter->parameter_count == 1) {
            const char* argument_pointers[1];
            argument_pointers[0] = literal_text;
            SpiteReplArguments repl_arguments;
            repl_arguments.tokens = argument_pointers;
            repl_arguments.count = 1;
            SpiteString* rendered = setter->thunk(unwrapped_parent.address, &repl_arguments);
            SpiteString_release(rendered);
        } else {
            void* slot = (char*)unwrapped_parent.address + attribute->offset;
            if (attribute->kind == SPITE_REFLECT_ENUM) {
                if (!spite_repl_write_enum_from_text(attribute->type_index, slot, literal_text)) return spite_repl_make_error("unknown enum value");
            } else {
                spite_repl_write_scalar_from_text(attribute->kind, slot, literal_text);
            }
        }
        /* Reading the value back to render it needs the same `via_pointer`
         * resolution any other read does (D1: a String attribute's slot
         * holds a pointer, not the value itself -- see this file's own
         * top comment and `SpiteReflectAttribute`'s). */
        void* rendered_slot = (char*)unwrapped_parent.address + attribute->offset;
        void* rendered_address = attribute->via_pointer ? (rendered_slot != 0 ? *(void**)rendered_slot : 0) : rendered_slot;
        SpiteString* rendered_value = spite_repl_render(attribute->kind, attribute->type_index, rendered_address, 0);
        return spite_repl_make_success(rendered_value, spite_repl_type_name_of(attribute->kind, attribute->type_index));
    }

    if (unwrapped_parent.kind == SPITE_REFLECT_LIST) {
        const SpiteReflectListType* list_type = &spite_reflect_lists[unwrapped_parent.type_index];
        int64_t count = *(int64_t*)((char*)unwrapped_parent.address + list_type->count_offset);
        int64_t index = strtoll(pending_index_text, 0, 10);
        if (index < 0 || index >= count) {
            char message[64];
            snprintf(message, sizeof(message), "index %lld out of range (0..%lld)", (long long)index, (long long)(count - 1));
            return spite_repl_make_error(message);
        }
        if (!spite_repl_kind_is_literal_assignable(list_type->element_kind)) return spite_repl_make_error("type mismatch: list element is not scalar/String/enum");
        void* items = *(void**)((char*)unwrapped_parent.address + list_type->items_offset);
        void* slot = (char*)items + index * (int64_t)list_type->element_size;
        if (list_type->element_kind == SPITE_REFLECT_ENUM) {
            if (!spite_repl_write_enum_from_text(list_type->element_type_index, slot, literal_text)) return spite_repl_make_error("unknown enum value");
        } else {
            spite_repl_write_scalar_from_text(list_type->element_kind, slot, literal_text);
        }
        void* rendered_address = list_type->element_via_pointer ? (slot != 0 ? *(void**)slot : 0) : slot;
        SpiteString* rendered_value = spite_repl_render(list_type->element_kind, list_type->element_type_index, rendered_address, 0);
        return spite_repl_make_success(rendered_value, spite_repl_type_name_of(list_type->element_kind, list_type->element_type_index));
    }
    if (unwrapped_parent.kind == SPITE_REFLECT_DICTIONARY) {
        const SpiteReflectDictionaryType* dictionary_type = &spite_reflect_dictionaries[unwrapped_parent.type_index];
        int64_t count = *(int64_t*)((char*)unwrapped_parent.address + dictionary_type->count_offset);
        SpiteString** keys = *(SpiteString***)((char*)unwrapped_parent.address + dictionary_type->keys_offset);
        int64_t found_index = -1;
        int64_t literal_length = (int64_t)strlen(pending_index_text);
        for (int64_t index = 0; index < count; index = index + 1) {
            if (literal_length == keys[index]->length && memcmp(keys[index]->data, pending_index_text, (size_t)literal_length) == 0) {
                found_index = index;
                break;
            }
        }
        if (found_index < 0) return spite_repl_make_error("unknown key");
        if (!spite_repl_kind_is_literal_assignable(dictionary_type->element_kind)) return spite_repl_make_error("type mismatch: dictionary value is not scalar/String/enum");
        void* values = *(void**)((char*)unwrapped_parent.address + dictionary_type->values_offset);
        void* slot = (char*)values + found_index * (int64_t)dictionary_type->element_size;
        if (dictionary_type->element_kind == SPITE_REFLECT_ENUM) {
            if (!spite_repl_write_enum_from_text(dictionary_type->element_type_index, slot, literal_text)) return spite_repl_make_error("unknown enum value");
        } else {
            spite_repl_write_scalar_from_text(dictionary_type->element_kind, slot, literal_text);
        }
        void* rendered_address = dictionary_type->element_via_pointer ? (slot != 0 ? *(void**)slot : 0) : slot;
        SpiteString* rendered_value = spite_repl_render(dictionary_type->element_kind, dictionary_type->element_type_index, rendered_address, 0);
        return spite_repl_make_success(rendered_value, spite_repl_type_name_of(dictionary_type->element_kind, dictionary_type->element_type_index));
    }
    return spite_repl_make_error("assignment target is not indexable");
}

/* ---- meta commands ---- */

SpiteString* spite_repl_join_lines(const char* const* lines, int32_t count) {
    SpiteString* result = spite_repl_static("", 0);
    for (int32_t index = 0; index < count; index = index + 1) {
        if (index != 0) result = spite_repl_append_static(result, "\n", 1);
        result = spite_repl_append_static(result, lines[index], (int64_t)strlen(lines[index]));
    }
    return result;
}

SpiteReplCommandResult spite_repl_command_classes(void) {
    const char* names[512];
    int32_t count = spite_reflect_class_count < 512 ? spite_reflect_class_count : 512;
    for (int32_t index = 0; index < count; index = index + 1) names[index] = spite_reflect_classes[index].qualified_name;
    return spite_repl_make_success(spite_repl_join_lines(names, count), "");
}

SpiteReplCommandResult spite_repl_command_enums(void) {
    static char lines_storage[64][256];
    const char* lines[64];
    int32_t count = spite_reflect_enum_count < 64 ? spite_reflect_enum_count : 64;
    for (int32_t index = 0; index < count; index = index + 1) {
        const SpiteReflectEnumType* enum_type = &spite_reflect_enums[index];
        int written = snprintf(lines_storage[index], sizeof(lines_storage[index]), "%s: ", enum_type->name);
        for (int32_t value_index = 0; value_index < enum_type->value_count; value_index = value_index + 1) {
            written = written + snprintf(lines_storage[index] + written, sizeof(lines_storage[index]) - (size_t)written, "%s%s", value_index == 0 ? "" : ", ", enum_type->value_names[value_index]);
        }
        lines[index] = lines_storage[index];
    }
    return spite_repl_make_success(spite_repl_join_lines(lines, count), "");
}

SpiteReplCommandResult spite_repl_command_memory(void) {
#ifdef SPITE_DEBUG_MEMORY
    char buffer[128];
    int written = snprintf(buffer, sizeof(buffer), "live allocations: %lld, live bytes: %lld", (long long)spite_debug_live_count, (long long)spite_debug_live_bytes);
    return spite_repl_make_success(spite_string_from_bytes(buffer, (int64_t)written), "");
#else
    return spite_repl_make_success(spite_string_from_cstring_owned("memory tracking is not enabled"), "");
#endif
}

SpiteReplCommandResult spite_repl_command_describe(const char* name) {
    for (int32_t class_index = 0; class_index < spite_reflect_class_count; class_index = class_index + 1) {
        if (strcmp(spite_reflect_classes[class_index].qualified_name, name) != 0) continue;
        const SpiteReflectClassType* class_type = &spite_reflect_classes[class_index];
        static char storage[128][256];
        const char* lines[128];
        int32_t line_count = 0;
        snprintf(storage[line_count], sizeof(storage[0]), "%s", class_type->qualified_name);
        lines[line_count] = storage[line_count];
        line_count = line_count + 1;
        for (int32_t attribute_index = 0; attribute_index < class_type->attribute_count && line_count < 128; attribute_index = attribute_index + 1) {
            snprintf(storage[line_count], sizeof(storage[0]), "  %s: %s", class_type->attributes[attribute_index].name, class_type->attributes[attribute_index].type_name);
            lines[line_count] = storage[line_count];
            line_count = line_count + 1;
        }
        for (int32_t function_index = 0; function_index < class_type->function_count && line_count < 128; function_index = function_index + 1) {
            const SpiteReflectFunction* function = &class_type->functions[function_index];
            char parameters_text[128];
            parameters_text[0] = '\0';
            for (int32_t parameter_index = 0; parameter_index < function->parameter_count; parameter_index = parameter_index + 1) {
                if (parameter_index != 0) strncat(parameters_text, ", ", sizeof(parameters_text) - strlen(parameters_text) - 1);
                strncat(parameters_text, function->parameter_type_names[parameter_index], sizeof(parameters_text) - strlen(parameters_text) - 1);
            }
            snprintf(storage[line_count], sizeof(storage[0]), "  %s(%s): %s", function->name, parameters_text, function->return_type_name);
            lines[line_count] = storage[line_count];
            line_count = line_count + 1;
        }
        return spite_repl_make_success(spite_repl_join_lines(lines, line_count), "");
    }
    char message[128];
    snprintf(message, sizeof(message), "unknown class '%s'", name);
    return spite_repl_make_error(message);
}

static const char* spite_repl_help_text =
    "commands: program<path>, program<path>(args), program<path> = literal, classes, describe <Class>, enums, memory, help, exit";

/* ---- top-level dispatch ---- */

SpiteReplCommandResult spite_repl_execute(const char* raw_line) {
    while (*raw_line == ' ' || *raw_line == '\t') raw_line = raw_line + 1;
    size_t length = strlen(raw_line);
    while (length > 0 && (raw_line[length - 1] == ' ' || raw_line[length - 1] == '\t' || raw_line[length - 1] == '\r' || raw_line[length - 1] == '\n')) length = length - 1;

    static char trimmed[1024];
    if (length >= sizeof(trimmed)) length = sizeof(trimmed) - 1;
    memcpy(trimmed, raw_line, length);
    trimmed[length] = '\0';

    if (length == 0) return spite_repl_make_success(spite_repl_static("", 0), "");
    if (strcmp(trimmed, "exit") == 0) {
        SpiteReplCommandResult result = spite_repl_make_success(spite_string_from_cstring_owned("goodbye"), "");
        result.should_exit = true;
        return result;
    }
    if (strcmp(trimmed, "help") == 0) return spite_repl_make_success(spite_string_from_cstring_owned(spite_repl_help_text), "");
    if (strcmp(trimmed, "classes") == 0) return spite_repl_command_classes();
    if (strcmp(trimmed, "enums") == 0) return spite_repl_command_enums();
    if (strcmp(trimmed, "memory") == 0) return spite_repl_command_memory();
    if (strncmp(trimmed, "describe", 8) == 0 && (trimmed[8] == ' ' || trimmed[8] == '\t')) {
        const char* name = trimmed + 8;
        while (*name == ' ' || *name == '\t') name = name + 1;
        return spite_repl_command_describe(name);
    }

    int equals_index = -1;
    bool in_double_quote = false;
    bool in_single_quote = false;
    for (size_t index = 0; index < length; index = index + 1) {
        char character = trimmed[index];
        if (character == '"' && !in_single_quote) in_double_quote = !in_double_quote;
        else if (character == '\'' && !in_double_quote) in_single_quote = !in_single_quote;
        else if (character == '=' && !in_double_quote && !in_single_quote) {
            equals_index = (int)index;
            break;
        }
    }
    if (equals_index >= 0) {
        trimmed[equals_index] = '\0';
        return spite_repl_execute_assignment(trimmed, trimmed + equals_index + 1);
    }

    SpiteReplParser parser;
    parser.cursor = trimmed;
    return spite_repl_evaluate_expression(&parser);
}

/* ---- JSON wire protocol (--repl-port) ---- */

void spite_repl_append_json_escaped(char* buffer, size_t buffer_size, size_t* used, const char* text, int64_t length) {
    for (int64_t index = 0; index < length; index = index + 1) {
        char character = text[index];
        const char* escape = 0;
        switch (character) {
            case '"': escape = "\\\""; break;
            case '\\': escape = "\\\\"; break;
            case '\n': escape = "\\n"; break;
            case '\r': escape = "\\r"; break;
            case '\t': escape = "\\t"; break;
            default: break;
        }
        if (escape != 0) {
            size_t escape_length = strlen(escape);
            if (*used + escape_length < buffer_size) {
                memcpy(buffer + *used, escape, escape_length);
                *used = *used + escape_length;
            }
        } else if ((unsigned char)character >= 0x20) {
            if (*used + 1 < buffer_size) {
                buffer[*used] = character;
                *used = *used + 1;
            }
        }
    }
}

SpiteString* spite_repl_result_to_json(const SpiteReplCommandResult* result) {
    static char buffer[8192];
    size_t used = 0;
    if (result->ok) {
        used = used + (size_t)snprintf(buffer + used, sizeof(buffer) - used, "{\"ok\":true,\"value\":\"");
        spite_repl_append_json_escaped(buffer, sizeof(buffer), &used, result->value->data, result->value->length);
        used = used + (size_t)snprintf(buffer + used, sizeof(buffer) - used, "\",\"type\":\"");
        spite_repl_append_json_escaped(buffer, sizeof(buffer), &used, result->type_name->data, result->type_name->length);
        used = used + (size_t)snprintf(buffer + used, sizeof(buffer) - used, "\"}");
    } else {
        used = used + (size_t)snprintf(buffer + used, sizeof(buffer) - used, "{\"ok\":false,\"error\":\"");
        spite_repl_append_json_escaped(buffer, sizeof(buffer), &used, result->error->data, result->error->length);
        used = used + (size_t)snprintf(buffer + used, sizeof(buffer) - used, "\"}");
    }
    return spite_string_from_bytes(buffer, (int64_t)used);
}

/* ---- local stdin REPL (--repl) ---- */

void spite_repl_run_stdin(void) {
    while (true) {
        printf("spite> ");
        fflush(stdout);
        bool ok = false;
        SpiteString* line = spite_console_read_line(&ok);
        if (!ok) {
            SpiteString_release(line);
            break;
        }
        char text[1024];
        int64_t copy_length = line->length < (int64_t)sizeof(text) - 1 ? line->length : (int64_t)sizeof(text) - 1;
        memcpy(text, line->data, (size_t)copy_length);
        text[copy_length] = '\0';
        SpiteString_release(line);

        SpiteReplCommandResult result = spite_repl_execute(text);
        if (result.ok) {
            printf("%.*s\n", (int)result.value->length, result.value->data);
        } else {
            printf("error: %.*s\n", (int)result.error->length, result.error->data);
        }
        bool should_exit = result.should_exit;
        spite_repl_drop_result(&result);
        if (should_exit) break;
    }
}

/* ---- remote TCP REPL (--repl-port); no authentication, 127.0.0.1 only ---- */

char* spite_repl_socket_read_line(SpiteSocketHandle socket_handle) {
    int64_t capacity = 256;
    int64_t used = 0;
    char* buffer = (char*)malloc((size_t)capacity);
    while (true) {
        char character;
        int received = recv(socket_handle, &character, 1, 0);
        if (received <= 0) {
            if (used == 0) {
                free(buffer);
                return 0;
            }
            break;
        }
        if (character == '\n') break;
        if (character == '\r') continue;
        if (used + 1 >= capacity) {
            capacity = capacity * 2;
            buffer = (char*)realloc(buffer, (size_t)capacity);
        }
        buffer[used] = character;
        used = used + 1;
    }
    buffer[used] = '\0';
    return buffer;
}

void spite_repl_socket_write_line(SpiteSocketHandle socket_handle, const char* text, int64_t length) {
    int64_t sent_total = 0;
    while (sent_total < length) {
        int sent = send(socket_handle, text + sent_total, (int)(length - sent_total), 0);
        if (sent <= 0) return;
        sent_total = sent_total + sent;
    }
    send(socket_handle, "\n", 1, 0);
}

void spite_repl_close_socket(SpiteSocketHandle socket_handle) {
#ifdef _WIN32
    closesocket(socket_handle);
#else
    close(socket_handle);
#endif
}

/* Serves one client until it disconnects or sends "exit" (which terminates
 * the whole process immediately -- the entry constructor may be running its
 * own infinite `while` loop on the main thread, so there is no guarantee of
 * ever unwinding back to a clean `main` return for the remote REPL; this is
 * a debug tool, documented as such in manual.md section 13). */
void spite_repl_handle_client(SpiteSocketHandle client_socket) {
    while (true) {
        char* line = spite_repl_socket_read_line(client_socket);
        if (line == 0) break;
        SpiteReplCommandResult result = spite_repl_execute(line);
        free(line);
        SpiteString* json = spite_repl_result_to_json(&result);
        spite_repl_socket_write_line(client_socket, json->data, json->length);
        bool should_exit = result.should_exit;
        SpiteString_release(json);
        spite_repl_drop_result(&result);
        if (should_exit) {
            spite_repl_close_socket(client_socket);
            exit(0);
        }
    }
    spite_repl_close_socket(client_socket);
}

static SpiteSocketHandle spite_repl_listen_socket = SPITE_INVALID_SOCKET;

#ifdef _WIN32
DWORD WINAPI spite_repl_server_thread(LPVOID parameter) {
    (void)parameter;
    while (true) {
        SpiteSocketHandle client_socket = accept(spite_repl_listen_socket, 0, 0);
        if (client_socket == SPITE_INVALID_SOCKET) break;
        spite_repl_handle_client(client_socket);
    }
    return 0;
}

void spite_repl_start_server(int32_t port) {
    WSADATA winsock_data;
    WSAStartup(MAKEWORD(2, 2), &winsock_data);
    spite_repl_listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port);
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    bind(spite_repl_listen_socket, (struct sockaddr*)&address, sizeof(address));
    listen(spite_repl_listen_socket, 8);
    CreateThread(0, 0, spite_repl_server_thread, 0, 0, 0);
}
#else
void* spite_repl_server_thread(void* parameter) {
    (void)parameter;
    while (true) {
        SpiteSocketHandle client_socket = accept(spite_repl_listen_socket, 0, 0);
        if (client_socket == SPITE_INVALID_SOCKET) break;
        spite_repl_handle_client(client_socket);
    }
    return 0;
}

void spite_repl_start_server(int32_t port) {
    spite_repl_listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port);
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    bind(spite_repl_listen_socket, (struct sockaddr*)&address, sizeof(address));
    listen(spite_repl_listen_socket, 8);
    pthread_t thread;
    pthread_create(&thread, 0, spite_repl_server_thread, 0);
    pthread_detach(thread);
}
#endif
