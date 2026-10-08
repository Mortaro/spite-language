/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Naive_window_total___held_0(Naive* self, List_Integer* values_, int32_t weight_) {
    int32_t total_ = 0;
    int32_t at_ = 0;
    int32_t spite_temp_1 = List_Integer_count(values_);
    int32_t* spite_temp_2 = (int32_t*)(intptr_t)(values_)->items_;
    while (at_ < spite_temp_1 - 2) {
        #if defined(__clang__)
        #pragma clang fp contract(fast) reassociate(on)
        #endif
        total_ = ({ int32_t spite_temp_3 = ({ int32_t spite_temp_4 = ({ int32_t spite_temp_5 = total_; int32_t spite_temp_6 = ({ int32_t spite_temp_7 = spite_temp_2[at_]; int32_t spite_temp_8 = weight_; int32_t spite_temp_9; if (__builtin_expect(__builtin_mul_overflow(spite_temp_7, spite_temp_8, &spite_temp_9), 0)) spite_overflowed("values[at] * weight", "an Integer", "*", (int64_t)spite_temp_7, (int64_t)spite_temp_8, spite_site_1()); spite_temp_9; }); int32_t spite_temp_10; if (__builtin_expect(__builtin_add_overflow(spite_temp_5, spite_temp_6, &spite_temp_10), 0)) spite_overflowed("total + values[at] * weight", "an Integer", "+", (int64_t)spite_temp_5, (int64_t)spite_temp_6, spite_site_1()); spite_temp_10; }); int32_t spite_temp_11 = spite_temp_2[at_ + 1]; int32_t spite_temp_12; if (__builtin_expect(__builtin_add_overflow(spite_temp_4, spite_temp_11, &spite_temp_12), 0)) spite_overflowed("total + values[at] * weight + values[at + 1]", "an Integer", "+", (int64_t)spite_temp_4, (int64_t)spite_temp_11, spite_site_1()); spite_temp_12; }); int32_t spite_temp_13 = spite_temp_2[at_ + 2]; int32_t spite_temp_14; if (__builtin_expect(__builtin_add_overflow(spite_temp_3, spite_temp_13, &spite_temp_14), 0)) spite_overflowed("total + values[at] * weight + values[at + 1] + values[at + 2]", "an Integer", "+", (int64_t)spite_temp_3, (int64_t)spite_temp_13, spite_site_1()); spite_temp_14; });
        at_ = (at_ + 1);
    }
    int32_t spite_temp_15 = total_;
    return spite_temp_15;
}

int32_t Naive_rise_count___held_0(Naive* self, List_Integer* values_, int32_t threshold_) {
    int32_t count_ = List_Integer_count(values_);
    int32_t rises_ = 0;
    int32_t previous_ = 0;
    int32_t index_ = 0;
    while (((index_ < count_))) {
        int32_t value_ = ({ Nullable_Integer spite_temp_16 = List_Integer_get_at(values_, index_); if (__builtin_expect(!spite_temp_16.has_value, 0)) spite_outside_list("values[index]", spite_site_2()); spite_temp_16.value; });
        if (((({ int32_t spite_temp_17 = value_; int32_t spite_temp_18 = previous_; int32_t spite_temp_19; if (__builtin_expect(__builtin_sub_overflow(spite_temp_17, spite_temp_18, &spite_temp_19), 0)) spite_overflowed("value - previous", "an Integer", "-", (int64_t)spite_temp_17, (int64_t)spite_temp_18, spite_site_3()); spite_temp_19; }) > threshold_))) {
            rises_ = ({ int32_t spite_temp_20 = rises_; int32_t spite_temp_21 = 1; int32_t spite_temp_22; if (__builtin_expect(__builtin_add_overflow(spite_temp_20, spite_temp_21, &spite_temp_22), 0)) spite_overflowed("rises + 1", "an Integer", "+", (int64_t)spite_temp_20, (int64_t)spite_temp_21, spite_site_4()); spite_temp_22; });
        }
        previous_ = value_;
        index_ = (index_ + 1);
    }
    int32_t spite_temp_23 = rises_;
    return spite_temp_23;
}

Nullable_Integer List_Integer_get_at(List_Integer* self, int32_t index_) {
    if ((((index_ >= 0))) && (((index_ < self->item_count_)))) {
        Nullable_Integer spite_temp_24 = ((Nullable_Integer){ .has_value = true, .value = TypedMemory__Integer_read_value(self->values_, self->items_, index_) });
        return spite_temp_24;
    }
    Nullable_Integer spite_temp_25 = ((Nullable_Integer){ .has_value = false, .value = 0 });
    return spite_temp_25;
}
