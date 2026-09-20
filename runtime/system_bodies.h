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
    SpiteString* spite_process_command = spite_string_from_bytes(self->command->data, self->command->length);
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
