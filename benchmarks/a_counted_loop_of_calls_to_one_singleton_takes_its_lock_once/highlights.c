/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Counter_count_up(Counter* self) {
    int32_t index_ = 0;
    spite_coarse_1_enter();
    while (((index_ < self->rounds_))) {
        Tally_add___unguarded(self->tally_, (index_ % 3));
        index_ = (index_ + 1);
    }
    spite_coarse_1_leave();
    int32_t spite_temp_1 = self->rounds_;
    return spite_temp_1;
}

void Tally_add___unguarded(Tally* self, int32_t amount_) {
    do { int64_t spite_step = (SpiteInteger_to_long(amount_)); int64_t spite_before; int64_t spite_after; if (Tally___atomic) spite_before = __atomic_fetch_add(&(self->total_), spite_step, __ATOMIC_SEQ_CST); else spite_before = (self->total_); if (__builtin_expect(__builtin_add_overflow(spite_before, spite_step, &spite_after), 0)) spite_overflowed("total + amount", "a Long", "+", (int64_t)spite_before, (int64_t)spite_step, spite_site_1()); if (!Tally___atomic) (self->total_) = spite_after; } while (0);
    do { int32_t spite_step = (1); int32_t spite_before; int32_t spite_after; if (Tally___atomic) spite_before = __atomic_fetch_add(&(self->calls_), spite_step, __ATOMIC_SEQ_CST); else spite_before = (self->calls_); if (__builtin_expect(__builtin_add_overflow(spite_before, spite_step, &spite_after), 0)) spite_overflowed("calls + 1", "an Integer", "+", (int64_t)spite_before, (int64_t)spite_step, spite_site_2()); if (!Tally___atomic) (self->calls_) = spite_after; } while (0);
    if (((amount_ > SPITE_SINGLETON_LOAD(Tally, self->largest_)))) {
        SPITE_SINGLETON_STORE(Tally, self->largest_, amount_);
    }
}
