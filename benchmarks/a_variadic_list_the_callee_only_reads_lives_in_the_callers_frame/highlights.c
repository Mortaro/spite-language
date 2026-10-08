/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int64_t Naive_all_rounds(Naive* self, int32_t rounds_) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t round_ = 0;
    while (((round_ < rounds_))) {
        List_Integer spite_framed_1; int32_t spite_framed_1_items[4]; int32_t spite_framed_1_count = 0;
        int32_t biggest_ = Naive_largest(self, ({ spite_framed_1_items[0] = (round_ % 10); spite_framed_1_items[1] = (round_ % 7); spite_framed_1_items[2] = (round_ % 13); spite_framed_1_items[3] = (round_ % 3); spite_framed_1_count = 4; List_Integer___framed(&spite_framed_1, (int64_t)(intptr_t)spite_framed_1_items, 4); }));
        total_ = (total_ + SpiteInteger_to_long(biggest_));
        round_ = (round_ + 1);
    }
    int64_t spite_temp_1 = total_;
    return spite_temp_1;
}

int32_t Naive_largest(Naive* self, List_Integer* values_) {
    int32_t biggest_ = 0;
    int32_t index_ = 0;
    int32_t spite_temp_2 = List_Integer_count(values_);
    int32_t* spite_temp_3 = (int32_t*)(intptr_t)(values_)->items_;
    while (index_ < spite_temp_2) {
        #if defined(__clang__)
        #pragma clang fp contract(fast) reassociate(on)
        #endif
        if (((spite_temp_3[index_] > biggest_))) {
            biggest_ = spite_temp_3[index_];
        }
        index_ = (index_ + 1);
    }
    int32_t spite_temp_4 = biggest_;
    List_Integer___release(values_);
    return spite_temp_4;
}
