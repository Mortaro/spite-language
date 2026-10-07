/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int64_t Naive_rounds___held_0(Naive* self, List_Integer* values_) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t round_ = 0;
    while (((round_ < 400))) {
        int32_t sum_ = Naive_checksum___held_0(self, values_, round_);
        total_ = ({ int64_t spite_temp_1 = total_; int64_t spite_temp_2 = SpiteInteger_to_long(sum_); int64_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("total + sum", "a Long", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
        round_ = (round_ + 1);
    }
    int64_t spite_temp_4 = total_;
    return spite_temp_4;
}

int32_t Naive_checksum___held_0(Naive* self, List_Integer* values_, int32_t round_) {
    int32_t total_ = 0;
    int32_t index_ = 0;
    int32_t spite_temp_5 = List_Integer_count(values_);
    int32_t* spite_temp_6 = (int32_t*)(intptr_t)(values_)->items_;
    while (index_ < spite_temp_5) {
        #if defined(__clang__)
        #pragma clang fp contract(fast) reassociate(on)
        #endif
        total_ = ({ int32_t spite_temp_7 = ({ int32_t spite_temp_8 = total_; int32_t spite_temp_9 = ({ int32_t spite_temp_10 = spite_temp_6[index_]; int32_t spite_temp_11 = 3; int32_t spite_temp_12; if (__builtin_expect(__builtin_mul_overflow(spite_temp_10, spite_temp_11, &spite_temp_12), 0)) spite_overflowed("values[index] * 3", "an Integer", "*", (int64_t)spite_temp_10, (int64_t)spite_temp_11, spite_site_2()); spite_temp_12; }); int32_t spite_temp_13; if (__builtin_expect(__builtin_add_overflow(spite_temp_8, spite_temp_9, &spite_temp_13), 0)) spite_overflowed("total + values[index] * 3", "an Integer", "+", (int64_t)spite_temp_8, (int64_t)spite_temp_9, spite_site_2()); spite_temp_13; }); int32_t spite_temp_14 = round_; int32_t spite_temp_15; if (__builtin_expect(__builtin_add_overflow(spite_temp_7, spite_temp_14, &spite_temp_15), 0)) spite_overflowed("total + values[index] * 3 + round", "an Integer", "+", (int64_t)spite_temp_7, (int64_t)spite_temp_14, spite_site_2()); spite_temp_15; });
        index_ = (index_ + 1);
    }
    int32_t spite_temp_16 = total_;
    return spite_temp_16;
}
