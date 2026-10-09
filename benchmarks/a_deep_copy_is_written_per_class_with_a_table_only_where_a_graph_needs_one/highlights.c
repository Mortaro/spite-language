/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int64_t Naive_copy_rounds___held_0(Naive* self, List_Order* orders_) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t round_ = 0;
    while (((round_ < 40))) {
        List_Order* copies_ = spite_copy_site_1(orders_);
        int32_t value_ = List_Order_sum_value(copies_);
        total_ = (total_ + SpiteInteger_to_long(value_));
        round_ = (round_ + 1);
        List_Order___release(copies_);
    }
    int64_t spite_temp_1 = total_;
    return spite_temp_1;
}

static inline List_Order* spite_copy_site_1(List_Order* self) {
    return List_Order___retain(self);
}
