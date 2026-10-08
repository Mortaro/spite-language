/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

#define Ledger___atomic 0

int32_t Naive_note_many(Naive* self, int32_t count_) {
    int32_t seen_ = 0;
    int32_t index_ = 0;
    while (((index_ < count_))) {
        index_ = Ledger_note(self->ledger_, index_);
        seen_ = ({ int32_t spite_temp_1 = seen_; int32_t spite_temp_2 = List_Integer_count((self->ledger_)->milestones_); int32_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("seen + ledger.milestones.count()", "an Integer", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
    }
    int32_t spite_temp_4 = seen_;
    return spite_temp_4;
}

int32_t Ledger_note(Ledger* self, int32_t amount_) {
    SPITE_GUARDS_COUNT(1);
    int32_t spite_unshared = Ledger_note___unguarded(self, amount_);
    SPITE_GUARDS_COUNT(-1);
    return spite_unshared;
}

int32_t Ledger_note___unguarded(Ledger* self, int32_t amount_) {
    do { int64_t spite_step = (SpiteInteger_to_long(amount_)); int64_t spite_before; int64_t spite_after; if (Ledger___atomic) spite_before = __atomic_fetch_add(&(self->total_), spite_step, __ATOMIC_SEQ_CST); else spite_before = (self->total_); if (__builtin_expect(__builtin_add_overflow(spite_before, spite_step, &spite_after), 0)) spite_overflowed("total + amount", "a Long", "+", (int64_t)spite_before, (int64_t)spite_step, spite_site_2()); if (!Ledger___atomic) (self->total_) = spite_after; } while (0);
    do { int32_t spite_step = (1); int32_t spite_before; int32_t spite_after; if (Ledger___atomic) spite_before = __atomic_fetch_add(&(self->notes_), spite_step, __ATOMIC_SEQ_CST); else spite_before = (self->notes_); if (__builtin_expect(__builtin_add_overflow(spite_before, spite_step, &spite_after), 0)) spite_overflowed("notes + 1", "an Integer", "+", (int64_t)spite_before, (int64_t)spite_step, spite_site_3()); if (!Ledger___atomic) (self->notes_) = spite_after; } while (0);
    if ((((amount_ % 1000000) == 0))) {
        List_Integer_append(self->milestones_, amount_);
    }
    int32_t spite_temp_5 = ({ int32_t spite_temp_6 = amount_; int32_t spite_temp_7 = 1; int32_t spite_temp_8; if (__builtin_expect(__builtin_add_overflow(spite_temp_6, spite_temp_7, &spite_temp_8), 0)) spite_overflowed("amount + 1", "an Integer", "+", (int64_t)spite_temp_6, (int64_t)spite_temp_7, spite_site_4()); spite_temp_8; });
    return spite_temp_5;
}
