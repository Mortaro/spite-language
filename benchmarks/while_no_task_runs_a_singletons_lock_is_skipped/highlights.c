/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Naive_add_many(Naive* self, int32_t count_) {
    int32_t index_ = 0;
    int32_t spite_coarse_1_skipping = spite_coarse_1_skip_enter();
    while (((index_ < count_))) {
        index_ = (spite_coarse_1_skipping ? Tally_add___unguarded(self->tally_, index_) : Tally_add(self->tally_, index_));
    }
    spite_coarse_1_skip_leave(spite_coarse_1_skipping);
    int32_t spite_temp_1 = index_;
    return spite_temp_1;
}

int32_t Tally_add(Tally* self, int32_t amount_) {
    if (spite_skipped_depth < 16 && __atomic_load_n(&spite_tasks_in_flight, __ATOMIC_ACQUIRE) == 0) {
        int32_t spite_skip = spite_skipped_depth;
        spite_skipped[spite_skip] = (void*)&Tally___guard;
        spite_skipped_taken[spite_skip] = 0;
        spite_skipped_depth = spite_skip + 1;
        SPITE_GUARDS_COUNT(1);
        int32_t spite_unshared = Tally_add___unguarded(self, amount_);
        spite_skipped_depth = spite_skip;
        if (spite_skipped_taken[spite_skip]) spite_guard_leave(&Tally___guard);
        SPITE_GUARDS_COUNT(-1);
        return spite_unshared;
    }
    spite_guard_enter(&Tally___guard);
    SPITE_GUARDS_COUNT(1);
    int32_t spite_guarded = Tally_add___unguarded(self, amount_);
    spite_guard_leave(&Tally___guard);
    SPITE_GUARDS_COUNT(-1);
    return spite_guarded;
}

int32_t Tally_add___unguarded(Tally* self, int32_t amount_) {
    do { int64_t spite_step = (SpiteInteger_to_long(amount_)); int64_t spite_before; int64_t spite_after; if (Tally___atomic) spite_before = __atomic_fetch_add(&(self->total_), spite_step, __ATOMIC_SEQ_CST); else spite_before = (self->total_); if (__builtin_expect(__builtin_add_overflow(spite_before, spite_step, &spite_after), 0)) spite_overflowed("total + amount", "a Long", "+", (int64_t)spite_before, (int64_t)spite_step, spite_site_1()); if (!Tally___atomic) (self->total_) = spite_after; } while (0);
    do { int32_t spite_step = (1); int32_t spite_before; int32_t spite_after; if (Tally___atomic) spite_before = __atomic_fetch_add(&(self->calls_), spite_step, __ATOMIC_SEQ_CST); else spite_before = (self->calls_); if (__builtin_expect(__builtin_add_overflow(spite_before, spite_step, &spite_after), 0)) spite_overflowed("calls + 1", "an Integer", "+", (int64_t)spite_before, (int64_t)spite_step, spite_site_2()); if (!Tally___atomic) (self->calls_) = spite_after; } while (0);
    int32_t spite_temp_2 = ({ int32_t spite_temp_3 = amount_; int32_t spite_temp_4 = 1; int32_t spite_temp_5; if (__builtin_expect(__builtin_add_overflow(spite_temp_3, spite_temp_4, &spite_temp_5), 0)) spite_overflowed("amount + 1", "an Integer", "+", (int64_t)spite_temp_3, (int64_t)spite_temp_4, spite_site_3()); spite_temp_5; });
    return spite_temp_2;
}

void ThreadPool__task_begun(ThreadPool* self) {
    #ifdef SPITE_THREADS
    for (int32_t spite_index = 0; spite_index < spite_skipped_depth; spite_index++) { if (!spite_skipped_taken[spite_index]) { spite_enter_skipped(spite_skipped[spite_index]); spite_skipped_taken[spite_index] = 1; } }
    __atomic_fetch_add(&spite_tasks_in_flight, 1, __ATOMIC_SEQ_CST);
    #endif
}

void ThreadPool__task_ended(ThreadPool* self) {
    #ifdef SPITE_THREADS
    __atomic_fetch_sub(&spite_tasks_in_flight, 1, __ATOMIC_RELEASE);
    #endif
}
