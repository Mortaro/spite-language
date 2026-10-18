/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int64_t Naive_rounds___held_0(Naive* self, List_Integer* amounts_) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t round_ = 0;
    while (((round_ < 20))) {
        int32_t parts_ = ((round_ % 7) + 1);
        int64_t shared_ = Naive_shares___held_0(self, amounts_, parts_);
        total_ = ({ int64_t spite_temp_1 = total_; int64_t spite_temp_2 = shared_; int64_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("total + shared", "a Long", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
        round_ = (round_ + 1);
    }
    int64_t spite_temp_4 = total_;
    return spite_temp_4;
}

int64_t Naive_shares___held_0(Naive* self, List_Integer* amounts_, int32_t parts_) {
    if (!(((parts_ > 0)))) {
        spite_failed_1(parts_);
    }
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t index_ = 0;
    int32_t spite_temp_5 = List_Integer_count(amounts_);
    int32_t* spite_temp_6 = (int32_t*)(intptr_t)(amounts_)->items_;
    while (index_ < spite_temp_5) {
        #if defined(__clang__)
        #pragma clang fp contract(fast) reassociate(on)
        #endif
        int32_t amount_ = ({ List_Integer* spite_temp_7 = amounts_; int32_t spite_temp_8 = index_; if (__builtin_expect(spite_temp_8 < 0 || spite_temp_8 >= (spite_temp_7)->item_count_, 0)) spite_outside_list("amounts[index]", spite_site_2()); ((int32_t*)(intptr_t)(spite_temp_7)->items_)[spite_temp_8]; });
        total_ = ({ int64_t spite_temp_9 = ({ int64_t spite_temp_10 = total_; int64_t spite_temp_11 = SpiteInteger_to_long(({ int32_t spite_temp_12 = amount_; int32_t spite_temp_13 = parts_; int32_t spite_temp_14 = 0; if (__builtin_expect(spite_temp_13 == -1 && __builtin_sub_overflow((int32_t)0, spite_temp_12, &spite_temp_14), 0)) spite_overflowed("amount / parts", "an Integer", "/", (int64_t)spite_temp_12, (int64_t)spite_temp_13, spite_site_3()); (int32_t)(spite_temp_13 == -1 ? spite_temp_14 : spite_temp_12 / spite_temp_13); })); int64_t spite_temp_15; if (__builtin_expect(__builtin_add_overflow(spite_temp_10, spite_temp_11, &spite_temp_15), 0)) spite_overflowed("total + amount / parts", "a Long", "+", (int64_t)spite_temp_10, (int64_t)spite_temp_11, spite_site_3()); spite_temp_15; }); int64_t spite_temp_16 = SpiteInteger_to_long(({ int32_t spite_temp_17 = amount_; int32_t spite_temp_18 = parts_; (int32_t)(spite_temp_18 == -1 ? (int32_t)0 : spite_temp_17 % spite_temp_18); })); int64_t spite_temp_19; if (__builtin_expect(__builtin_add_overflow(spite_temp_9, spite_temp_16, &spite_temp_19), 0)) spite_overflowed("total + amount / parts + amount % parts", "a Long", "+", (int64_t)spite_temp_9, (int64_t)spite_temp_16, spite_site_3()); spite_temp_19; });
        index_ = (index_ + 1);
    }
    int64_t spite_temp_20 = total_;
    return spite_temp_20;
}
