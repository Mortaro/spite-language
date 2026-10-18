/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

typedef struct Napper_nap___frame { SpiteFrame spite_head; int32_t spite_result; Napper* self; Program* spite_temp_1; Program* spite_temp_2; bool spite_temp_3; int32_t doubled_; Program* spite_temp_4; Program* spite_temp_5; bool spite_temp_6; int32_t spite_temp_7; void* spite_wait_1; void* spite_wait_2; } Napper_nap___frame;

static void* Napper_nap___begin(Napper* self) {
    Napper_nap___frame* spite_frame = (Napper_nap___frame*)SPITE_MALLOC(sizeof(Napper_nap___frame));
    memset(spite_frame, 0, sizeof(Napper_nap___frame));
    spite_frame->spite_head.step = Napper_nap___step;
    spite_frame->self = self;
    return spite_frame;
}

static bool Napper_nap___step(void* spite_raw) {
    Napper_nap___frame* spite_frame = (Napper_nap___frame*)spite_raw;
    Napper* self = spite_frame->self;
    switch (spite_frame->spite_head.state) { case 1: goto spite_resume_1; case 2: goto spite_resume_2; default: break; }
    spite_frame->spite_temp_1 = Program___retain(self->program_);
    spite_frame->spite_temp_2 = Program___retain(spite_frame->spite_temp_1);
    spite_frame->spite_wait_1 = Program_sleep___begin(spite_frame->spite_temp_2, 2);
    spite_resume_1: if (!Program_sleep___step(spite_frame->spite_wait_1)) SPITE_SUSPEND(1);
    spite_frame->spite_temp_3 = true;
    SPITE_FREE(spite_frame->spite_wait_1);
    Program___release(spite_frame->spite_temp_2);
    ({ (void)spite_frame->spite_temp_3; Program___release(spite_frame->spite_temp_1); });
    spite_frame->doubled_ = ({ int32_t spite_temp_8 = self->number_; int32_t spite_temp_9 = 2; int32_t spite_temp_10; if (__builtin_expect(__builtin_mul_overflow(spite_temp_8, spite_temp_9, &spite_temp_10), 0)) spite_overflowed("number * 2", "an Integer", "*", (int64_t)spite_temp_8, (int64_t)spite_temp_9, spite_site_1()); spite_temp_10; });
    spite_frame->spite_temp_4 = Program___retain(self->program_);
    spite_frame->spite_temp_5 = Program___retain(spite_frame->spite_temp_4);
    spite_frame->spite_wait_2 = Program_sleep___begin(spite_frame->spite_temp_5, 2);
    spite_resume_2: if (!Program_sleep___step(spite_frame->spite_wait_2)) SPITE_SUSPEND(2);
    spite_frame->spite_temp_6 = true;
    SPITE_FREE(spite_frame->spite_wait_2);
    Program___release(spite_frame->spite_temp_5);
    ({ (void)spite_frame->spite_temp_6; Program___release(spite_frame->spite_temp_4); });
    spite_frame->spite_temp_7 = ({ int32_t spite_temp_11 = spite_frame->doubled_; int32_t spite_temp_12 = 1; int32_t spite_temp_13; if (__builtin_expect(__builtin_add_overflow(spite_temp_11, spite_temp_12, &spite_temp_13), 0)) spite_overflowed("doubled + 1", "an Integer", "+", (int64_t)spite_temp_11, (int64_t)spite_temp_12, spite_site_2()); spite_temp_13; });
    { spite_frame->spite_result = spite_frame->spite_temp_7; return true; }
    return true;
}

static bool Program_sleep___step(void* spite_raw) {
    Program_sleep___frame* spite_frame = (Program_sleep___frame*)spite_raw;
    Scheduler* spite_scheduler = spite_singleton_Scheduler();
    if (spite_frame->spite_head.state == 0) { spite_frame->spite_deadline = Scheduler_timer_start(spite_scheduler, spite_frame->milliseconds_); spite_frame->spite_head.state = 1; }
    bool spite_over = Scheduler_timer_over(spite_scheduler, spite_frame->spite_deadline);
    Scheduler___release(spite_scheduler);
    return spite_over;
}

int32_t Naive_nap_all(Naive* self, int32_t count_) {
    List_Concurrent__Integer* nappings_ = List_Concurrent__Integer___make();
    int32_t index_ = 0;
    while (((index_ < count_))) {
        Napper* napper_ = Napper___make(index_);
        Concurrent__Integer* napping_ = Concurrent__Integer___make(spite_function_value_Napper_nap(napper_));
        List_Concurrent__Integer_append(nappings_, Concurrent__Integer___retain(napping_));
        index_ = (index_ + 1);
        Concurrent__Integer___release(napping_);
        Napper___release(napper_);
    }
    int32_t total_ = 0;
    int32_t read_ = 0;
    while (((read_ < spite_folded_List_Concurrent__Integer_count(nappings_)))) {
        int32_t napped_ = ({ Concurrent__Integer* spite_temp_14 = ({ Concurrent__Integer* spite_temp_15 = List_Concurrent__Integer_get_at(nappings_, read_); if (__builtin_expect(!(((spite_temp_15) != 0)), 0)) spite_outside_list("nappings[read]", spite_site_3()); spite_temp_15; }); int32_t spite_temp_16 = Concurrent__Integer__result(spite_temp_14); Concurrent__Integer___release(spite_temp_14); spite_temp_16; });
        total_ = ({ int32_t spite_temp_17 = total_; int32_t spite_temp_18 = napped_; int32_t spite_temp_19; if (__builtin_expect(__builtin_add_overflow(spite_temp_17, spite_temp_18, &spite_temp_19), 0)) spite_overflowed("total + napped", "an Integer", "+", (int64_t)spite_temp_17, (int64_t)spite_temp_18, spite_site_4()); spite_temp_19; });
        read_ = (read_ + 1);
    }
    int32_t spite_temp_20 = total_;
    List_Concurrent__Integer___release(nappings_);
    return spite_temp_20;
}
