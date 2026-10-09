/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Checkout_price(Checkout* self, int32_t amount_) {
    if (self->steps_->item_count_ == 3 && ((Checkout_Step*)(intptr_t)self->steps_->items_)[0] == Checkout_Step_discount && ((Checkout_Step*)(intptr_t)self->steps_->items_)[1] == Checkout_Step_tax && ((Checkout_Step*)(intptr_t)self->steps_->items_)[2] == Checkout_Step_discount) return Checkout_price___configured_0(self, amount_);
    int32_t total_ = amount_;
    int32_t index_ = 0;
    while (((index_ < List_Checkout_Step_count(self->steps_)))) {
        if (((({ Nullable_Checkout_Step spite_temp_1 = List_Checkout_Step_get_at(self->steps_, index_); if (__builtin_expect(!spite_temp_1.has_value, 0)) spite_outside_list("steps[index]", spite_site_1()); spite_temp_1.value; }) == Checkout_Step_discount))) {
            total_ = ({ int32_t spite_temp_2 = total_; int32_t spite_temp_3 = (total_ / 10); int32_t spite_temp_4; if (__builtin_expect(__builtin_sub_overflow(spite_temp_2, spite_temp_3, &spite_temp_4), 0)) spite_overflowed("total - total / 10", "an Integer", "-", (int64_t)spite_temp_2, (int64_t)spite_temp_3, spite_site_2()); spite_temp_4; });
        }
        if (((({ Nullable_Checkout_Step spite_temp_5 = List_Checkout_Step_get_at(self->steps_, index_); if (__builtin_expect(!spite_temp_5.has_value, 0)) spite_outside_list("steps[index]", spite_site_3()); spite_temp_5.value; }) == Checkout_Step_tax))) {
            total_ = ({ int32_t spite_temp_6 = total_; int32_t spite_temp_7 = (total_ / 5); int32_t spite_temp_8; if (__builtin_expect(__builtin_add_overflow(spite_temp_6, spite_temp_7, &spite_temp_8), 0)) spite_overflowed("total + total / 5", "an Integer", "+", (int64_t)spite_temp_6, (int64_t)spite_temp_7, spite_site_4()); spite_temp_8; });
        }
        if ((((void)((({ Nullable_Checkout_Step spite_temp_9 = List_Checkout_Step_get_at(self->steps_, index_); if (__builtin_expect(!spite_temp_9.has_value, 0)) spite_outside_list("steps[index]", spite_site_5()); spite_temp_9.value; }) == Checkout_Step_rounding)), 0))) {
            total_ = ((total_ / 100) * 100);
        }
        if ((((void)((({ Nullable_Checkout_Step spite_temp_10 = List_Checkout_Step_get_at(self->steps_, index_); if (__builtin_expect(!spite_temp_10.has_value, 0)) spite_outside_list("steps[index]", spite_site_6()); spite_temp_10.value; }) == Checkout_Step_coupon)), 0))) {
            total_ = ({ int32_t spite_temp_11 = total_; int32_t spite_temp_12 = 50; int32_t spite_temp_13; if (__builtin_expect(__builtin_sub_overflow(spite_temp_11, spite_temp_12, &spite_temp_13), 0)) spite_overflowed("total - 50", "an Integer", "-", (int64_t)spite_temp_11, (int64_t)spite_temp_12, spite_site_7()); spite_temp_13; });
        }
        index_ = (index_ + 1);
    }
    int32_t spite_temp_14 = total_;
    return spite_temp_14;
}

int32_t Checkout_price___configured_0(Checkout* self, int32_t amount_) {
    int32_t total_ = amount_;
    int32_t index_ = 0;
    while (((index_ < List_Checkout_Step_count(((List_Checkout_Step*)&spite_configured_Checkout_steps_0))))) {
        if (((({ Nullable_Checkout_Step spite_temp_1 = List_Checkout_Step_get_at(((List_Checkout_Step*)&spite_configured_Checkout_steps_0), index_); if (__builtin_expect(!spite_temp_1.has_value, 0)) spite_outside_list("steps[index]", spite_site_1()); spite_temp_1.value; }) == Checkout_Step_discount))) {
            total_ = ({ int32_t spite_temp_2 = total_; int32_t spite_temp_3 = (total_ / 10); int32_t spite_temp_4; if (__builtin_expect(__builtin_sub_overflow(spite_temp_2, spite_temp_3, &spite_temp_4), 0)) spite_overflowed("total - total / 10", "an Integer", "-", (int64_t)spite_temp_2, (int64_t)spite_temp_3, spite_site_2()); spite_temp_4; });
        }
        if (((({ Nullable_Checkout_Step spite_temp_5 = List_Checkout_Step_get_at(((List_Checkout_Step*)&spite_configured_Checkout_steps_0), index_); if (__builtin_expect(!spite_temp_5.has_value, 0)) spite_outside_list("steps[index]", spite_site_3()); spite_temp_5.value; }) == Checkout_Step_tax))) {
            total_ = ({ int32_t spite_temp_6 = total_; int32_t spite_temp_7 = (total_ / 5); int32_t spite_temp_8; if (__builtin_expect(__builtin_add_overflow(spite_temp_6, spite_temp_7, &spite_temp_8), 0)) spite_overflowed("total + total / 5", "an Integer", "+", (int64_t)spite_temp_6, (int64_t)spite_temp_7, spite_site_4()); spite_temp_8; });
        }
        if ((((void)((({ Nullable_Checkout_Step spite_temp_9 = List_Checkout_Step_get_at(((List_Checkout_Step*)&spite_configured_Checkout_steps_0), index_); if (__builtin_expect(!spite_temp_9.has_value, 0)) spite_outside_list("steps[index]", spite_site_5()); spite_temp_9.value; }) == Checkout_Step_rounding)), 0))) {
            total_ = ((total_ / 100) * 100);
        }
        if ((((void)((({ Nullable_Checkout_Step spite_temp_10 = List_Checkout_Step_get_at(((List_Checkout_Step*)&spite_configured_Checkout_steps_0), index_); if (__builtin_expect(!spite_temp_10.has_value, 0)) spite_outside_list("steps[index]", spite_site_6()); spite_temp_10.value; }) == Checkout_Step_coupon)), 0))) {
            total_ = ({ int32_t spite_temp_11 = total_; int32_t spite_temp_12 = 50; int32_t spite_temp_13; if (__builtin_expect(__builtin_sub_overflow(spite_temp_11, spite_temp_12, &spite_temp_13), 0)) spite_overflowed("total - 50", "an Integer", "-", (int64_t)spite_temp_11, (int64_t)spite_temp_12, spite_site_7()); spite_temp_13; });
        }
        index_ = (index_ + 1);
    }
    int32_t spite_temp_14 = total_;
    return spite_temp_14;
}
