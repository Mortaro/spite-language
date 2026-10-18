/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void Naive_sort_and_check(Naive* self) {
    List_Integer* numbers_ = List_Integer___make();
    int64_t seed_ = SpiteInteger_to_long(42);
    int32_t index_ = 0;
    while (((index_ < 2000000))) {
        seed_ = ((seed_ * SpiteInteger_to_long(48271)) % SpiteInteger_to_long(2147483647));
        int32_t number_ = ({ int64_t spite_temp_1 = (seed_ % SpiteInteger_to_long(1000000)); if (__builtin_expect(spite_temp_1 < INT32_MIN || spite_temp_1 > INT32_MAX, 0)) spite_narrowed((int64_t)spite_temp_1, "a Long", "an Integer", spite_site_1()); (int32_t)spite_temp_1; });
        List_Integer_append(numbers_, number_);
        index_ = (index_ + 1);
    }
    int32_t last_ = (List_Integer_count(numbers_) - 1);
    Naive_sort___held_0(self, numbers_, 0, last_);
    int64_t checksum_ = SpiteInteger_to_long(0);
    bool ordered_ = true;
    index_ = 0;
    int32_t spite_temp_2 = List_Integer_count(numbers_);
    int32_t* spite_temp_3 = (int32_t*)(intptr_t)(numbers_)->items_;
    while (index_ < spite_temp_2) {
        #if defined(__clang__)
        #pragma clang fp contract(fast) reassociate(on)
        #endif
        checksum_ = (checksum_ + SpiteInteger_to_long(({ int32_t spite_temp_4 = spite_temp_3[index_]; int32_t spite_temp_5 = (index_ % 7); int32_t spite_temp_6; if (__builtin_expect(__builtin_mul_overflow(spite_temp_4, spite_temp_5, &spite_temp_6), 0)) spite_overflowed("numbers[index] * (index % 7)", "an Integer", "*", (int64_t)spite_temp_4, (int64_t)spite_temp_5, spite_site_2()); spite_temp_6; })));
        if (((index_ > 0))) {
            if (!(((List_Integer_get_at(numbers_, (index_ - 1))).has_value))) {
                spite_failed_1(index_, numbers_, seed_, last_, checksum_, ordered_, self);
            }
            if ((((List_Integer_get_at(numbers_, (index_ - 1))).value > spite_temp_3[index_]))) {
                ordered_ = false;
            }
        }
        index_ = (index_ + 1);
    }
    self->sorted_in_order_ = ordered_;
    self->weighted_sum_ = checksum_;
    List_Integer___release(numbers_);
}

