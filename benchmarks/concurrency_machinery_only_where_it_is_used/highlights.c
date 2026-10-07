/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Naive_write_and_read___held_0(Naive* self, File* notes_, int32_t rounds_) {
    int32_t characters_ = 0;
    int32_t round_ = 0;
    while (((round_ < rounds_))) {
        (void)(File_write(notes_, ({ char spite_temp_1_digits[24]; SpiteString spite_temp_1 = SPITE_STATIC_STRING(spite_temp_1_digits, spite_long_digits(spite_temp_1_digits, (int64_t)(round_))); SpiteString spite_temp_2[] = {spite_lit_1, spite_temp_1, spite_lit_2}; SpiteString spite_temp_3 = spite_string_join(3, spite_temp_2); spite_temp_3; })));
        SpiteString read_ = File_read(notes_);
        if (!((!SPITE_STRING_IS_NULL(read_)))) {
            spite_failed_1(rounds_, characters_, round_);
        }
        characters_ = ({ int32_t spite_temp_4 = characters_; int32_t spite_temp_5 = SpiteString_length(read_); int32_t spite_temp_6; if (__builtin_expect(__builtin_add_overflow(spite_temp_4, spite_temp_5, &spite_temp_6), 0)) spite_overflowed("characters + read.length()", "an Integer", "+", (int64_t)spite_temp_4, (int64_t)spite_temp_5, spite_site_1()); spite_temp_6; });
        round_ = (round_ + 1);
        SpiteString___release(read_);
    }
    int32_t spite_temp_7 = characters_;
    return spite_temp_7;
}

SpiteString File_read(File* self) {
    int64_t handle_ = File_open_file(self, spite_lit_3);
    if (!(((handle_ != SpiteInteger_to_long(0))))) {
        return SPITE_STRING_NULL;
    }
    File_seek(self, handle_, SpiteInteger_to_long(0), 2);
    int64_t length_ = File_tell(self, handle_);
    File_seek(self, handle_, SpiteInteger_to_long(0), 0);
    int64_t buffer_ = Memory_Heap_allocate(self->heap_, ({ int64_t spite_temp_8 = length_; int64_t spite_temp_9 = SpiteInteger_to_long(1); int64_t spite_temp_10; if (__builtin_expect(__builtin_add_overflow(spite_temp_8, spite_temp_9, &spite_temp_10), 0)) spite_overflowed("length + 1", "a Long", "+", (int64_t)spite_temp_8, (int64_t)spite_temp_9, spite_site_2()); spite_temp_10; }));
    int64_t got_ = File_read_into(self, SpiteMemory_Address_to_long(buffer_), length_, handle_);
    File_close_file(self, handle_);
    SpiteString content_ = SpiteMemory_Address_text(buffer_, got_);
    Memory_Heap_free(self->heap_, buffer_);
    SpiteString spite_temp_11 = SpiteString___retain(content_);
    SpiteString___release(content_);
    return spite_temp_11;
}

int64_t File_open_file(File* self, SpiteString mode_) {
    int64_t spite_temp_12 = ({ SpiteString spite_temp_13 = self->path_; SpiteString spite_temp_14 = mode_; spite_last_foreign_call = "fopen\tlibrary=ucrtbase.dll\tfrom=library/windows/file.spite:5"; int64_t spite_temp_15 = ((int64_t (*)(const char*, const char*))spite_foreign_1_11)(spite_string_bytes(&spite_temp_13), spite_string_bytes(&spite_temp_14));  int64_t spite_foreign_result = spite_temp_15;  (void)spite_foreign_result; spite_temp_15; });
    SpiteString___release(mode_);
    return spite_temp_12;
}

int64_t File_read_into(File* self, int64_t buffer_, int64_t bytes_, int64_t handle_) {
    int64_t spite_temp_16 = ({ spite_last_foreign_call = "fread\tlibrary=ucrtbase.dll\tfrom=library/windows/file.spite:21"; int64_t spite_temp_17 = ((int64_t (*)(int64_t, int64_t, int64_t, int64_t))spite_foreign_1_15)((int64_t)(buffer_), (int64_t)(1), (int64_t)(bytes_), (int64_t)(handle_));  int64_t spite_foreign_result = spite_temp_17;  (void)spite_foreign_result; spite_temp_17; });
    return spite_temp_16;
}
