typedef struct {
    int64_t count;
    const char** items;
} SpiteArguments;

typedef struct Console Console;
struct Console {
    SpiteHeader header;
};

typedef struct DynamicLibrary DynamicLibrary;
struct DynamicLibrary {
    SpiteHeader header;
    SpiteString* file_name;
    void* handle;
};

typedef struct Memory Memory;
struct Memory {
    SpiteHeader header;
};
