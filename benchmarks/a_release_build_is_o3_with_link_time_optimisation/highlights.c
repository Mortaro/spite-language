/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int64_t Naive_checksum___held_0(Naive* self, Mixer* mixer_, int32_t count_) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t index_ = 0;
    while (((index_ < count_))) {
        total_ = ({ int64_t spite_temp_1 = total_; int64_t spite_temp_2 = SpiteInteger_to_long(Mixer_mixed(mixer_, index_)); int64_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("total + mixer.mixed(index)", "a Long", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
        index_ = (index_ + 1);
    }
    int64_t spite_temp_4 = total_;
    return spite_temp_4;
}

int32_t Mixer_mixed(Mixer* self, int32_t value_) {
    int32_t spite_high_1 = 65536;
    int32_t spite_high_2 = (value_ % spite_high_1);
    int32_t spite_high_3 = (self)->factor_;
    int32_t spite_high_4_left = spite_high_2;
    int32_t spite_high_4_right = spite_high_3;
    int32_t spite_high_4;
    if (__builtin_expect(__builtin_mul_overflow(spite_high_4_left, spite_high_4_right, &spite_high_4), 0)) spite_overflowed("value % 65536 * factor", "an Integer", "*", (int64_t)spite_high_4_left, (int64_t)spite_high_4_right, spite_site_2());
    int32_t spite_high_5 = 7;
    int32_t spite_high_6_left = spite_high_4;
    int32_t spite_high_6_right = spite_high_5;
    int32_t spite_high_6;
    if (__builtin_expect(__builtin_add_overflow(spite_high_6_left, spite_high_6_right, &spite_high_6), 0)) spite_overflowed("value % 65536 * factor + 7", "an Integer", "+", (int64_t)spite_high_6_left, (int64_t)spite_high_6_right, spite_site_2());
    int32_t spite_high_7 = 1000;
    int32_t spite_high_8 = (spite_high_6 % spite_high_7);
    return spite_high_8;
}
