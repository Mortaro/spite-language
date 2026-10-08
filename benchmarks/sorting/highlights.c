/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void Naive_Naive(Naive* self) {
    int64_t start_ = Clock_elapsed_nanoseconds(self->clock_);
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
                spite_failed_1(index_, numbers_, start_, seed_, last_, checksum_, ordered_);
            }
            if ((((List_Integer_get_at(numbers_, (index_ - 1))).value > spite_temp_3[index_]))) {
                ordered_ = false;
            }
        }
        index_ = (index_ + 1);
    }
    int64_t microseconds_ = (({ int64_t spite_temp_7 = Clock_elapsed_nanoseconds(self->clock_); int64_t spite_temp_8 = start_; int64_t spite_temp_9; if (__builtin_expect(__builtin_sub_overflow(spite_temp_7, spite_temp_8, &spite_temp_9), 0)) spite_overflowed("clock.elapsed_nanoseconds() - start", "a Long", "-", (int64_t)spite_temp_7, (int64_t)spite_temp_8, spite_site_3()); spite_temp_9; }) / SpiteInteger_to_long(1000));
    List_Console_Printable spite_framed_1; Console_Printable spite_framed_1_items[4]; int32_t spite_framed_1_count = 0;
    Console_print(self->console_, ({ spite_framed_1_items[0] = spite_tagged_object(0, ((void*)&spite_lit_1_box)); spite_framed_1_items[1] = spite_tagged_SpiteBoolean(ordered_); spite_framed_1_items[2] = spite_tagged_object(0, ((void*)&spite_lit_2_box)); spite_framed_1_items[3] = spite_tagged_SpiteLong(checksum_); spite_framed_1_count = 4; List_Console_Printable___framed(&spite_framed_1, (int64_t)(intptr_t)spite_framed_1_items, 4); }));
    for (int32_t spite_index = 0; spite_index < spite_framed_1_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_1_items[spite_index]); }
    List_Console_Printable spite_framed_2; Console_Printable spite_framed_2_items[1]; int32_t spite_framed_2_count = 0;
    Console_error(self->console_, ({ spite_framed_2_items[0] = spite_tagged_object(0, spite_box_SpiteString(({ char spite_temp_10_digits[24]; SpiteString spite_temp_10 = SPITE_STATIC_STRING(spite_temp_10_digits, spite_long_digits(spite_temp_10_digits, (int64_t)(microseconds_))); SpiteString spite_temp_11[] = {spite_lit_3, spite_temp_10}; SpiteString spite_temp_12 = spite_string_join(2, spite_temp_11); spite_temp_12; }))); spite_framed_2_count = 1; List_Console_Printable___framed(&spite_framed_2, (int64_t)(intptr_t)spite_framed_2_items, 1); }));
    for (int32_t spite_index = 0; spite_index < spite_framed_2_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_2_items[spite_index]); }
    List_Integer___release(numbers_);
}

