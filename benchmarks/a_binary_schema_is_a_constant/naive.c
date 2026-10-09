/* naive/ written in C the way it reads: schema() is a function of the writer and of the reader that walks the
 * class's attributes into a text, "Reading{sensor:Integer;level:Float;label:String}", and hashes it with FNV-1a,
 * every time it is asked. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

typedef struct Reading {
    int32_t sensor;
    float level;
    char* label;
} Reading;

typedef struct AttributeDescription {
    const char* name;
    const char* type;
} AttributeDescription;

typedef struct ClassDescription {
    const char* name;
    const AttributeDescription* attributes;
    int32_t attribute_count;
} ClassDescription;

static const AttributeDescription reading_attributes[] = {
    {"sensor", "Integer"},
    {"level", "Float"},
    {"label", "String"},
};

static const ClassDescription reading_class = {"Reading", reading_attributes, 3};

typedef struct Writer {
    const ClassDescription* described;
} Writer;

typedef struct Reader {
    const ClassDescription* described;
} Reader;

typedef struct LongList {
    int64_t* items;
    int32_t count;
    int32_t capacity;
} LongList;

static LongList* long_list_make(void) {
    LongList* list = malloc(sizeof(LongList));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void long_list_append(LongList* list, int64_t item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(int64_t) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static void long_list_free(LongList* list) {
    free(list->items);
    free(list);
}

static void text_append(char** text, size_t* length, size_t* capacity, const char* piece) {
    size_t piece_length = strlen(piece);
    if (*length + piece_length + 1 > *capacity) {
        while (*length + piece_length + 1 > *capacity) *capacity = *capacity == 0 ? 16 : *capacity * 2;
        *text = realloc(*text, *capacity);
    }
    memcpy(*text + *length, piece, piece_length + 1);
    *length = *length + piece_length;
}

static int64_t schema_of(const ClassDescription* described) {
    char* text = NULL;
    size_t length = 0;
    size_t capacity = 0;
    text_append(&text, &length, &capacity, described->name);
    text_append(&text, &length, &capacity, "{");
    for (int32_t index = 0; index < described->attribute_count; index = index + 1) {
        if (index > 0) text_append(&text, &length, &capacity, ";");
        text_append(&text, &length, &capacity, described->attributes[index].name);
        text_append(&text, &length, &capacity, ":");
        text_append(&text, &length, &capacity, described->attributes[index].type);
    }
    text_append(&text, &length, &capacity, "}");
    uint64_t hash = 14695981039346656037ull;
    for (size_t index = 0; index < length; index = index + 1) {
        hash = (hash ^ (uint8_t)text[index]) * 1099511628211ull;
    }
    free(text);
    return (int64_t)hash;
}

static int64_t writer_schema(Writer* writer) {
    return schema_of(writer->described);
}

static int64_t reader_schema(Reader* reader) {
    return schema_of(reader->described);
}

static Writer* writer;
static Reader* reader;

static LongList* received_headers(int32_t count) {
    LongList* headers = long_list_make();
    for (int32_t index = 0; index < count; index = index + 1) {
        int64_t header = writer_schema(writer);
        if (index % 10 == 0) {
            header = index;
        }
        long_list_append(headers, header);
    }
    return headers;
}

static bool is_current(int64_t header) {
    return header == reader_schema(reader);
}

static int32_t count_current(LongList* headers) {
    int32_t count = 0;
    for (int32_t index = 0; index < headers->count; index = index + 1) {
        if (is_current(headers->items[index])) count = count + 1;
    }
    return count;
}

static uint8_t* write_reading(Reading* reading, size_t* size) {
    size_t label_length = strlen(reading->label);
    *size = 4 + 4 + 4 + label_length;
    uint8_t* bytes = malloc(*size);
    int32_t length = (int32_t)label_length;
    memcpy(bytes, &reading->sensor, 4);
    memcpy(bytes + 4, &reading->level, 4);
    memcpy(bytes + 8, &length, 4);
    memcpy(bytes + 12, reading->label, label_length);
    return bytes;
}

static Reading* read_reading(uint8_t* bytes, size_t size) {
    if (size < 12) return NULL;
    int32_t length = 0;
    memcpy(&length, bytes + 8, 4);
    if (length < 0 || (size_t)length != size - 12) return NULL;
    Reading* reading = malloc(sizeof(Reading));
    memcpy(&reading->sensor, bytes, 4);
    memcpy(&reading->level, bytes + 4, 4);
    reading->label = malloc((size_t)length + 1);
    memcpy(reading->label, bytes + 12, (size_t)length);
    reading->label[length] = 0;
    return reading;
}

static int32_t sent_and_read_back(int32_t sensor) {
    Reading* reading = malloc(sizeof(Reading));
    reading->sensor = sensor;
    reading->level = 0.0f;
    reading->label = calloc(1, 1);
    size_t size = 0;
    uint8_t* bytes = write_reading(reading, &size);
    Reading* read = read_reading(bytes, size);
    if (read == NULL) abort();
    int32_t answer = read->sensor;
    free(reading->label);
    free(reading);
    free(bytes);
    free(read->label);
    free(read);
    return answer;
}

int main(void) {
    writer = malloc(sizeof(Writer));
    writer->described = &reading_class;
    reader = malloc(sizeof(Reader));
    reader->described = &reading_class;
    int64_t start = now_nanoseconds();
    LongList* headers = received_headers(200000);
    int32_t accepted = 0;
    for (int32_t round = 0; round < 10; round = round + 1) {
        accepted = accepted + count_current(headers);
    }
    int64_t microseconds = microseconds_since(start);
    int32_t sensor = sent_and_read_back(7);
    printf("accepted %d sensor %d\n", accepted, sensor);
    printf("schema %lld\n", (long long)writer_schema(writer));
    print_microseconds(microseconds);
    long_list_free(headers);
    free(writer);
    free(reader);
    return 0;
}
