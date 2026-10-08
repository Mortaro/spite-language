/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int64_t Naive_simulate___held_0(Naive* self, List_Body* bodies_) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t round_ = 0;
    while (((round_ < 100))) {
        
        
        
        
        
        
        
        List_Body_each_advance(bodies_);
        
        
        
        int32_t positions_ = List_Body_sum_position(bodies_);
        total_ = ({ int64_t spite_temp_1 = total_; int64_t spite_temp_2 = SpiteInteger_to_long(positions_); int64_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("total + positions", "a Long", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
        round_ = (round_ + 1);
    }
    int64_t spite_temp_4 = total_;
    return spite_temp_4;
}

void List_Body_each_advance(List_Body* self) {
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        Body* item_ = ((Body**)(intptr_t)self->items_)[index_];
        Body_advance(item_);
        index_ = (index_ + 1);
    }
}

int32_t List_Body_sum_position(List_Body* self) {
    int32_t total_ = 0;
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        Body* item_ = ((Body**)(intptr_t)self->items_)[index_];
        total_ = ({ int32_t spite_temp_5 = total_; int32_t spite_temp_6 = (item_)->position_; int32_t spite_temp_7; if (__builtin_expect(__builtin_add_overflow(spite_temp_5, spite_temp_6, &spite_temp_7), 0)) spite_overflowed("total + item.attributes[member]", "an Integer", "+", (int64_t)spite_temp_5, (int64_t)spite_temp_6, spite_site_2()); spite_temp_7; });
        index_ = (index_ + 1);
    }
    int32_t spite_temp_8 = total_;
    return spite_temp_8;
}
