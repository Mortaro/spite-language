/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Naive_points_for___held_0(Naive* self, Result* result_) {
    if (!(((((Nullable_Integer){ .has_value = true, .value = 50 })).has_value))) {
        spite_failed_1();
    }
    if (!(((((Nullable_Integer){ .has_value = true, .value = 20 })).has_value))) {
        spite_failed_2();
    }
    if (!(((((Nullable_Integer){ .has_value = true, .value = 5 })).has_value))) {
        spite_failed_3();
    }
    int32_t spite_temp_1 = ({ int32_t spite_temp_2 = ({ int32_t spite_temp_3 = ({ int32_t spite_temp_4 = (result_)->golds_; int32_t spite_temp_5 = (((Nullable_Integer){ .has_value = true, .value = 50 })).value; int32_t spite_temp_6; if (__builtin_expect(__builtin_mul_overflow(spite_temp_4, spite_temp_5, &spite_temp_6), 0)) spite_overflowed("result.golds * points[\"gold\"]", "an Integer", "*", (int64_t)spite_temp_4, (int64_t)spite_temp_5, spite_site_1()); spite_temp_6; }); int32_t spite_temp_7 = ({ int32_t spite_temp_8 = (result_)->silvers_; int32_t spite_temp_9 = (((Nullable_Integer){ .has_value = true, .value = 20 })).value; int32_t spite_temp_10; if (__builtin_expect(__builtin_mul_overflow(spite_temp_8, spite_temp_9, &spite_temp_10), 0)) spite_overflowed("result.silvers * points[\"silver\"]", "an Integer", "*", (int64_t)spite_temp_8, (int64_t)spite_temp_9, spite_site_1()); spite_temp_10; }); int32_t spite_temp_11; if (__builtin_expect(__builtin_add_overflow(spite_temp_3, spite_temp_7, &spite_temp_11), 0)) spite_overflowed("result.golds * points[\"gold\"] + result.silvers * points[\"silver\"]", "an Integer", "+", (int64_t)spite_temp_3, (int64_t)spite_temp_7, spite_site_1()); spite_temp_11; }); int32_t spite_temp_12 = ({ int32_t spite_temp_13 = (result_)->bronzes_; int32_t spite_temp_14 = (((Nullable_Integer){ .has_value = true, .value = 5 })).value; int32_t spite_temp_15; if (__builtin_expect(__builtin_mul_overflow(spite_temp_13, spite_temp_14, &spite_temp_15), 0)) spite_overflowed("result.bronzes * points[\"bronze\"]", "an Integer", "*", (int64_t)spite_temp_13, (int64_t)spite_temp_14, spite_site_1()); spite_temp_15; }); int32_t spite_temp_16; if (__builtin_expect(__builtin_add_overflow(spite_temp_2, spite_temp_12, &spite_temp_16), 0)) spite_overflowed("result.golds * points[\"gold\"] + result.silvers * points[\"silver\"] + result.bronzes * points[\"bronze\"]", "an Integer", "+", (int64_t)spite_temp_2, (int64_t)spite_temp_12, spite_site_1()); spite_temp_16; });
    return spite_temp_1;
}

int64_t Naive_totals___held_0(Naive* self, List_Result* results_) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t round_ = 0;
    while (((round_ < 40))) {
        int32_t index_ = 0;
        while (((index_ < spite_folded_List_Result_count(results_)))) {
            Result* result_ = ({ List_Result* spite_temp_17 = results_; int32_t spite_temp_18 = index_; if (__builtin_expect(spite_temp_18 < 0 || spite_temp_18 >= (spite_temp_17)->item_count_, 0)) spite_outside_list("results[index]", spite_site_2()); ((Result**)(intptr_t)(spite_temp_17)->items_)[spite_temp_18]; });
            int32_t points_ = Naive_points_for___held_0(self, result_);
            total_ = ({ int64_t spite_temp_19 = total_; int64_t spite_temp_20 = SpiteInteger_to_long(points_); int64_t spite_temp_21; if (__builtin_expect(__builtin_add_overflow(spite_temp_19, spite_temp_20, &spite_temp_21), 0)) spite_overflowed("total + points", "a Long", "+", (int64_t)spite_temp_19, (int64_t)spite_temp_20, spite_site_3()); spite_temp_21; });
            index_ = (index_ + 1);
        }
        round_ = (round_ + 1);
    }
    int64_t spite_temp_22 = total_;
    return spite_temp_22;
}
