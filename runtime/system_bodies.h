int64_t SpiteArguments_count(SpiteArguments* self) {
    return self->count;
}

SpiteString* SpiteArguments_get(SpiteArguments* self, int64_t index) {
    if (index < 0 || index >= self->count) return (&spite_static_string_empty);
    return spite_string_from_cstring_owned(self->items[index]);
}

SpiteString* SpiteArguments_lookup(SpiteArguments* self, const char* key) {
    int64_t key_length = (int64_t)strlen(key);
    for (int64_t index = 0; index < self->count; index = index + 1) {
        const char* item = self->items[index];
        if (strcmp(item, "--") == 0) return 0;
        if (strncmp(item, "--", 2) == 0 && strncmp(item + 2, key, (size_t)key_length) == 0 && item[2 + key_length] == '=') {
            return spite_string_from_cstring_owned(item + 2 + key_length + 1);
        }
    }
    return 0;
}

void File_init(File* self) {
    self->path = (&spite_static_string_empty);
}

File* File_allocate(void) {
    File* self = (File*)SPITE_MALLOC(sizeof(File));
    self->header.ref_count = 1;
    self->header.class_id = SPITE_CLASS_ID_FILE;
    #ifdef SPITE_DEBUG_MEMORY
    spite_debug_register_object(self, SPITE_CLASS_ID_FILE);
    #endif
    File_init(self);
    return self;
}

File* File_default(void) {
    return File_allocate();
}

File* File_make(SpiteString* path) {
    File* self = File_allocate();
    File_File(self, path);
    return self;
}

File* File_retain(File* self) {
    if (self != 0) self->header.ref_count = self->header.ref_count + 1;
    return self;
}

void File_release(File* self) {
    if (self == 0) return;
    self->header.ref_count = self->header.ref_count - 1;
    if (self->header.ref_count > 0) return;
    SpiteString_release(self->path);
    SPITE_FREE(self);
}

File* File_copy(File* self) {
    File* spite_copy = File_allocate();
    SpiteString_release(spite_copy->path);
    spite_copy->path = SpiteString_retain(self->path);
    return spite_copy;
}

File* File_deep_copy(File* self) {
    File* spite_copy = File_allocate();
    SpiteString_release(spite_copy->path);
    spite_copy->path = spite_string_from_bytes((self->path)->data, (self->path)->length);
    return spite_copy;
}

void File_File(File* self, SpiteString* path) { SpiteString_release(self->path); self->path = SpiteString_retain(path); SpiteString_release(path); }

SpiteString* File_read(File* self) {
    bool spite_file_ok = false;
    SpiteString* spite_file_content = spite_file_read_all(self->path, &spite_file_ok);
    if (!spite_file_ok) return 0;
    return spite_file_content;
}

bool File_write(File* self, SpiteString* text) { bool spite_ok = spite_file_write_all(self->path, text, "wb"); SpiteString_release(text); return spite_ok; }
bool File_append(File* self, SpiteString* text) { bool spite_ok = spite_file_write_all(self->path, text, "ab"); SpiteString_release(text); return spite_ok; }
bool File_exists(File* self) { return spite_file_exists(self->path); }
bool File_remove(File* self) { return spite_file_remove(self->path); }

void Directory_init(Directory* self) {
    self->path = (&spite_static_string_empty);
}

Directory* Directory_allocate(void) {
    Directory* self = (Directory*)SPITE_MALLOC(sizeof(Directory));
    self->header.ref_count = 1;
    self->header.class_id = SPITE_CLASS_ID_DIRECTORY;
    #ifdef SPITE_DEBUG_MEMORY
    spite_debug_register_object(self, SPITE_CLASS_ID_DIRECTORY);
    #endif
    Directory_init(self);
    return self;
}

Directory* Directory_default(void) {
    return Directory_allocate();
}

Directory* Directory_make(SpiteString* path) {
    Directory* self = Directory_allocate();
    Directory_Directory(self, path);
    return self;
}

