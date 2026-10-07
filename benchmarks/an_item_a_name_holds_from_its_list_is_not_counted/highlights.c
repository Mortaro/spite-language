/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void Naive_visit(Naive* self, int32_t index_) {
    if (!(({ List_Column* spite_temp_1 = self->columns_; int32_t spite_temp_2 = index_; (spite_temp_2 >= 0 && spite_temp_2 < (spite_temp_1)->item_count_) && ((((Column**)(intptr_t)(spite_temp_1)->items_)[spite_temp_2]) != 0); }))) {
        spite_failed_1(index_, self);
    }
    Column* column_ = ({ List_Column* spite_temp_3 = self->columns_; int32_t spite_temp_4 = index_; if (__builtin_expect(spite_temp_4 < 0 || spite_temp_4 >= (spite_temp_3)->item_count_, 0)) spite_outside_list("columns[index]", spite_site_1()); ((Column**)(intptr_t)(spite_temp_3)->items_)[spite_temp_4]; });
    Column_hit(column_);
    List_Integer_set_at(self->weights_, index_, Naive_weight___held_0(self, column_));
}

int32_t Naive_weight___held_0(Naive* self, Column* column_) {
    int32_t spite_temp_5 = ({ int32_t spite_temp_6 = (column_)->hits_; int32_t spite_temp_7 = 10; int32_t spite_temp_8; if (__builtin_expect(__builtin_mul_overflow(spite_temp_6, spite_temp_7, &spite_temp_8), 0)) spite_overflowed("column.hits * 10", "an Integer", "*", (int64_t)spite_temp_6, (int64_t)spite_temp_7, spite_site_2()); spite_temp_8; });
    return spite_temp_5;
}
