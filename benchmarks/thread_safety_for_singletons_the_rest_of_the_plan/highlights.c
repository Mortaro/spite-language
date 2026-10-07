/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Logger_log_all(Logger* self) {
    int32_t index_ = 0;
    while (((index_ < self->rounds_))) {
        int32_t entry_ = Logger_entry_for(self, index_);
        EventLog_record(self->log_, entry_);
        index_ = (index_ + 1);
    }
    int32_t spite_temp_1 = self->rounds_;
    return spite_temp_1;
}

void EventLog_record(EventLog* self, int32_t entry_) {
    if (spite_skipped_depth < 16 && __atomic_load_n(&spite_tasks_in_flight, __ATOMIC_ACQUIRE) == 0) {
        int32_t spite_skip = spite_skipped_depth;
        spite_skipped[spite_skip] = (void*)&EventLog___guard;
        spite_skipped_taken[spite_skip] = 0;
        spite_skipped_depth = spite_skip + 1;
        SPITE_GUARDS_COUNT(1);
        EventLog_record___unguarded(self, entry_);
        spite_skipped_depth = spite_skip;
        if (spite_skipped_taken[spite_skip]) spite_guard_leave(&EventLog___guard);
        SPITE_GUARDS_COUNT(-1);
        return;
    }
    spite_guard_enter(&EventLog___guard);
    SPITE_GUARDS_COUNT(1);
    EventLog_record___unguarded(self, entry_);
    spite_guard_leave(&EventLog___guard);
    SPITE_GUARDS_COUNT(-1);
}

void EventLog_record___unguarded(EventLog* self, int32_t entry_) {
    List_Integer_append(self->entries_, entry_);
}
