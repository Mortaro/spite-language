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
