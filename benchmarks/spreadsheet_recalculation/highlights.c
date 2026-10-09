/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

struct Cell {
    SpiteHeader header;
    Cell_Formula formula_;
    int32_t first_;
    int32_t second_;
    int64_t constant_;
    int64_t value_;
    int32_t row_;
    int32_t column_;
    int32_t style_;
};

void Naive_recalculate___held_0(Naive* self, List_Cell* cells_) {
    int32_t index_ = 0;
    while (((index_ < spite_folded_List_Cell_count(cells_)))) {
        Cell* cell_ = ({ List_Cell* spite_temp_1 = cells_; int32_t spite_temp_2 = index_; if (__builtin_expect(spite_temp_2 < 0 || spite_temp_2 >= (spite_temp_1)->item_count_, 0)) spite_outside_list("cells[index]", spite_site_1()); ((Cell**)(intptr_t)(spite_temp_1)->items_)[spite_temp_2]; });
        if (!(({ List_Cell* spite_temp_3 = cells_; int32_t spite_temp_4 = (cell_)->first_; (spite_temp_4 >= 0 && spite_temp_4 < (spite_temp_3)->item_count_) && ((((Cell**)(intptr_t)(spite_temp_3)->items_)[spite_temp_4]) != 0); }))) {
            spite_failed_1(cell_, cells_, index_, self);
        }
        if (!(({ List_Cell* spite_temp_5 = cells_; int32_t spite_temp_6 = (cell_)->second_; (spite_temp_6 >= 0 && spite_temp_6 < (spite_temp_5)->item_count_) && ((((Cell**)(intptr_t)(spite_temp_5)->items_)[spite_temp_6]) != 0); }))) {
            spite_failed_2(cell_, cells_, index_, self);
        }
        int64_t left_ = ({ Cell* spite_temp_7 = List_Cell_get_at(cells_, (cell_)->first_); int64_t spite_temp_8 = (spite_temp_7)->value_; Cell___release(spite_temp_7); spite_temp_8; });
        int64_t right_ = ({ Cell* spite_temp_9 = List_Cell_get_at(cells_, (cell_)->second_); int64_t spite_temp_10 = (spite_temp_9)->value_; Cell___release(spite_temp_9); spite_temp_10; });
        (cell_)->value_ = Cell_computed(cell_, left_, right_);
        index_ = (index_ + 1);
    }
}

int64_t Cell_computed(Cell* self, int64_t left_, int64_t right_) {
    {
        Cell_Formula spite_temp_11 = self->formula_;
        if (spite_temp_11 == Cell_Formula_constant) {
            int64_t spite_temp_12 = self->constant_;
            return spite_temp_12;
        }
        else if (spite_temp_11 == Cell_Formula_sum) {
            int64_t spite_temp_13 = (({ int64_t spite_temp_14 = left_; int64_t spite_temp_15 = right_; int64_t spite_temp_16; if (__builtin_expect(__builtin_add_overflow(spite_temp_14, spite_temp_15, &spite_temp_16), 0)) spite_overflowed("left + right", "a Long", "+", (int64_t)spite_temp_14, (int64_t)spite_temp_15, spite_site_2()); spite_temp_16; }) % SpiteInteger_to_long(1000003));
            return spite_temp_13;
        }
        else if (spite_temp_11 == Cell_Formula_product) {
            int64_t spite_temp_17 = (({ int64_t spite_temp_18 = left_; int64_t spite_temp_19 = right_; int64_t spite_temp_20; if (__builtin_expect(__builtin_mul_overflow(spite_temp_18, spite_temp_19, &spite_temp_20), 0)) spite_overflowed("left * right", "a Long", "*", (int64_t)spite_temp_18, (int64_t)spite_temp_19, spite_site_3()); spite_temp_20; }) % SpiteInteger_to_long(1000003));
            return spite_temp_17;
        }
        else if (spite_temp_11 == Cell_Formula_scaled) {
            int64_t spite_temp_21 = (({ int64_t spite_temp_22 = left_; int64_t spite_temp_23 = self->constant_; int64_t spite_temp_24; if (__builtin_expect(__builtin_mul_overflow(spite_temp_22, spite_temp_23, &spite_temp_24), 0)) spite_overflowed("left * constant", "a Long", "*", (int64_t)spite_temp_22, (int64_t)spite_temp_23, spite_site_4()); spite_temp_24; }) % SpiteInteger_to_long(1000003));
            return spite_temp_21;
        }
    }
    return 0;
}

int64_t List_Cell_sum_value(List_Cell* self) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        Cell* item_ = ((Cell**)(intptr_t)self->items_)[index_];
        total_ = ({ int64_t spite_temp_25 = total_; int64_t spite_temp_26 = (item_)->value_; int64_t spite_temp_27; if (__builtin_expect(__builtin_add_overflow(spite_temp_25, spite_temp_26, &spite_temp_27), 0)) spite_overflowed("total + item.attributes[member]", "a Long", "+", (int64_t)spite_temp_25, (int64_t)spite_temp_26, spite_site_5()); spite_temp_27; });
        index_ = (index_ + 1);
    }
    int64_t spite_temp_28 = total_;
    return spite_temp_28;
}
