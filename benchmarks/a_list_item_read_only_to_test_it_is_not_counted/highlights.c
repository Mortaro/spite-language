/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Naive_probe(Naive* self, int32_t probes_) {
    int32_t found_ = 0;
    int32_t missing_ = 0;
    int32_t at_ = 0;
    int32_t index_ = 0;
    while (((index_ < probes_))) {
        at_ = (({ int32_t spite_temp_1 = at_; int32_t spite_temp_2 = 7919; int32_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("at + 7919", "an Integer", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; }) % 400000);
        if (({ List_Entry* spite_temp_4 = self->entries_; int32_t spite_temp_5 = at_; (spite_temp_5 >= 0 && spite_temp_5 < (spite_temp_4)->item_count_) && ((((Entry**)(intptr_t)(spite_temp_4)->items_)[spite_temp_5]) != 0); })) {
            found_ = ({ int32_t spite_temp_6 = found_; int32_t spite_temp_7 = 1; int32_t spite_temp_8; if (__builtin_expect(__builtin_add_overflow(spite_temp_6, spite_temp_7, &spite_temp_8), 0)) spite_overflowed("found + 1", "an Integer", "+", (int64_t)spite_temp_6, (int64_t)spite_temp_7, spite_site_2()); spite_temp_8; });
        }
        else {
            missing_ = ({ int32_t spite_temp_9 = missing_; int32_t spite_temp_10 = 1; int32_t spite_temp_11; if (__builtin_expect(__builtin_add_overflow(spite_temp_9, spite_temp_10, &spite_temp_11), 0)) spite_overflowed("missing + 1", "an Integer", "+", (int64_t)spite_temp_9, (int64_t)spite_temp_10, spite_site_3()); spite_temp_11; });
        }
        index_ = (index_ + 1);
    }
    if (!(((({ int32_t spite_temp_12 = found_; int32_t spite_temp_13 = missing_; int32_t spite_temp_14; if (__builtin_expect(__builtin_add_overflow(spite_temp_12, spite_temp_13, &spite_temp_14), 0)) spite_overflowed("found + missing", "an Integer", "+", (int64_t)spite_temp_12, (int64_t)spite_temp_13, spite_site_4()); spite_temp_14; }) == probes_)))) {
        spite_failed_1(found_, missing_, probes_, at_, index_);
    }
    int32_t spite_temp_15 = found_;
    return spite_temp_15;
}
