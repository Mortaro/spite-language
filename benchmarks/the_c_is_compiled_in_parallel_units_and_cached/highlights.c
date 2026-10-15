/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Item_worth(Item* self) {
    int32_t spite_high_1 = (self)->price_;
    int32_t spite_high_2 = (self)->count_;
    int32_t spite_high_3_left = spite_high_1;
    int32_t spite_high_3_right = spite_high_2;
    int32_t spite_high_3;
    if (__builtin_expect(__builtin_mul_overflow(spite_high_3_left, spite_high_3_right, &spite_high_3), 0)) spite_overflowed("price * count", "an Integer", "*", (int64_t)spite_high_3_left, (int64_t)spite_high_3_right, spite_site_1());
    return spite_high_3;
}

int32_t List_Item_sum_worth(List_Item* self) {
    int32_t total_ = 0;
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        Item* item_ = ((Item**)(intptr_t)self->items_)[index_];
        total_ = ({ int32_t spite_temp_1 = total_; int32_t spite_temp_2 = Item_worth(item_); int32_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("total + item.attributes[member]", "an Integer", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_2()); spite_temp_3; });
        index_ = (index_ + 1);
    }
    int32_t spite_temp_4 = total_;
    return spite_temp_4;
}
