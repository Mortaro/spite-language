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

/* The platform this program was compiled for: a fact of the build, so it is part of the floor. */
SpiteString* Program_platform(Program* self) {
    (void)self;
#ifdef _WIN32
    return spite_string_from_cstring_owned("windows");
#elif defined(__APPLE__)
    return spite_string_from_cstring_owned("mac");
#else
    return spite_string_from_cstring_owned("linux");
#endif
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

/* Opens the file exactly as named (D71: a Spite wrapper names the real file). A library that cannot be
 * opened stops the program, naming the file. */
DynamicLibrary* DynamicLibrary_make(SpiteString* file_name) {
    DynamicLibrary* self = DynamicLibrary_allocate();
    self->file_name = file_name;
    const char* path = file_name->data;
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

/* The floor of a standard library written in Spite (manual.md section 15, "The floor, named"): raw memory,
 * addressed by a Long, through the same allocator every Spite object uses, so --debug-memory counts it. */
void Memory_init(Memory* self) {
    (void)self;
}

Memory* Memory_allocate(void) {
    Memory* self = (Memory*)SPITE_MALLOC(sizeof(Memory));
    self->header.ref_count = 1;
    self->header.class_id = SPITE_CLASS_ID_MEMORY;
    #ifdef SPITE_DEBUG_MEMORY
    spite_debug_register_object(self, SPITE_CLASS_ID_MEMORY);
    #endif
    return self;
}

Memory* Memory_default(void) { return Memory_allocate(); }
Memory* Memory_make(void) { return Memory_allocate(); }
Memory* Memory_retain(Memory* self) { if (self != 0) self->header.ref_count = self->header.ref_count + 1; return self; }
void Memory_release(Memory* self) {
    if (self == 0) return;
    self->header.ref_count = self->header.ref_count - 1;
    if (self->header.ref_count > 0) return;
    SPITE_FREE(self);
}
Memory* Memory_copy(Memory* self) { return Memory_retain(self); }
Memory* Memory_deep_copy(Memory* self) { return Memory_retain(self); }
int64_t Memory_allocate_bytes(Memory* self, int64_t bytes) { (void)self; return (int64_t)(intptr_t)SPITE_MALLOC((size_t)bytes); }
int64_t Memory_resize(Memory* self, int64_t address, int64_t bytes) { (void)self; return (int64_t)(intptr_t)SPITE_REALLOC((void*)(intptr_t)address, (size_t)bytes); }
void Memory_free(Memory* self, int64_t address) { (void)self; SPITE_FREE((void*)(intptr_t)address); }
uint8_t Memory_read_byte(Memory* self, int64_t address, int64_t offset) { (void)self; return ((uint8_t*)(intptr_t)address)[offset]; }
int32_t Memory_read_int(Memory* self, int64_t address, int64_t offset) { (void)self; int32_t value; memcpy(&value, (char*)(intptr_t)address + offset, sizeof(value)); return value; }
int64_t Memory_read_long(Memory* self, int64_t address, int64_t offset) { (void)self; int64_t value; memcpy(&value, (char*)(intptr_t)address + offset, sizeof(value)); return value; }
double Memory_read_double(Memory* self, int64_t address, int64_t offset) { (void)self; double value; memcpy(&value, (char*)(intptr_t)address + offset, sizeof(value)); return value; }
void Memory_write_byte(Memory* self, int64_t address, int64_t offset, uint8_t value) { (void)self; ((uint8_t*)(intptr_t)address)[offset] = value; }
void Memory_write_int(Memory* self, int64_t address, int64_t offset, int32_t value) { (void)self; memcpy((char*)(intptr_t)address + offset, &value, sizeof(value)); }
void Memory_write_long(Memory* self, int64_t address, int64_t offset, int64_t value) { (void)self; memcpy((char*)(intptr_t)address + offset, &value, sizeof(value)); }
void Memory_write_double(Memory* self, int64_t address, int64_t offset, double value) { (void)self; memcpy((char*)(intptr_t)address + offset, &value, sizeof(value)); }
void Memory_copy_bytes(Memory* self, int64_t from, int64_t to, int64_t bytes) { (void)self; memmove((void*)(intptr_t)to, (void*)(intptr_t)from, (size_t)bytes); }
SpiteString* Memory_text(Memory* self, int64_t address, int64_t length) { (void)self; return spite_string_from_bytes((const char*)(intptr_t)address, length); }