Directory* Directory_retain(Directory* self) {
    if (self != 0) self->header.ref_count = self->header.ref_count + 1;
    return self;
}

void Directory_release(Directory* self) {
    if (self == 0) return;
    self->header.ref_count = self->header.ref_count - 1;
    if (self->header.ref_count > 0) return;
    SpiteString_release(self->path);
    SPITE_FREE(self);
}

Directory* Directory_copy(Directory* self) {
    Directory* spite_copy = Directory_allocate();
    SpiteString_release(spite_copy->path);
    spite_copy->path = SpiteString_retain(self->path);
    return spite_copy;
}

Directory* Directory_deep_copy(Directory* self) {
    Directory* spite_copy = Directory_allocate();
    SpiteString_release(spite_copy->path);
    spite_copy->path = spite_string_from_bytes((self->path)->data, (self->path)->length);
    return spite_copy;
}

void Directory_Directory(Directory* self, SpiteString* path) { SpiteString_release(self->path); self->path = SpiteString_retain(path); SpiteString_release(path); }

List_String* Directory_files(Directory* self) {
    List_String* spite_directory_result = List_String_make();
    int64_t spite_directory_count = 0;
    char** spite_directory_names = spite_directory_list(self->path, false, &spite_directory_count);
    for (int64_t spite_index = 0; spite_index < spite_directory_count; spite_index = spite_index + 1) {
        List_String_add(spite_directory_result, spite_string_take(spite_directory_names[spite_index], (int64_t)strlen(spite_directory_names[spite_index])));
    }
    SPITE_FREE(spite_directory_names);
    return spite_directory_result;
}

List_String* Directory_folders(Directory* self) {
    List_String* spite_directory_result = List_String_make();
    int64_t spite_directory_count = 0;
    char** spite_directory_names = spite_directory_list(self->path, true, &spite_directory_count);
    for (int64_t spite_index = 0; spite_index < spite_directory_count; spite_index = spite_index + 1) {
        List_String_add(spite_directory_result, spite_string_take(spite_directory_names[spite_index], (int64_t)strlen(spite_directory_names[spite_index])));
    }
    SPITE_FREE(spite_directory_names);
    return spite_directory_result;
}

bool Directory_exists(Directory* self) { return spite_directory_exists(self->path); }
bool Directory_create(Directory* self) { return spite_directory_create(self->path); }

void Process_init(Process* self) {
    self->command = (&spite_static_string_empty);
    self->arguments = List_String_make();
    self->last_output = (&spite_static_string_empty);
}

Process* Process_allocate(void) {
    Process* self = (Process*)SPITE_MALLOC(sizeof(Process));
    self->header.ref_count = 1;
    self->header.class_id = SPITE_CLASS_ID_PROCESS;
    #ifdef SPITE_DEBUG_MEMORY
    spite_debug_register_object(self, SPITE_CLASS_ID_PROCESS);
    #endif
    Process_init(self);
    return self;
}

Process* Process_default(void) {
    return Process_allocate();
}

Process* Process_make(SpiteString* command, List_String* arguments) {
    Process* self = Process_allocate();
    Process_Process(self, command, arguments);
    return self;
}

Process* Process_retain(Process* self) {
    if (self != 0) self->header.ref_count = self->header.ref_count + 1;
    return self;
}

void Process_release(Process* self) {
    if (self == 0) return;
    self->header.ref_count = self->header.ref_count - 1;
    if (self->header.ref_count > 0) return;
    SpiteString_release(self->command);
    List_String_release(self->arguments);
    SpiteString_release(self->last_output);
    SPITE_FREE(self);
}

Process* Process_copy(Process* self) {
    Process* spite_copy = Process_allocate();
    SpiteString_release(spite_copy->command);
    spite_copy->command = SpiteString_retain(self->command);
    List_String_release(spite_copy->arguments);
    spite_copy->arguments = List_String_retain(self->arguments);
    SpiteString_release(spite_copy->last_output);
    spite_copy->last_output = SpiteString_retain(self->last_output);
    return spite_copy;
}

