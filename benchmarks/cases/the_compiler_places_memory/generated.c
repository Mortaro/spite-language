/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int64_t Naive_sum_of_squares(Naive* self, int32_t count_, int32_t offset_) {
    int64_t spite_temp_1[32];
    int64_t spite_temp_2 = SpiteInteger_to_long(({ int32_t spite_temp_3 = count_; int32_t spite_temp_4 = 8; int32_t spite_temp_5; if (__builtin_expect(__builtin_mul_overflow(spite_temp_3, spite_temp_4, &spite_temp_5), 0)) spite_overflowed("count * 8", "an Integer", "*", (int64_t)spite_temp_3, (int64_t)spite_temp_4, spite_site_1()); spite_temp_5; }));
    int64_t squares_ = spite_temp_2 <= 256 ? (int64_t)(intptr_t)spite_temp_1 : Memory_Heap_allocate(self->heap_, spite_temp_2);
    int32_t index_ = 0;
    while (((index_ < count_))) {
        int64_t value_ = SpiteInteger_to_long(({ int32_t spite_temp_6 = index_; int32_t spite_temp_7 = offset_; int32_t spite_temp_8; if (__builtin_expect(__builtin_add_overflow(spite_temp_6, spite_temp_7, &spite_temp_8), 0)) spite_overflowed("index + offset", "an Integer", "+", (int64_t)spite_temp_6, (int64_t)spite_temp_7, spite_site_2()); spite_temp_8; }));
        TypedMemory__Long_write_value(self->longs_, squares_, index_, ({ int64_t spite_temp_9 = value_; int64_t spite_temp_10 = value_; int64_t spite_temp_11; if (__builtin_expect(__builtin_mul_overflow(spite_temp_9, spite_temp_10, &spite_temp_11), 0)) spite_overflowed("value * value", "a Long", "*", (int64_t)spite_temp_9, (int64_t)spite_temp_10, spite_site_3()); spite_temp_11; }));
        index_ = (index_ + 1);
    }
    int64_t total_ = SpiteInteger_to_long(0);
    index_ = 0;
    while (((index_ < count_))) {
        total_ = ({ int64_t spite_temp_12 = total_; int64_t spite_temp_13 = TypedMemory__Long_read_value(self->longs_, squares_, index_); int64_t spite_temp_14; if (__builtin_expect(__builtin_add_overflow(spite_temp_12, spite_temp_13, &spite_temp_14), 0)) spite_overflowed("total + longs.read_value(squares, index)", "a Long", "+", (int64_t)spite_temp_12, (int64_t)spite_temp_13, spite_site_4()); spite_temp_14; });
        index_ = (index_ + 1);
    }
    if (squares_ != (int64_t)(intptr_t)spite_temp_1) Memory_Heap_free(self->heap_, squares_);
    int64_t spite_temp_15 = total_;
    return spite_temp_15;
}
