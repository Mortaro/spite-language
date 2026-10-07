/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void List_Integer_each_count_value_for_tally(List_Integer* self, Tally* owner_) {
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        int32_t item_ = TypedMemory__Integer_read_value(self->values_, self->items_, index_);
        Tally_count_value(owner_, item_);
        index_ = (index_ + 1);
    }
    Tally___release(owner_);
}

void Tally_count_value(Tally* self, int32_t value_) {
    self->total_ = ({ int64_t spite_temp_1 = self->total_; int64_t spite_temp_2 = SpiteInteger_to_long(value_); int64_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("total + value", "a Long", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
    if (((value_ > 900))) {
        self->large_ = ({ int32_t spite_temp_4 = self->large_; int32_t spite_temp_5 = 1; int32_t spite_temp_6; if (__builtin_expect(__builtin_add_overflow(spite_temp_4, spite_temp_5, &spite_temp_6), 0)) spite_overflowed("large + 1", "an Integer", "+", (int64_t)spite_temp_4, (int64_t)spite_temp_5, spite_site_2()); spite_temp_6; });
    }
}
