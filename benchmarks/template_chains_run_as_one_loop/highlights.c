/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int64_t Naive_totals___held_0(Naive* self, List_Item* items_) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t round_ = 0;
    while (((round_ < 100))) {
        int32_t prices_ = List_Item_filter_is_active_then_sum_price(items_);
        int32_t ages_ = List_Item_filter_is_active_then_map_owners_then_filter_is_adult_then_sum_age(items_);
        total_ = ({ int64_t spite_temp_1 = ({ int64_t spite_temp_2 = total_; int64_t spite_temp_3 = SpiteInteger_to_long(prices_); int64_t spite_temp_4; if (__builtin_expect(__builtin_add_overflow(spite_temp_2, spite_temp_3, &spite_temp_4), 0)) spite_overflowed("total + prices", "a Long", "+", (int64_t)spite_temp_2, (int64_t)spite_temp_3, spite_site_1()); spite_temp_4; }); int64_t spite_temp_5 = SpiteInteger_to_long(ages_); int64_t spite_temp_6; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_5, &spite_temp_6), 0)) spite_overflowed("total + prices + ages", "a Long", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_5, spite_site_1()); spite_temp_6; });
        round_ = (round_ + 1);
    }
    int64_t spite_temp_7 = total_;
    return spite_temp_7;
}

int32_t List_Item_filter_is_active_then_sum_price(List_Item* self) {
    int32_t total_ = 0;
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        Item* item_ = ((Item**)(intptr_t)self->items_)[index_];
        index_ = (index_ + 1);
        if ((Item_is_active(item_))) {
            total_ = ({ int32_t spite_temp_8 = total_; int32_t spite_temp_9 = (item_)->price_; int32_t spite_temp_10; if (__builtin_expect(__builtin_add_overflow(spite_temp_8, spite_temp_9, &spite_temp_10), 0)) spite_overflowed("total + item.price", "an Integer", "+", (int64_t)spite_temp_8, (int64_t)spite_temp_9, spite_site_2()); spite_temp_10; });
        }
    }
    int32_t spite_temp_11 = total_;
    return spite_temp_11;
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
                total_ = ({ int32_t spite_temp_12 = total_; int32_t spite_temp_13 = (mapped_)->age_; int32_t spite_temp_14; if (__builtin_expect(__builtin_add_overflow(spite_temp_12, spite_temp_13, &spite_temp_14), 0)) spite_overflowed("total + mapped.age", "an Integer", "+", (int64_t)spite_temp_12, (int64_t)spite_temp_13, spite_site_3()); spite_temp_14; });
            }
        }
    }
    int32_t spite_temp_15 = total_;
    return spite_temp_15;
}
