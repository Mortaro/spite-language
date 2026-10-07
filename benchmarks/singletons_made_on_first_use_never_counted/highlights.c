/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

Rules* spite_singleton_Rules(void) {
    Rules* found = SPITE_SINGLETON_FOUND(spite_singleton_Rules_cache);
    if (found != 0) return found;
    spite_singleton_check_circle("Rules");
    SPITE_LOCK(spite_singleton_Rules_lock);
    if (spite_singleton_Rules_cache == 0) {
        if (spite_singleton_Rules_destroyed) spite_singleton_used_after_exit("Rules");
        spite_singleton_making("Rules");
        Rules* made = Rules___make();
        spite_singleton_made();
        spite_singleton_created(spite_singleton_Rules_teardown);
        SPITE_SINGLETON_PUBLISH(spite_singleton_Rules_cache, made);
    }
    SPITE_UNLOCK(spite_singleton_Rules_lock);
    return spite_singleton_Rules_cache;
}

void Visit___init(Visit* self) {
    self->rules_ = spite_singleton_Rules();
    self->number_ = 0;
}

int64_t Fetcher_fetch_many(Fetcher* self) {
    int64_t found_ = SpiteInteger_to_long(0);
    int32_t index_ = 0;
    while (((index_ < self->rounds_))) {
        Visit spite_slot_1;
        Visit* visit_ = Visit___make_into(&spite_slot_1, index_);
        found_ = ({ int64_t spite_temp_1 = found_; int64_t spite_temp_2 = SpiteInteger_to_long(Visit_weight(visit_)); int64_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("found + visit.weight()", "a Long", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
        index_ = (index_ + 1);
    }
    int64_t spite_temp_4 = found_;
    return spite_temp_4;
}

int32_t Visit_weight(Visit* self) {
    int32_t spite_temp_5 = ({ int32_t spite_temp_6 = (self->number_ % 7); int32_t spite_temp_7 = (self->rules_)->base_; int32_t spite_temp_8; if (__builtin_expect(__builtin_add_overflow(spite_temp_6, spite_temp_7, &spite_temp_8), 0)) spite_overflowed("number % 7 + rules.base", "an Integer", "+", (int64_t)spite_temp_6, (int64_t)spite_temp_7, spite_site_2()); spite_temp_8; });
    return spite_temp_5;
}
