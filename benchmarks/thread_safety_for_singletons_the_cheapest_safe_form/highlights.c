/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

#define HitCounter___atomic 1

#define Settings___atomic 0

int32_t Worker_run(Worker* self) {
    int32_t index_ = 0;
    while (((index_ < self->rounds_))) {
        int32_t step_ = Settings_step_size(self->settings_);
        HitCounter_record(self->counter_, step_);
        index_ = (index_ + 1);
    }
    int32_t spite_temp_1 = index_;
    return spite_temp_1;
}

void HitCounter_record(HitCounter* self, int32_t amount_) {
    do { int32_t spite_step = (amount_); int32_t spite_before; int32_t spite_after; if (HitCounter___atomic) spite_before = __atomic_fetch_add(&(self->hits_), spite_step, __ATOMIC_SEQ_CST); else spite_before = (self->hits_); if (__builtin_expect(__builtin_add_overflow(spite_before, spite_step, &spite_after), 0)) spite_overflowed("hits + amount", "an Integer", "+", (int64_t)spite_before, (int64_t)spite_step, spite_site_1()); if (!HitCounter___atomic) (self->hits_) = spite_after; } while (0);
}

int32_t Settings_step_size(Settings* self) {
    int32_t spite_temp_2 = SPITE_SINGLETON_LOAD(Settings, self->step_);
    return spite_temp_2;
}

void Journal_note(Journal* self, SpiteString line_) {
    List_String_append(self->lines_, SpiteString___retain(line_));
    SpiteString___release(line_);
}
