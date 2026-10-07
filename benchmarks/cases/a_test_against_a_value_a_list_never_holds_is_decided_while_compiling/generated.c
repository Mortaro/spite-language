/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Pipeline_price(Pipeline* self, int32_t amount_) {
    int32_t total_ = amount_;
    int32_t index_ = 0;
    while (((index_ < List_Pipeline_Step_count(self->steps_)))) {
        if (((({ Nullable_Pipeline_Step spite_temp_1 = List_Pipeline_Step_get_at(self->steps_, index_); if (__builtin_expect(!spite_temp_1.has_value, 0)) spite_outside_list("steps[index]", spite_site_1()); spite_temp_1.value; }) == Pipeline_Step_discount))) {
            total_ = ({ int32_t spite_temp_2 = total_; int32_t spite_temp_3 = (total_ / 10); int32_t spite_temp_4; if (__builtin_expect(__builtin_sub_overflow(spite_temp_2, spite_temp_3, &spite_temp_4), 0)) spite_overflowed("total - total / 10", "an Integer", "-", (int64_t)spite_temp_2, (int64_t)spite_temp_3, spite_site_2()); spite_temp_4; });
        }
        if (((({ Nullable_Pipeline_Step spite_temp_5 = List_Pipeline_Step_get_at(self->steps_, index_); if (__builtin_expect(!spite_temp_5.has_value, 0)) spite_outside_list("steps[index]", spite_site_3()); spite_temp_5.value; }) == Pipeline_Step_tax))) {
            total_ = ({ int32_t spite_temp_6 = total_; int32_t spite_temp_7 = (total_ / 5); int32_t spite_temp_8; if (__builtin_expect(__builtin_add_overflow(spite_temp_6, spite_temp_7, &spite_temp_8), 0)) spite_overflowed("total + total / 5", "an Integer", "+", (int64_t)spite_temp_6, (int64_t)spite_temp_7, spite_site_4()); spite_temp_8; });
        }
        if ((((void)((({ Nullable_Pipeline_Step spite_temp_9 = List_Pipeline_Step_get_at(self->steps_, index_); if (__builtin_expect(!spite_temp_9.has_value, 0)) spite_outside_list("steps[index]", spite_site_5()); spite_temp_9.value; }) == Pipeline_Step_rounding)), 0))) {
            total_ = ({ int32_t spite_temp_10 = (total_ / 100); int32_t spite_temp_11 = 100; int32_t spite_temp_12; if (__builtin_expect(__builtin_mul_overflow(spite_temp_10, spite_temp_11, &spite_temp_12), 0)) spite_overflowed("total / 100 * 100", "an Integer", "*", (int64_t)spite_temp_10, (int64_t)spite_temp_11, spite_site_6()); spite_temp_12; });
        }
        if ((((void)((({ Nullable_Pipeline_Step spite_temp_13 = List_Pipeline_Step_get_at(self->steps_, index_); if (__builtin_expect(!spite_temp_13.has_value, 0)) spite_outside_list("steps[index]", spite_site_7()); spite_temp_13.value; }) == Pipeline_Step_coupon)), 0))) {
            total_ = ({ int32_t spite_temp_14 = total_; int32_t spite_temp_15 = 50; int32_t spite_temp_16; if (__builtin_expect(__builtin_sub_overflow(spite_temp_14, spite_temp_15, &spite_temp_16), 0)) spite_overflowed("total - 50", "an Integer", "-", (int64_t)spite_temp_14, (int64_t)spite_temp_15, spite_site_8()); spite_temp_16; });
        }
        index_ = (index_ + 1);
    }
    int32_t spite_temp_17 = total_;
    return spite_temp_17;
}
