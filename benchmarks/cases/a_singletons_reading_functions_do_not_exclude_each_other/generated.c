/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t System_run(System* self) {
    int32_t total_ = 0;
    int32_t row_ = 0;
    while (((row_ < self->rows_))) {
        Transform* transform_ = Transforms_at(self->transforms_, row_);
        total_ = ({ int32_t spite_temp_1 = total_; int32_t spite_temp_2 = (transform_)->across_; int32_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("total + transform.across", "an Integer", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
        row_ = (row_ + 1);
        Transform___release(transform_);
    }
    int32_t spite_temp_4 = total_;
    return spite_temp_4;
}

Transform* Transforms_at(Transforms* self, int32_t row_) {
    if (spite_skipped_depth < 16 && __atomic_load_n(&spite_tasks_in_flight, __ATOMIC_ACQUIRE) == 0) {
        int32_t spite_skip = spite_skipped_depth;
        spite_skipped[spite_skip] = (void*)&Transforms___guard;
        spite_skipped_taken[spite_skip] = 0;
        spite_skipped_depth = spite_skip + 1;
        SPITE_GUARDS_COUNT(1);
        Transform* spite_unshared = Transforms_at___unguarded(self, row_);
        spite_skipped_depth = spite_skip;
        if (spite_skipped_taken[spite_skip]) spite_guard_leave(&Transforms___guard);
        SPITE_GUARDS_COUNT(-1);
        return spite_unshared;
    }
    int64_t* spite_reading = spite_read_enter(&Transforms___guard, Transforms___readers);
    SPITE_GUARDS_COUNT(1);
    Transform* spite_guarded = Transforms_at___unguarded(self, row_);
    spite_read_leave(spite_reading);
    SPITE_GUARDS_COUNT(-1);
    return spite_guarded;
}

static int64_t* spite_read_enter(SpiteGuard* guard, SpiteReaders* readers) {
    int64_t spite_me = (int64_t)(intptr_t)&spite_guard_thread;
    if (__atomic_load_n(&guard->owner, __ATOMIC_ACQUIRE) == spite_me) return 0;
    if (spite_reader_index < 0) spite_reader_index = __atomic_fetch_add(&spite_reader_next, 1, __ATOMIC_RELAXED) % SPITE_READER_SLOTS;
    int64_t* spite_slot = &readers[spite_reader_index].count;
    for (;;) {
        __atomic_fetch_add(spite_slot, 1, __ATOMIC_SEQ_CST);
        if (__atomic_load_n(&guard->owner, __ATOMIC_SEQ_CST) == 0) return spite_slot;
        __atomic_fetch_sub(spite_slot, 1, __ATOMIC_SEQ_CST);
        while (__atomic_load_n(&guard->owner, __ATOMIC_RELAXED) != 0) { }
    }
}

static void spite_read_leave(int64_t* slot) {
    if (slot != 0) __atomic_fetch_sub(slot, 1, __ATOMIC_RELEASE);
}

void Transforms_insert___held_0(Transforms* self, Transform* transform_) {
    if (spite_skipped_depth < 16 && __atomic_load_n(&spite_tasks_in_flight, __ATOMIC_ACQUIRE) == 0) {
        int32_t spite_skip = spite_skipped_depth;
        spite_skipped[spite_skip] = (void*)&Transforms___guard;
        spite_skipped_taken[spite_skip] = 0;
        spite_skipped_depth = spite_skip + 1;
        SPITE_GUARDS_COUNT(1);
        Transforms_insert___held_0___unguarded(self, transform_);
        spite_skipped_depth = spite_skip;
        if (spite_skipped_taken[spite_skip]) spite_guard_leave(&Transforms___guard);
        SPITE_GUARDS_COUNT(-1);
        return;
    }
    spite_guard_enter_writing(&Transforms___guard, Transforms___readers);
    SPITE_GUARDS_COUNT(1);
    Transforms_insert___held_0___unguarded(self, transform_);
    spite_guard_leave(&Transforms___guard);
    SPITE_GUARDS_COUNT(-1);
}

static void spite_guard_enter_writing(SpiteGuard* guard, SpiteReaders* readers) {
    spite_guard_enter(guard);
    if (guard->depth != 1) return;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    for (int32_t spite_index = 0; spite_index < SPITE_READER_SLOTS; spite_index++) {
        while (__atomic_load_n(&readers[spite_index].count, __ATOMIC_SEQ_CST) != 0) { }
    }
}