void Naive_sort___held_0(Naive* self, List_Integer* numbers_, int32_t low_, int32_t high_) {
    int32_t left_ = low_;
    int32_t right_ = high_;
    while (((left_ < right_))) {
        int32_t middle_ = ({ int32_t spite_temp_7 = left_; int32_t spite_temp_8 = (({ int32_t spite_temp_9 = right_; int32_t spite_temp_10 = left_; int32_t spite_temp_11; if (__builtin_expect(__builtin_sub_overflow(spite_temp_9, spite_temp_10, &spite_temp_11), 0)) spite_overflowed("right - left", "an Integer", "-", (int64_t)spite_temp_9, (int64_t)spite_temp_10, spite_site_3()); spite_temp_11; }) / 2); int32_t spite_temp_12; if (__builtin_expect(__builtin_add_overflow(spite_temp_7, spite_temp_8, &spite_temp_12), 0)) spite_overflowed("left + (right - left) / 2", "an Integer", "+", (int64_t)spite_temp_7, (int64_t)spite_temp_8, spite_site_3()); spite_temp_12; });
        if (!(((List_Integer_get_at(numbers_, middle_)).has_value))) {
            spite_failed_2(middle_, numbers_, low_, high_, left_, right_, self);
        }
        int32_t pivot_ = ({ List_Integer* spite_temp_13 = numbers_; int32_t spite_temp_14 = middle_; if (__builtin_expect(spite_temp_14 < 0 || spite_temp_14 >= (spite_temp_13)->item_count_, 0)) spite_outside_list("numbers[middle]", spite_site_4()); ((int32_t*)(intptr_t)(spite_temp_13)->items_)[spite_temp_14]; });
        int32_t lower_ = left_;
        int32_t upper_ = right_;
        while (((lower_ <= upper_))) {
            while (((((lower_ < List_Integer_count(numbers_))) && ((({ Nullable_Integer spite_temp_15 = List_Integer_get_at(numbers_, lower_); if (__builtin_expect(!spite_temp_15.has_value, 0)) spite_outside_list("numbers[lower]", spite_site_5()); spite_temp_15.value; }) < pivot_))))) {
                lower_ = (lower_ + 1);
            }
            while (((((((upper_ >= 0)) && ((upper_ < List_Integer_count(numbers_))))) && ((({ int32_t spite_temp_16 = upper_; if (__builtin_expect(spite_temp_16 >= (numbers_)->item_count_, 0)) spite_outside_list("numbers[upper]", spite_site_6()); ((int32_t*)(intptr_t)(numbers_)->items_)[spite_temp_16]; }) > pivot_))))) {
                upper_ = ({ int32_t spite_temp_17 = upper_; int32_t spite_temp_18 = 1; int32_t spite_temp_19; if (__builtin_expect(__builtin_sub_overflow(spite_temp_17, spite_temp_18, &spite_temp_19), 0)) spite_overflowed("upper - 1", "an Integer", "-", (int64_t)spite_temp_17, (int64_t)spite_temp_18, spite_site_7()); spite_temp_19; });
            }
            if (((lower_ <= upper_))) {
                if (!(((List_Integer_get_at(numbers_, lower_)).has_value))) {
                    spite_failed_3(lower_, numbers_, low_, high_, left_, right_, middle_, pivot_, upper_, self);
                }
                if (!(((List_Integer_get_at(numbers_, upper_)).has_value))) {
                    spite_failed_4(upper_, numbers_, low_, high_, left_, right_, middle_, pivot_, lower_, self);
                }
                int32_t swapped_ = ({ List_Integer* spite_temp_20 = numbers_; int32_t spite_temp_21 = lower_; if (__builtin_expect(spite_temp_21 < 0 || spite_temp_21 >= (spite_temp_20)->item_count_, 0)) spite_outside_list("numbers[lower]", spite_site_8()); ((int32_t*)(intptr_t)(spite_temp_20)->items_)[spite_temp_21]; });
                int32_t moved_ = ({ List_Integer* spite_temp_22 = numbers_; int32_t spite_temp_23 = upper_; if (__builtin_expect(spite_temp_23 < 0 || spite_temp_23 >= (spite_temp_22)->item_count_, 0)) spite_outside_list("numbers[upper]", spite_site_9()); ((int32_t*)(intptr_t)(spite_temp_22)->items_)[spite_temp_23]; });
                List_Integer_set_at(numbers_, lower_, moved_);
                List_Integer_set_at(numbers_, upper_, swapped_);
                lower_ = ({ int32_t spite_temp_24 = lower_; int32_t spite_temp_25 = 1; int32_t spite_temp_26; if (__builtin_expect(__builtin_add_overflow(spite_temp_24, spite_temp_25, &spite_temp_26), 0)) spite_overflowed("lower + 1", "an Integer", "+", (int64_t)spite_temp_24, (int64_t)spite_temp_25, spite_site_10()); spite_temp_26; });
                upper_ = ({ int32_t spite_temp_27 = upper_; int32_t spite_temp_28 = 1; int32_t spite_temp_29; if (__builtin_expect(__builtin_sub_overflow(spite_temp_27, spite_temp_28, &spite_temp_29), 0)) spite_overflowed("upper - 1", "an Integer", "-", (int64_t)spite_temp_27, (int64_t)spite_temp_28, spite_site_11()); spite_temp_29; });
            }
        }
        if (((({ int32_t spite_temp_30 = upper_; int32_t spite_temp_31 = left_; int32_t spite_temp_32; if (__builtin_expect(__builtin_sub_overflow(spite_temp_30, spite_temp_31, &spite_temp_32), 0)) spite_overflowed("upper - left", "an Integer", "-", (int64_t)spite_temp_30, (int64_t)spite_temp_31, spite_site_12()); spite_temp_32; }) < ({ int32_t spite_temp_33 = right_; int32_t spite_temp_34 = lower_; int32_t spite_temp_35; if (__builtin_expect(__builtin_sub_overflow(spite_temp_33, spite_temp_34, &spite_temp_35), 0)) spite_overflowed("right - lower", "an Integer", "-", (int64_t)spite_temp_33, (int64_t)spite_temp_34, spite_site_12()); spite_temp_35; })))) {
            Naive_sort___held_0(self, numbers_, left_, upper_);
            left_ = lower_;
        }
        else {
            Naive_sort___held_0(self, numbers_, lower_, right_);
            right_ = upper_;
        }
    }
}