Process* Process_deep_copy(Process* self) {
    Process* spite_copy = Process_allocate();
    SpiteString_release(spite_copy->command);
    spite_copy->command = spite_string_from_bytes((self->command)->data, (self->command)->length);
    List_String_release(spite_copy->arguments);
    spite_copy->arguments = List_String_retain(self->arguments);
    SpiteString_release(spite_copy->last_output);
    spite_copy->last_output = spite_string_from_bytes((self->last_output)->data, (self->last_output)->length);
    return spite_copy;
}

void Process_Process(Process* self, SpiteString* command, List_String* arguments) {
    SpiteString_release(self->command);
    self->command = SpiteString_retain(command);
    SpiteString_release(command);
    List_String_release(self->arguments);
    self->arguments = List_String_retain(arguments);
    List_String_release(arguments);
    self->last_output = (&spite_static_string_empty);
}

int32_t Process_run(Process* self) {
    /* A command that is the path of an existing file is quoted, so a path with spaces works; anything else is
       passed as written, so a command may carry its own arguments ("clang -Wno-deprecated-declarations"). */
    bool spite_command_is_path = strchr(self->command->data, ' ') != 0 && spite_file_exists(self->command);
    SpiteString* spite_process_command = spite_command_is_path ? spite_string_from_cstring_owned("\"") : spite_string_from_bytes(self->command->data, self->command->length);
    if (spite_command_is_path) {
        SpiteString* spite_quoted_start = SpiteString_concat(spite_process_command, self->command);
        SpiteString_release(spite_process_command);
        SpiteString* spite_quote_end = spite_string_from_cstring_owned("\"");
        spite_process_command = SpiteString_concat(spite_quoted_start, spite_quote_end);
        SpiteString_release(spite_quoted_start);
        SpiteString_release(spite_quote_end);
    }
    for (int64_t spite_index = 0; spite_index < self->arguments->count; spite_index = spite_index + 1) {
        SpiteString* spite_open_quote = spite_string_from_cstring_owned(" \"");
        SpiteString* spite_with_space = SpiteString_concat(spite_process_command, spite_open_quote);
        SpiteString_release(spite_process_command);
        SpiteString_release(spite_open_quote);
        SpiteString* spite_with_argument = SpiteString_concat(spite_with_space, self->arguments->items[spite_index]);
        SpiteString_release(spite_with_space);
        SpiteString* spite_close_quote = spite_string_from_cstring_owned("\"");
        spite_process_command = SpiteString_concat(spite_with_argument, spite_close_quote);
        SpiteString_release(spite_with_argument);
        SpiteString_release(spite_close_quote);
    }
#ifdef _WIN32
    if (spite_command_is_path) {
        /* cmd.exe strips one outer pair of quotes from a line that starts with one, so give it a pair to strip. */
        SpiteString* spite_outer_open = spite_string_from_cstring_owned("\"");
        SpiteString* spite_outer_joined = SpiteString_concat(spite_outer_open, spite_process_command);
        SpiteString_release(spite_outer_open);
        SpiteString_release(spite_process_command);
        SpiteString* spite_outer_close = spite_string_from_cstring_owned("\"");
        spite_process_command = SpiteString_concat(spite_outer_joined, spite_outer_close);
        SpiteString_release(spite_outer_joined);
        SpiteString_release(spite_outer_close);
    }
#endif
    int spite_process_exit_code = 0;
    SpiteString* spite_process_output = spite_process_run(spite_process_command->data, &spite_process_exit_code);
    SpiteString_release(spite_process_command);
    SpiteString_release(self->last_output);
    self->last_output = spite_process_output;
    return (int32_t)spite_process_exit_code;
}

