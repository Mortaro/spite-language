/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Naive_total_of___held_0_1(Naive* self, List_Integer* prices_, List_Integer* picks_) {
    int32_t total_ = 0;
    int32_t index_ = 0;
    int32_t spite_temp_1 = List_Integer_count(picks_);
    int32_t* spite_temp_2 = (int32_t*)(intptr_t)(picks_)->items_;
    while (index_ < spite_temp_1) {
        #if defined(__clang__)
        #pragma clang fp contract(fast) reassociate(on)
        #endif
        int32_t pick_ = spite_temp_2[index_];
        if (!(((List_Integer_get_at(prices_, pick_)).has_value))) {
            spite_failed_1(pick_, prices_, total_, index_);
        }
        total_ = ({ int32_t spite_temp_3 = total_; int32_t spite_temp_4 = (List_Integer_get_at(prices_, pick_)).value; int32_t spite_temp_5; if (__builtin_expect(__builtin_add_overflow(spite_temp_3, spite_temp_4, &spite_temp_5), 0)) spite_overflowed("total + prices[pick]", "an Integer", "+", (int64_t)spite_temp_3, (int64_t)spite_temp_4, spite_site_1()); spite_temp_5; });
        index_ = (index_ + 1);
    }
    int32_t spite_temp_6 = total_;
    return spite_temp_6;
}

Nullable_Integer Naive_price_at___held_0(Naive* self, List_Integer* prices_, int32_t place_) {
    if (!(((place_ < List_Integer_count(prices_))))) {
        SPITE_TRACE_ASSERT(spite_site_2());
        return ((Nullable_Integer){ .has_value = false, .value = 0 });
    }
    Nullable_Integer spite_temp_7 = ((Nullable_Integer){ .has_value = true, .value = ({ Nullable_Integer spite_temp_8 = List_Integer_get_at(prices_, place_); if (__builtin_expect(!spite_temp_8.has_value, 0)) spite_outside_list("prices[place]", spite_site_3()); spite_temp_8.value; }) });
    return spite_temp_7;
}
