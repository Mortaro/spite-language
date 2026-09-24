typedef struct {
    int64_t count;
    const char** items;
} SpiteArguments;

typedef struct File File;
struct File {
    SpiteHeader header;
    SpiteString* path;
};

typedef struct Directory Directory;
struct Directory {
    SpiteHeader header;
    SpiteString* path;
};

typedef struct Process Process;
struct Process {
    SpiteHeader header;
    SpiteString* command;
    List_String* arguments;
    SpiteString* last_output;
};

typedef struct Program Program;
struct Program {
    SpiteHeader header;
};

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
