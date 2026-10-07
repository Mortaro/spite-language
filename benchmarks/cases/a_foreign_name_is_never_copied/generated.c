/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

DynamicLibrary* spite_foreign_library_1(void) {
    DynamicLibrary* found = SPITE_SINGLETON_FOUND(spite_foreign_library_1_cache);
    if (found != 0) return found;
    SPITE_LOCK(spite_foreign_library_1_lock);
    if (spite_foreign_library_1_cache == 0) {
        DynamicLibrary* made = DynamicLibrary___make(((SpiteString)SPITE_STATIC_STRING("kernel32.dll", 12)), spite_symbol_1, ((SpiteString)SPITE_STATIC_STRING("", 0)));
        spite_foreign_library_1_tracked = spite_singleton_tracked();
        (void)&DynamicLibrary_find_symbol;
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        spite_foreign_1_34 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("InitializeSRWLock", 17)), ((SpiteString)SPITE_STATIC_STRING("Lock.create_lock", 16)));
        spite_foreign_1_35 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("AcquireSRWLockExclusive", 23)), ((SpiteString)SPITE_STATIC_STRING("Lock.acquire", 12)));
        spite_foreign_1_36 = (void*)(intptr_t)DynamicLibrary_find_symbol(made, ((SpiteString)SPITE_STATIC_STRING("ReleaseSRWLockExclusive", 23)), ((SpiteString)SPITE_STATIC_STRING("Lock.release_lock", 17)));
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        SPITE_SINGLETON_PUBLISH(spite_foreign_library_1_cache, made);
    }
    SPITE_UNLOCK(spite_foreign_library_1_lock);
    return spite_foreign_library_1_cache;
}

DynamicLibrary* spite_foreign_library_2(void) {
    DynamicLibrary* found = SPITE_SINGLETON_FOUND(spite_foreign_library_2_cache);
    if (found != 0) return found;
    SPITE_LOCK(spite_foreign_library_2_lock);
    if (spite_foreign_library_2_cache == 0) {
        DynamicLibrary* made = DynamicLibrary___make(((SpiteString)SPITE_STATIC_STRING("ucrtbase.dll", 12)), spite_symbol_1, ((SpiteString)SPITE_STATIC_STRING("", 0)));
        spite_foreign_library_2_tracked = spite_singleton_tracked();
        (void)&DynamicLibrary_find_symbol;
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        SPITE_SINGLETON_PUBLISH(spite_foreign_library_2_cache, made);
    }
    SPITE_UNLOCK(spite_foreign_library_2_lock);
    return spite_foreign_library_2_cache;
}

int64_t DynamicLibrary_find_symbol(DynamicLibrary* self, SpiteString name_, SpiteString wanted_by_) {
    #ifdef _WIN32
    void* found = (void*)GetProcAddress((HMODULE)(intptr_t)self->handle_, spite_string_bytes(&name_));
    #else
    void* found = dlsym((void*)(intptr_t)self->handle_, spite_string_bytes(&name_));
    #endif
    if (found == 0) {
        fflush(stdout);
        fprintf(stderr, "spite: the library '%s' has no '%s', which %s calls\n", spite_string_bytes(&self->file_name_), spite_string_bytes(&name_), spite_string_bytes(&wanted_by_));
        exit(1);
    }
    SpiteString___release(name_);
    SpiteString___release(wanted_by_);
    return (int64_t)(intptr_t)found;
}