SpiteString* Process_output(Process* self) { return SpiteString_retain(self->last_output); }

void Program_init(Program* self) {
}

Program* Program_allocate(void) {
    Program* self = (Program*)SPITE_MALLOC(sizeof(Program));
    self->header.ref_count = 1;
    self->header.class_id = SPITE_CLASS_ID_PROGRAM;
    #ifdef SPITE_DEBUG_MEMORY
    spite_debug_register_object(self, SPITE_CLASS_ID_PROGRAM);
    #endif
    Program_init(self);
    return self;
}

Program* Program_default(void) {
    return Program_allocate();
}

Program* Program_make(void) {
    Program* self = Program_allocate();
    return self;
}

Program* Program_retain(Program* self) {
    if (self != 0) self->header.ref_count = self->header.ref_count + 1;
    return self;
}

void Program_release(Program* self) {
    if (self == 0) return;
    self->header.ref_count = self->header.ref_count - 1;
    if (self->header.ref_count > 0) return;
    SPITE_FREE(self);
}

Program* Program_copy(Program* self) {
    Program* spite_copy = Program_allocate();
    return spite_copy;
}

Program* Program_deep_copy(Program* self) {
    Program* spite_copy = Program_allocate();
    return spite_copy;
}

void Program_exit(Program* self, int32_t code) {
    (void)self;
    exit((int)code);
}

void Program_sleep(Program* self, int32_t milliseconds) {
    (void)self;
    spite_sleep_milliseconds((int64_t)milliseconds);
}

SpiteString* Program_environment(Program* self, SpiteString* name) {
    (void)self;
    const char* spite_environment_value = getenv(name->data);
    SpiteString_release(name);
    if (spite_environment_value == 0) return 0;
    return spite_string_from_cstring_owned(spite_environment_value);
}

int32_t Program_live_allocations(Program* self) {
    (void)self;
    return (int32_t)spite_live_allocation_count();
}

void Console_init(Console* self) {
}

Console* Console_allocate(void) {
    Console* self = (Console*)SPITE_MALLOC(sizeof(Console));
    self->header.ref_count = 1;
    self->header.class_id = SPITE_CLASS_ID_CONSOLE;
    #ifdef SPITE_DEBUG_MEMORY
    spite_debug_register_object(self, SPITE_CLASS_ID_CONSOLE);
    #endif
    Console_init(self);
    return self;
}

Console* Console_default(void) {
    return Console_allocate();
}

Console* Console_make(void) {
    Console* self = Console_allocate();
    return self;
}

Console* Console_retain(Console* self) {
    if (self != 0) self->header.ref_count = self->header.ref_count + 1;
    return self;
}

void Console_release(Console* self) {
    if (self == 0) return;
    self->header.ref_count = self->header.ref_count - 1;
    if (self->header.ref_count > 0) return;
    SPITE_FREE(self);
}

Console* Console_copy(Console* self) {
    Console* spite_copy = Console_allocate();
    return spite_copy;
}

Console* Console_deep_copy(Console* self) {
    Console* spite_copy = Console_allocate();
    return spite_copy;
}

SpiteString* Console_read_line(Console* self) {
    (void)self;
    int64_t spite_capacity = 128;
    int64_t spite_length = 0;
    char* spite_buffer = (char*)SPITE_MALLOC((size_t)spite_capacity);
    while (1) {
        int spite_character = fgetc(stdin);
        if (spite_character == EOF) {
            if (spite_length == 0) {
                SPITE_FREE(spite_buffer);
                return 0;
            }
            break;
        }
        if (spite_character == '\n') break;
        if (spite_length + 1 >= spite_capacity) {
            spite_capacity = spite_capacity * 2;
            char* spite_grown = (char*)SPITE_MALLOC((size_t)spite_capacity);
            memcpy(spite_grown, spite_buffer, (size_t)spite_length);
            SPITE_FREE(spite_buffer);
            spite_buffer = spite_grown;
        }
        spite_buffer[spite_length] = (char)spite_character;
        spite_length = spite_length + 1;
    }
    if (spite_length > 0 && spite_buffer[spite_length - 1] == '\r') spite_length = spite_length - 1;
    SpiteString* spite_line = spite_string_from_bytes(spite_buffer, spite_length);
    SPITE_FREE(spite_buffer);
    return spite_line;
}

