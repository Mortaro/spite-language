/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Naive_probe(Naive* self, int32_t probes_) {
    int32_t found_ = 0;
    int32_t missing_ = 0;
    int32_t at_ = 0;
    int32_t index_ = 0;
    while (((index_ < probes_))) {
        at_ = ((at_ + 7919) % 400000);
        if (({ List_Entry* spite_temp_1 = self->entries_; int32_t spite_temp_2 = at_; (spite_temp_2 >= 0 && spite_temp_2 < (spite_temp_1)->item_count_) && ((((Entry**)(intptr_t)(spite_temp_1)->items_)[spite_temp_2]) != 0); })) {
            found_ = (found_ + 1);
        }
        else {
            missing_ = (missing_ + 1);
        }
        index_ = (index_ + 1);
    }
    if (!(((({ int32_t spite_temp_3 = found_; int32_t spite_temp_4 = missing_; int32_t spite_temp_5; if (__builtin_expect(__builtin_add_overflow(spite_temp_3, spite_temp_4, &spite_temp_5), 0)) spite_overflowed("found + missing", "an Integer", "+", (int64_t)spite_temp_3, (int64_t)spite_temp_4, spite_site_1()); spite_temp_5; }) == probes_)))) {
        spite_failed_1(found_, missing_, probes_, at_, index_, self);
    }
    int32_t spite_temp_6 = found_;
    return spite_temp_6;
}
