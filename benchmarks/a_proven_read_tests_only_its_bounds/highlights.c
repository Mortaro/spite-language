/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Naive_window_total___held_0(Naive* self, List_Integer* values_, int32_t weight_) {
    int32_t total_ = 0;
    int32_t at_ = 0;
    while (((({ int32_t spite_temp_1 = at_; int32_t spite_temp_2 = 2; int32_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("at + 2", "an Integer", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; }) < List_Integer_count(values_)))) {
        total_ = ({ int32_t spite_temp_4 = ({ int32_t spite_temp_5 = ({ int32_t spite_temp_6 = total_; int32_t spite_temp_7 = ({ int32_t spite_temp_8 = ({ Nullable_Integer spite_temp_9 = List_Integer_get_at(values_, at_); if (__builtin_expect(!spite_temp_9.has_value, 0)) spite_outside_list("values[at]", spite_site_2()); spite_temp_9.value; }); int32_t spite_temp_10 = weight_; int32_t spite_temp_11; if (__builtin_expect(__builtin_mul_overflow(spite_temp_8, spite_temp_10, &spite_temp_11), 0)) spite_overflowed("values[at] * weight", "an Integer", "*", (int64_t)spite_temp_8, (int64_t)spite_temp_10, spite_site_2()); spite_temp_11; }); int32_t spite_temp_12; if (__builtin_expect(__builtin_add_overflow(spite_temp_6, spite_temp_7, &spite_temp_12), 0)) spite_overflowed("total + values[at] * weight", "an Integer", "+", (int64_t)spite_temp_6, (int64_t)spite_temp_7, spite_site_2()); spite_temp_12; }); int32_t spite_temp_13 = ({ Nullable_Integer spite_temp_14 = List_Integer_get_at(values_, ({ int32_t spite_temp_15 = at_; int32_t spite_temp_16 = 1; int32_t spite_temp_17; if (__builtin_expect(__builtin_add_overflow(spite_temp_15, spite_temp_16, &spite_temp_17), 0)) spite_overflowed("at + 1", "an Integer", "+", (int64_t)spite_temp_15, (int64_t)spite_temp_16, spite_site_2()); spite_temp_17; })); if (__builtin_expect(!spite_temp_14.has_value, 0)) spite_outside_list("values[at + 1]", spite_site_2()); spite_temp_14.value; }); int32_t spite_temp_18; if (__builtin_expect(__builtin_add_overflow(spite_temp_5, spite_temp_13, &spite_temp_18), 0)) spite_overflowed("total + values[at] * weight + values[at + 1]", "an Integer", "+", (int64_t)spite_temp_5, (int64_t)spite_temp_13, spite_site_2()); spite_temp_18; }); int32_t spite_temp_19 = ({ Nullable_Integer spite_temp_20 = List_Integer_get_at(values_, ({ int32_t spite_temp_21 = at_; int32_t spite_temp_22 = 2; int32_t spite_temp_23; if (__builtin_expect(__builtin_add_overflow(spite_temp_21, spite_temp_22, &spite_temp_23), 0)) spite_overflowed("at + 2", "an Integer", "+", (int64_t)spite_temp_21, (int64_t)spite_temp_22, spite_site_2()); spite_temp_23; })); if (__builtin_expect(!spite_temp_20.has_value, 0)) spite_outside_list("values[at + 2]", spite_site_2()); spite_temp_20.value; }); int32_t spite_temp_24; if (__builtin_expect(__builtin_add_overflow(spite_temp_4, spite_temp_19, &spite_temp_24), 0)) spite_overflowed("total + values[at] * weight + values[at + 1] + values[at + 2]", "an Integer", "+", (int64_t)spite_temp_4, (int64_t)spite_temp_19, spite_site_2()); spite_temp_24; });
        at_ = ({ int32_t spite_temp_25 = at_; int32_t spite_temp_26 = 1; int32_t spite_temp_27; if (__builtin_expect(__builtin_add_overflow(spite_temp_25, spite_temp_26, &spite_temp_27), 0)) spite_overflowed("at + 1", "an Integer", "+", (int64_t)spite_temp_25, (int64_t)spite_temp_26, spite_site_3()); spite_temp_27; });
    }
    int32_t spite_temp_28 = total_;
    return spite_temp_28;
}

int32_t Naive_rise_count___held_0(Naive* self, List_Integer* values_, int32_t threshold_) {
    int32_t count_ = List_Integer_count(values_);
    int32_t rises_ = 0;
    int32_t previous_ = 0;
    int32_t index_ = 0;
    while (((index_ < count_))) {
        int32_t value_ = ({ Nullable_Integer spite_temp_29 = List_Integer_get_at(values_, index_); if (__builtin_expect(!spite_temp_29.has_value, 0)) spite_outside_list("values[index]", spite_site_4()); spite_temp_29.value; });
        if (((({ int32_t spite_temp_30 = value_; int32_t spite_temp_31 = previous_; int32_t spite_temp_32; if (__builtin_expect(__builtin_sub_overflow(spite_temp_30, spite_temp_31, &spite_temp_32), 0)) spite_overflowed("value - previous", "an Integer", "-", (int64_t)spite_temp_30, (int64_t)spite_temp_31, spite_site_5()); spite_temp_32; }) > threshold_))) {
            rises_ = ({ int32_t spite_temp_33 = rises_; int32_t spite_temp_34 = 1; int32_t spite_temp_35; if (__builtin_expect(__builtin_add_overflow(spite_temp_33, spite_temp_34, &spite_temp_35), 0)) spite_overflowed("rises + 1", "an Integer", "+", (int64_t)spite_temp_33, (int64_t)spite_temp_34, spite_site_6()); spite_temp_35; });
        }
        previous_ = value_;
        index_ = (index_ + 1);
    }
    int32_t spite_temp_36 = rises_;
    return spite_temp_36;
}

Nullable_Integer List_Integer_get_at(List_Integer* self, int32_t index_) {
    if ((((index_ >= 0))) && (((index_ < self->item_count_)))) {
        Nullable_Integer spite_temp_37 = ((Nullable_Integer){ .has_value = true, .value = TypedMemory__Integer_read_value(self->values_, self->items_, index_) });
        return spite_temp_37;
    }
    Nullable_Integer spite_temp_38 = ((Nullable_Integer){ .has_value = false, .value = 0 });
    return spite_temp_38;
}
