/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

typedef struct SpiteGuard { _Alignas(64) int64_t owner; int64_t depth; } SpiteGuard;

int32_t Recorder_record_all(Recorder* self) {
    int32_t index_ = 0;
    while (((index_ < self->rounds_))) {
        int32_t weight_ = Recorder_weight_of(self, index_);
        Registry_record(self->registry_, weight_);
        index_ = (index_ + 1);
    }
    int32_t spite_temp_1 = self->rounds_;
    return spite_temp_1;
}

void Registry_record(Registry* self, int32_t weight_) {
    if (spite_skipped_depth < 16 && __atomic_load_n(&spite_tasks_in_flight, __ATOMIC_ACQUIRE) == 0) {
        int32_t spite_skip = spite_skipped_depth;
        spite_skipped[spite_skip] = (void*)&Registry___guard;
        spite_skipped_taken[spite_skip] = 0;
        spite_skipped_depth = spite_skip + 1;
        SPITE_GUARDS_COUNT(1);
        Registry_record___unguarded(self, weight_);
        spite_skipped_depth = spite_skip;
        if (spite_skipped_taken[spite_skip]) spite_guard_leave(&Registry___guard);
        SPITE_GUARDS_COUNT(-1);
        return;
    }
    spite_guard_enter(&Registry___guard);
    SPITE_GUARDS_COUNT(1);
    Registry_record___unguarded(self, weight_);
    spite_guard_leave(&Registry___guard);
    SPITE_GUARDS_COUNT(-1);
}

void Registry_record___unguarded(Registry* self, int32_t weight_) {
    List_Integer_append(self->weights_, weight_);
    do { int64_t spite_step = (SpiteInteger_to_long(weight_)); int64_t spite_before; int64_t spite_after; if (Registry___atomic) spite_before = __atomic_fetch_add(&(self->total_), spite_step, __ATOMIC_SEQ_CST); else spite_before = (self->total_); if (__builtin_expect(__builtin_add_overflow(spite_before, spite_step, &spite_after), 0)) spite_overflowed("total + weight", "a Long", "+", (int64_t)spite_before, (int64_t)spite_step, spite_site_1()); if (!Registry___atomic) (self->total_) = spite_after; } while (0);
}

static void spite_guard_enter(SpiteGuard* guard) {
    int64_t spite_me = (int64_t)(intptr_t)&spite_guard_thread;
    if (__atomic_load_n(&guard->owner, __ATOMIC_ACQUIRE) == spite_me) { guard->depth = guard->depth + 1; return; }
    int64_t spite_free = 0;
    while (!__atomic_compare_exchange_n(&guard->owner, &spite_free, spite_me, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) { spite_free = 0; }
    guard->depth = 1;
}

static void spite_guard_leave(SpiteGuard* guard) {
    guard->depth = guard->depth - 1;
    if (guard->depth == 0) __atomic_store_n(&guard->owner, 0, __ATOMIC_RELEASE);
}
