/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

SpiteString Naive_label(Naive* self, int32_t count_) {
    if (((count_ == 1))) {
        SpiteString spite_temp_1 = spite_lit_1;
        return spite_temp_1;
    }
    SpiteString plural_ = spite_lit_2;
    SpiteString spite_temp_2 = ({ char spite_temp_3_digits[24]; SpiteString spite_temp_3 = SPITE_STATIC_STRING(spite_temp_3_digits, spite_long_digits(spite_temp_3_digits, (int64_t)(count_))); SpiteString spite_temp_4 = plural_; SpiteString spite_temp_5[] = {spite_temp_3, spite_lit_3, spite_temp_4}; SpiteString spite_temp_6 = spite_string_join(3, spite_temp_5); spite_temp_6; });
    SpiteString___release(plural_);
    return spite_temp_2;
}

int64_t Naive_characters(Naive* self, int32_t count_) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t index_ = 0;
    while (((index_ < count_))) {
        SpiteString shown_ = Naive_label(self, (index_ % 50));
        total_ = ({ int64_t spite_temp_7 = total_; int64_t spite_temp_8 = SpiteInteger_to_long(SpiteString_length(shown_)); int64_t spite_temp_9; if (__builtin_expect(__builtin_add_overflow(spite_temp_7, spite_temp_8, &spite_temp_9), 0)) spite_overflowed("total + shown.length()", "a Long", "+", (int64_t)spite_temp_7, (int64_t)spite_temp_8, spite_site_1()); spite_temp_9; });
        index_ = (index_ + 1);
        SpiteString___release(shown_);
    }
    int64_t spite_temp_10 = total_;
    return spite_temp_10;
}
