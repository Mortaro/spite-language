/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Naive_run_frames(Naive* self) {
    while (((self->frames_ < 60))) {
        self->frames_ = ({ int32_t spite_temp_1 = self->frames_; int32_t spite_temp_2 = 1; int32_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("frames + 1", "an Integer", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
        Naive_draw(self);
        if (((self->frames_ <= 8))) {
            {
                Saver* spite_started_receiver_1_ = Saver___retain(self->saver_);
                
                Concurrent__Nothing* spite_started_1_0_ = Concurrent__Nothing___make(spite_function_value_Saver_save_part(spite_started_receiver_1_));
                WaitsInFlight__Nothing__keep(spite_singleton_WaitsInFlight__Nothing(), Concurrent__Nothing___retain(spite_started_1_0_), 1);
                Concurrent__Nothing___release(spite_started_1_0_);
                
                Saver___release(spite_started_receiver_1_);
            }
        }
        Program_sleep(self->program_, 1);
    }
    while ((((self->saver_)->saved_ < 8))) {
        Program_sleep(self->program_, 1);
    }
    int32_t spite_temp_4 = (self->saver_)->saved_;
    return spite_temp_4;
}

void WaitsInFlight__Nothing__keep(WaitsInFlight__Nothing* self, Concurrent__Nothing* started_, int32_t site_) {
    WaitsInFlight__Nothing__let_go_of_finished(self);
    if (!(((!(Concurrent__Nothing_get_finished(started_)))))) {
        Concurrent__Nothing___release(started_);
        return;
    }
    List_Concurrent__Nothing_append(self->_started_, Concurrent__Nothing___retain(started_));
    List_Integer_append(self->_sites_, site_);
    while (((WaitsInFlight__Nothing__in_flight_at(self, site_) > self->_bound_))) {
        WaitsInFlight__Nothing__wait_for_oldest_at(self, site_);
    }
    Concurrent__Nothing___release(started_);
}

static bool Saver_save_part___step(void* spite_raw) {
    Saver_save_part___frame* spite_frame = (Saver_save_part___frame*)spite_raw;
    Saver* self = spite_frame->self;
    switch (spite_frame->spite_head.state) { case 1: goto spite_resume_1; case 2: goto spite_resume_2; default: break; }
    spite_frame->part_ = self->next_part_;
    self->next_part_ = ({ int32_t spite_temp_5 = self->next_part_; int32_t spite_temp_6 = 1; int32_t spite_temp_7; if (__builtin_expect(__builtin_add_overflow(spite_temp_5, spite_temp_6, &spite_temp_7), 0)) spite_overflowed("next_part + 1", "an Integer", "+", (int64_t)spite_temp_5, (int64_t)spite_temp_6, spite_site_2()); spite_temp_7; });
    spite_frame->spite_temp_8 = Program___retain(self->program_);
    spite_frame->spite_temp_9 = Program___retain(spite_frame->spite_temp_8);
    spite_frame->spite_wait_1 = Program_sleep___begin(spite_frame->spite_temp_9, 20);
    spite_resume_1: if (!Program_sleep___step(spite_frame->spite_wait_1)) SPITE_SUSPEND(1);
    spite_frame->spite_temp_10 = true;
    SPITE_FREE(spite_frame->spite_wait_1);
    Program___release(spite_frame->spite_temp_9);
    ({ (void)spite_frame->spite_temp_10; Program___release(spite_frame->spite_temp_8); });
    spite_frame->file_ = File___make(({ char spite_temp_11_digits[24]; SpiteString spite_temp_11 = SPITE_STATIC_STRING(spite_temp_11_digits, spite_long_digits(spite_temp_11_digits, (int64_t)(spite_frame->part_))); SpiteString spite_temp_12[] = {spite_lit_1, spite_temp_11, spite_lit_2}; SpiteString spite_temp_13 = spite_string_join(3, spite_temp_12); spite_temp_13; }));
    spite_frame->spite_temp_14 = File___retain(spite_frame->file_);
    spite_frame->spite_wait_2 = File_write___begin(spite_frame->spite_temp_14, ({ char spite_temp_15_digits[24]; SpiteString spite_temp_15 = SPITE_STATIC_STRING(spite_temp_15_digits, spite_long_digits(spite_temp_15_digits, (int64_t)(spite_frame->part_))); SpiteString spite_temp_16[] = {spite_lit_3, spite_temp_15}; SpiteString spite_temp_17 = spite_string_join(2, spite_temp_16); spite_temp_17; }));
    spite_resume_2: if (!File_write___step(spite_frame->spite_wait_2)) SPITE_SUSPEND(2);
    spite_frame->spite_temp_18 = ((File_write___frame*)spite_frame->spite_wait_2)->spite_result;
    SPITE_FREE(spite_frame->spite_wait_2);
    File___release(spite_frame->spite_temp_14);
    (void)(spite_frame->spite_temp_18);
    self->saved_ = ({ int32_t spite_temp_19 = self->saved_; int32_t spite_temp_20 = 1; int32_t spite_temp_21; if (__builtin_expect(__builtin_add_overflow(spite_temp_19, spite_temp_20, &spite_temp_21), 0)) spite_overflowed("saved + 1", "an Integer", "+", (int64_t)spite_temp_19, (int64_t)spite_temp_20, spite_site_3()); spite_temp_21; });
    File___release(spite_frame->file_);
    return true;
}
