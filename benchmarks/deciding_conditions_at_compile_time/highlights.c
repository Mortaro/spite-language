/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

struct Meter__true {
    SpiteHeader header;
    int64_t total_;
};

void Meter__false_add(Meter__false* self, int32_t value_) {
    {
        self->total_ = ({ int64_t spite_temp_1 = self->total_; int64_t spite_temp_2 = SpiteInteger_to_long(value_); int64_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("total + value", "a Long", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
    }
}

void Meter__true_add(Meter__true* self, int32_t value_) {
    {
        int64_t square_ = SpiteInteger_to_long(value_);
        self->total_ = ({ int64_t spite_temp_4 = self->total_; int64_t spite_temp_5 = ({ int64_t spite_temp_6 = square_; int64_t spite_temp_7 = SpiteInteger_to_long(value_); int64_t spite_temp_8; if (__builtin_expect(__builtin_mul_overflow(spite_temp_6, spite_temp_7, &spite_temp_8), 0)) spite_overflowed("square * value", "a Long", "*", (int64_t)spite_temp_6, (int64_t)spite_temp_7, spite_site_2()); spite_temp_8; }); int64_t spite_temp_9; if (__builtin_expect(__builtin_add_overflow(spite_temp_4, spite_temp_5, &spite_temp_9), 0)) spite_overflowed("total + square * value", "a Long", "+", (int64_t)spite_temp_4, (int64_t)spite_temp_5, spite_site_2()); spite_temp_9; });
    }
}

void Naive_measure___held_0_1_2(Naive* self, List_Integer* values_, Meter__false* plain_, Meter__true* squared_) {
    int32_t index_ = 0;
    while (((index_ < List_Integer_count(values_)))) {
        int32_t value_ = ({ Nullable_Integer spite_temp_10 = List_Integer_get_at(values_, index_); if (__builtin_expect(!spite_temp_10.has_value, 0)) spite_outside_list("values[index]", spite_site_3()); spite_temp_10.value; });
        Meter__false_add(plain_, value_);
        Meter__true_add(squared_, value_);
        index_ = (index_ + 1);
    }
}