void DynamicLibrary_init(DynamicLibrary* self) {
    self->file_name = (&spite_static_string_empty);
    self->handle = 0;
}

DynamicLibrary* DynamicLibrary_allocate(void) {
    DynamicLibrary* self = (DynamicLibrary*)SPITE_MALLOC(sizeof(DynamicLibrary));
    self->header.ref_count = 1;
    self->header.class_id = SPITE_CLASS_ID_DYNAMICLIBRARY;
    #ifdef SPITE_DEBUG_MEMORY
    spite_debug_register_object(self, SPITE_CLASS_ID_DYNAMICLIBRARY);
    #endif
    DynamicLibrary_init(self);
    return self;
}

DynamicLibrary* DynamicLibrary_default(void) {
    return DynamicLibrary_allocate();
}

/* A file name with no extension gets the platform's own, so one Spite source names a library on every
 * platform. A library that cannot be opened stops the program, naming the file. */
DynamicLibrary* DynamicLibrary_make(SpiteString* file_name) {
    DynamicLibrary* self = DynamicLibrary_allocate();
    self->file_name = file_name;
    const char* base = file_name->data;
    for (const char* cursor = file_name->data; *cursor != '\0'; cursor = cursor + 1) {
        if (*cursor == '/' || *cursor == '\\') base = cursor + 1;
    }
    char path[1024];
#ifdef _WIN32
    const char* extension = ".dll";
#elif defined(__APPLE__)
    const char* extension = ".dylib";
#else
    const char* extension = ".so";
#endif
    snprintf(path, sizeof(path), "%s%s", file_name->data, strchr(base, '.') == 0 ? extension : "");
#ifdef _WIN32
    self->handle = (void*)LoadLibraryA(path);
#else
    self->handle = dlopen(path, RTLD_NOW);
#endif
    if (self->handle == 0) {
        fflush(stdout);
        fprintf(stderr, "spite: could not open the library '%s'\n", path);
        exit(1);
    }
    return self;
}

DynamicLibrary* DynamicLibrary_retain(DynamicLibrary* self) {
    if (self != 0) self->header.ref_count = self->header.ref_count + 1;
    return self;
}

void DynamicLibrary_release(DynamicLibrary* self) {
    if (self == 0) return;
    self->header.ref_count = self->header.ref_count - 1;
    if (self->header.ref_count > 0) return;
    if (self->handle != 0) {
#ifdef _WIN32
        FreeLibrary((HMODULE)self->handle);
#else
        dlclose(self->handle);
#endif
    }
    SpiteString_release(self->file_name);
    SPITE_FREE(self);
}

DynamicLibrary* DynamicLibrary_copy(DynamicLibrary* self) {
    return DynamicLibrary_retain(self);
}

DynamicLibrary* DynamicLibrary_deep_copy(DynamicLibrary* self) {
    return DynamicLibrary_retain(self);
}

/* Resolved once, when the library is opened, for every symbol the program calls. A missing one stops the
 * program, naming the library, the symbol and the Spite function that wanted it. */
void* DynamicLibrary_symbol(DynamicLibrary* self, const char* name, const char* wanted_by) {
#ifdef _WIN32
    void* found = (void*)GetProcAddress((HMODULE)self->handle, name);
#else
    void* found = dlsym(self->handle, name);
#endif
    if (found == 0) {
        fflush(stdout);
        fprintf(stderr, "spite: the library '%s' has no '%s', which %s calls\n", self->file_name->data, name, wanted_by);
        exit(1);
    }
    return found;
}
