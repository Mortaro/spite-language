/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

struct Sale {
    SpiteHeader header;
    int32_t region_;
    int32_t product_;
    int32_t quantity_;
    int32_t price_;
    int32_t cost_;
    int32_t discount_;
    int32_t day_;
    int32_t customer_;
};

int64_t Naive_profit_of___held_0(Naive* self, List_Sale* sales_, int32_t region_, int32_t quarter_) {
    int64_t profit_ = SpiteInteger_to_long(0);
    int32_t index_ = 0;
    while (((index_ < spite_folded_List_Sale_count(sales_)))) {
        Sale* sale_ = ({ List_Sale* spite_temp_1 = sales_; int32_t spite_temp_2 = index_; if (__builtin_expect(spite_temp_2 < 0 || spite_temp_2 >= (spite_temp_1)->item_count_, 0)) spite_outside_list("sales[index]", spite_site_1()); ((Sale**)(intptr_t)(spite_temp_1)->items_)[spite_temp_2]; });
        if (((((sale_)->region_ == region_))) && (((((sale_)->day_ / 92) == quarter_)))) {
            profit_ = (profit_ + SpiteInteger_to_long(({ int32_t spite_temp_3 = (sale_)->quantity_; int32_t spite_temp_4 = ({ int32_t spite_temp_5 = (sale_)->price_; int32_t spite_temp_6 = (sale_)->cost_; int32_t spite_temp_7; if (__builtin_expect(__builtin_sub_overflow(spite_temp_5, spite_temp_6, &spite_temp_7), 0)) spite_overflowed("sale.price - sale.cost", "an Integer", "-", (int64_t)spite_temp_5, (int64_t)spite_temp_6, spite_site_2()); spite_temp_7; }); int32_t spite_temp_8; if (__builtin_expect(__builtin_mul_overflow(spite_temp_3, spite_temp_4, &spite_temp_8), 0)) spite_overflowed("sale.quantity * (sale.price - sale.cost)", "an Integer", "*", (int64_t)spite_temp_3, (int64_t)spite_temp_4, spite_site_2()); spite_temp_8; })));
        }
        index_ = (index_ + 1);
    }
    int64_t spite_temp_9 = profit_;
    return spite_temp_9;
}

int32_t List_Sale_sum_quantity(List_Sale* self) {
    int32_t total_ = 0;
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        Sale* item_ = ((Sale**)(intptr_t)self->items_)[index_];
        total_ = ({ int32_t spite_temp_10 = total_; int32_t spite_temp_11 = (item_)->quantity_; int32_t spite_temp_12; if (__builtin_expect(__builtin_add_overflow(spite_temp_10, spite_temp_11, &spite_temp_12), 0)) spite_overflowed("total + item.attributes[member]", "an Integer", "+", (int64_t)spite_temp_10, (int64_t)spite_temp_11, spite_site_3()); spite_temp_12; });
        index_ = (index_ + 1);
    }
    int32_t spite_temp_13 = total_;
    return spite_temp_13;
}

int64_t List_Sale_sum_fingerprint(List_Sale* self) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        Sale* item_ = ((Sale**)(intptr_t)self->items_)[index_];
        total_ = ({ int64_t spite_temp_14 = total_; int64_t spite_temp_15 = Sale_fingerprint(item_); int64_t spite_temp_16; if (__builtin_expect(__builtin_add_overflow(spite_temp_14, spite_temp_15, &spite_temp_16), 0)) spite_overflowed("total + item.attributes[member]", "a Long", "+", (int64_t)spite_temp_14, (int64_t)spite_temp_15, spite_site_4()); spite_temp_16; });
        index_ = (index_ + 1);
    }
    int64_t spite_temp_17 = total_;
    return spite_temp_17;
}
