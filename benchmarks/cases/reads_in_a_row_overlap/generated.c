/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Naive_read_both___held_0_1(Naive* self, File* first_file_, File* second_file_) {
    Concurrent__Nullable_String* first_ = Concurrent__Nullable_String___make(spite_function_value_File_read(first_file_));
    SpiteString second_ = File_read(second_file_);
    Concurrent__Nullable_String__join(first_);
    SpiteString spite_temp_1_ = Concurrent__Nullable_String__result(first_);
    if (!((!SPITE_STRING_IS_NULL(spite_temp_1_)))) {
        spite_failed_1(second_);
    }
    if (!((!SPITE_STRING_IS_NULL(second_)))) {
        spite_failed_2(spite_temp_1_);
    }
    int32_t spite_temp_2 = ({ int32_t spite_temp_3 = SpiteString_length(spite_temp_1_); int32_t spite_temp_4 = SpiteString_length(second_); int32_t spite_temp_5; if (__builtin_expect(__builtin_add_overflow(spite_temp_3, spite_temp_4, &spite_temp_5), 0)) spite_overflowed("first.length() + second.length()", "an Integer", "+", (int64_t)spite_temp_3, (int64_t)spite_temp_4, spite_site_1()); spite_temp_5; });
    SpiteString___release(spite_temp_1_);
    SpiteString___release(second_);
    Concurrent__Nullable_String___release(first_);
    return spite_temp_2;
}

static bool File_read___step(void* spite_raw) {
    File_read___frame* spite_frame = (File_read___frame*)spite_raw;
    File* self = spite_frame->self;
    switch (spite_frame->spite_head.state) { case 2: goto spite_resume_1; default: break; }
    spite_frame->handle_ = File_open_file(self, spite_lit_1);
    if (!(((spite_frame->handle_ != SpiteInteger_to_long(0))))) {
        { spite_frame->spite_result = SPITE_STRING_NULL; return true; }
    }
    File_seek(self, spite_frame->handle_, SpiteInteger_to_long(0), 2);
    spite_frame->length_ = File_tell(self, spite_frame->handle_);
    File_seek(self, spite_frame->handle_, SpiteInteger_to_long(0), 0);
    spite_frame->buffer_ = Memory_Heap_allocate(self->heap_, ({ int64_t spite_temp_6 = spite_frame->length_; int64_t spite_temp_7 = SpiteInteger_to_long(1); int64_t spite_temp_8; if (__builtin_expect(__builtin_add_overflow(spite_temp_6, spite_temp_7, &spite_temp_8), 0)) spite_overflowed("length + 1", "a Long", "+", (int64_t)spite_temp_6, (int64_t)spite_temp_7, spite_site_2()); spite_temp_8; }));
    spite_frame->spite_wait_1 = File_read_into___begin(self, SpiteMemory_Address_to_long(spite_frame->buffer_), spite_frame->length_, spite_frame->handle_);
    spite_resume_1: if (!File_read_into___step(spite_frame->spite_wait_1)) SPITE_SUSPEND(2);
    spite_frame->spite_temp_9 = ((File_read_into___frame*)spite_frame->spite_wait_1)->spite_result;
    SPITE_FREE(spite_frame->spite_wait_1);
    spite_frame->got_ = spite_frame->spite_temp_9;
    File_close_file(self, spite_frame->handle_);
    spite_frame->content_ = SpiteMemory_Address_text(spite_frame->buffer_, spite_frame->got_);
    Memory_Heap_free(self->heap_, spite_frame->buffer_);
    spite_frame->spite_temp_10 = SpiteString___retain(spite_frame->content_);
    SpiteString___release(spite_frame->content_);
    { spite_frame->spite_result = spite_frame->spite_temp_10; return true; }
    return true;
}

static bool File_read_into___step(void* spite_raw) {
    File_read_into___frame* spite_frame = (File_read_into___frame*)spite_raw;
    Scheduler* spite_scheduler = spite_singleton_Scheduler();
    if (spite_frame->spite_head.state == 0) {
        spite_frame->spite_call = (File_read_into___call){ { 0, &File_read_into___perform, spite_scheduler }, spite_frame->self, spite_frame->buffer_, spite_frame->bytes_, spite_frame->handle_ };
        spite_frame->spite_thread = Scheduler_offload_start(spite_scheduler, (int64_t)(intptr_t)&spite_frame->spite_call, (int64_t)(intptr_t)&spite_offload_thread);
        spite_frame->spite_head.state = 1;
    }
    bool spite_over = Scheduler_offload_over(spite_scheduler, (int64_t)(intptr_t)&spite_frame->spite_call, spite_frame->spite_thread);
    if (spite_over) spite_frame->spite_result = spite_frame->spite_call.result;
    Scheduler___release(spite_scheduler);
    return spite_over;
}

int64_t File_read_into(File* self, int64_t buffer_, int64_t bytes_, int64_t handle_) {
    Scheduler* spite_scheduler = spite_singleton_Scheduler();
    if (SPITE_GUARDS_HELD() != 0 || !Scheduler_waits_here(spite_scheduler)) { Scheduler___release(spite_scheduler); return File_read_into___waiting(self, buffer_, bytes_, handle_); }
    File_read_into___call spite_call = { { 0, &File_read_into___perform, spite_scheduler }, self, buffer_, bytes_, handle_ };
    if (!Scheduler_offload(spite_scheduler, (int64_t)(intptr_t)&spite_call, (int64_t)(intptr_t)&spite_offload_thread)) { Scheduler___release(spite_scheduler); return File_read_into___waiting(self, buffer_, bytes_, handle_); }
    Scheduler___release(spite_scheduler);
    return spite_call.result;
}