void Naive_sort___held_0(Naive* self, List_Integer* numbers_, int32_t low_, int32_t high_) {
    int32_t left_ = low_;
    int32_t right_ = high_;
    while (((left_ < right_))) {
        int32_t middle_ = ({ int32_t spite_temp_13 = left_; int32_t spite_temp_14 = (({ int32_t spite_temp_15 = right_; int32_t spite_temp_16 = left_; int32_t spite_temp_17; if (__builtin_expect(__builtin_sub_overflow(spite_temp_15, spite_temp_16, &spite_temp_17), 0)) spite_overflowed("right - left", "an Integer", "-", (int64_t)spite_temp_15, (int64_t)spite_temp_16, spite_site_4()); spite_temp_17; }) / 2); int32_t spite_temp_18; if (__builtin_expect(__builtin_add_overflow(spite_temp_13, spite_temp_14, &spite_temp_18), 0)) spite_overflowed("left + (right - left) / 2", "an Integer", "+", (int64_t)spite_temp_13, (int64_t)spite_temp_14, spite_site_4()); spite_temp_18; });
        if (!(((List_Integer_get_at(numbers_, middle_)).has_value))) {
            spite_failed_2(middle_, numbers_, low_, high_, left_, right_);
        }
        int32_t pivot_ = (List_Integer_get_at(numbers_, middle_)).value;
        int32_t lower_ = left_;
        int32_t upper_ = right_;
        while (((lower_ <= upper_))) {
            while (((((lower_ < List_Integer_count(numbers_))) && ((({ Nullable_Integer spite_temp_19 = List_Integer_get_at(numbers_, lower_); if (__builtin_expect(!spite_temp_19.has_value, 0)) spite_outside_list("numbers[lower]", spite_site_5()); spite_temp_19.value; }) < pivot_))))) {
                lower_ = (lower_ + 1);
            }
            while (((((((upper_ >= 0)) && ((upper_ < List_Integer_count(numbers_))))) && ((({ Nullable_Integer spite_temp_20 = List_Integer_get_at(numbers_, upper_); if (__builtin_expect(!spite_temp_20.has_value, 0)) spite_outside_list("numbers[upper]", spite_site_6()); spite_temp_20.value; }) > pivot_))))) {
                upper_ = ({ int32_t spite_temp_21 = upper_; int32_t spite_temp_22 = 1; int32_t spite_temp_23; if (__builtin_expect(__builtin_sub_overflow(spite_temp_21, spite_temp_22, &spite_temp_23), 0)) spite_overflowed("upper - 1", "an Integer", "-", (int64_t)spite_temp_21, (int64_t)spite_temp_22, spite_site_7()); spite_temp_23; });
            }
            if (((lower_ <= upper_))) {
                if (!(((List_Integer_get_at(numbers_, lower_)).has_value))) {
                    spite_failed_3(lower_, numbers_, low_, high_, left_, right_, middle_, pivot_, upper_);
                }
                if (!(((List_Integer_get_at(numbers_, upper_)).has_value))) {
                    spite_failed_4(upper_, numbers_, low_, high_, left_, right_, middle_, pivot_, lower_);
                }
                int32_t swapped_ = (List_Integer_get_at(numbers_, lower_)).value;
                int32_t moved_ = (List_Integer_get_at(numbers_, upper_)).value;
                List_Integer_set_at(numbers_, lower_, moved_);
                List_Integer_set_at(numbers_, upper_, swapped_);
                lower_ = ({ int32_t spite_temp_24 = lower_; int32_t spite_temp_25 = 1; int32_t spite_temp_26; if (__builtin_expect(__builtin_add_overflow(spite_temp_24, spite_temp_25, &spite_temp_26), 0)) spite_overflowed("lower + 1", "an Integer", "+", (int64_t)spite_temp_24, (int64_t)spite_temp_25, spite_site_8()); spite_temp_26; });
                upper_ = ({ int32_t spite_temp_27 = upper_; int32_t spite_temp_28 = 1; int32_t spite_temp_29; if (__builtin_expect(__builtin_sub_overflow(spite_temp_27, spite_temp_28, &spite_temp_29), 0)) spite_overflowed("upper - 1", "an Integer", "-", (int64_t)spite_temp_27, (int64_t)spite_temp_28, spite_site_9()); spite_temp_29; });
            }
        }
        if (((({ int32_t spite_temp_30 = upper_; int32_t spite_temp_31 = left_; int32_t spite_temp_32; if (__builtin_expect(__builtin_sub_overflow(spite_temp_30, spite_temp_31, &spite_temp_32), 0)) spite_overflowed("upper - left", "an Integer", "-", (int64_t)spite_temp_30, (int64_t)spite_temp_31, spite_site_10()); spite_temp_32; }) < ({ int32_t spite_temp_33 = right_; int32_t spite_temp_34 = lower_; int32_t spite_temp_35; if (__builtin_expect(__builtin_sub_overflow(spite_temp_33, spite_temp_34, &spite_temp_35), 0)) spite_overflowed("right - lower", "an Integer", "-", (int64_t)spite_temp_33, (int64_t)spite_temp_34, spite_site_10()); spite_temp_35; })))) {
            Naive_sort___held_0(self, numbers_, left_, upper_);
            left_ = lower_;
        }
        else {
            Naive_sort___held_0(self, numbers_, lower_, right_);
            right_ = upper_;
        }
    }
}
