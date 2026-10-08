/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int64_t Naive_totals___held_0(Naive* self, List_Item* items_) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t round_ = 0;
    while (((round_ < 100))) {
        int32_t prices_ = List_Item_filter_is_active_then_sum_price(items_);
        int32_t ages_ = List_Item_filter_is_active_then_map_owners_then_filter_is_adult_then_sum_age(items_);
        total_ = ((total_ + SpiteInteger_to_long(prices_)) + SpiteInteger_to_long(ages_));
        round_ = (round_ + 1);
    }
    int64_t spite_temp_1 = total_;
    return spite_temp_1;
}

int32_t List_Item_filter_is_active_then_sum_price(List_Item* self) {
    int32_t total_ = 0;
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        Item* item_ = ((Item**)(intptr_t)self->items_)[index_];
        index_ = (index_ + 1);
        if ((Item_is_active(item_))) {
            total_ = ({ int32_t spite_temp_2 = total_; int32_t spite_temp_3 = (item_)->price_; int32_t spite_temp_4; if (__builtin_expect(__builtin_add_overflow(spite_temp_2, spite_temp_3, &spite_temp_4), 0)) spite_overflowed("total + item.price", "an Integer", "+", (int64_t)spite_temp_2, (int64_t)spite_temp_3, spite_site_1()); spite_temp_4; });
        }
    }
    int32_t spite_temp_5 = total_;
    return spite_temp_5;
}

int32_t List_Item_filter_is_active_then_map_owners_then_filter_is_adult_then_sum_age(List_Item* self) {
    int32_t total_ = 0;
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        Item* item_ = ((Item**)(intptr_t)self->items_)[index_];
        index_ = (index_ + 1);
        if ((Item_is_active(item_))) {
            Owner* mapped_ = (item_)->owner_;
            if ((Owner_is_adult(mapped_))) {
                total_ = ({ int32_t spite_temp_6 = total_; int32_t spite_temp_7 = (mapped_)->age_; int32_t spite_temp_8; if (__builtin_expect(__builtin_add_overflow(spite_temp_6, spite_temp_7, &spite_temp_8), 0)) spite_overflowed("total + mapped.age", "an Integer", "+", (int64_t)spite_temp_6, (int64_t)spite_temp_7, spite_site_2()); spite_temp_8; });
            }
        }
    }
    int32_t spite_temp_9 = total_;
    return spite_temp_9;
}
