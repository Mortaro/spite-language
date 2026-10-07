/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int64_t Naive_all_rounds(Naive* self, int32_t rounds_) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t round_ = 0;
    while (((round_ < rounds_))) {
        int32_t biggest_ = Naive_largest(self, ({ List_Integer* spite_temp_1 = List_Integer___make(); List_Integer_append(spite_temp_1, (round_ % 10)); List_Integer_append(spite_temp_1, (round_ % 7)); List_Integer_append(spite_temp_1, (round_ % 13)); List_Integer_append(spite_temp_1, (round_ % 3)); spite_temp_1; }));
        total_ = ({ int64_t spite_temp_2 = total_; int64_t spite_temp_3 = SpiteInteger_to_long(biggest_); int64_t spite_temp_4; if (__builtin_expect(__builtin_add_overflow(spite_temp_2, spite_temp_3, &spite_temp_4), 0)) spite_overflowed("total + biggest", "a Long", "+", (int64_t)spite_temp_2, (int64_t)spite_temp_3, spite_site_1()); spite_temp_4; });
        round_ = (round_ + 1);
    }
    int64_t spite_temp_5 = total_;
    return spite_temp_5;
}

int32_t Naive_largest(Naive* self, List_Integer* values_) {
    int32_t biggest_ = 0;
    int32_t index_ = 0;
    int32_t spite_temp_6 = List_Integer_count(values_);
    int32_t* spite_temp_7 = (int32_t*)(intptr_t)(values_)->items_;
    while (index_ < spite_temp_6) {
        #if defined(__clang__)
        #pragma clang fp contract(fast) reassociate(on)
        #endif
        if (((spite_temp_7[index_] > biggest_))) {
            biggest_ = spite_temp_7[index_];
        }
        index_ = (index_ + 1);
    }
    int32_t spite_temp_8 = biggest_;
    List_Integer___release(values_);
    return spite_temp_8;
}
