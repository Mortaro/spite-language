/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Naive_largest_remainder(Naive* self, int32_t round_) {
    int32_t spite_framed_1_items[4];
    spite_framed_1_items[0] = (round_ % 10);
    spite_framed_1_items[1] = (round_ % 7);
    spite_framed_1_items[2] = (round_ % 13);
    spite_framed_1_items[3] = (round_ % 3);
    List_Integer spite_framed_1;
    List_Integer* remainders_ = List_Integer___framed(&spite_framed_1, (int64_t)(intptr_t)spite_framed_1_items, 4);
    static const int32_t spite_framed_2_items[4] = { 4, 3, 2, 1 };
    List_Integer spite_framed_2;
    List_Integer* weights_ = List_Integer___framed(&spite_framed_2, (int64_t)(intptr_t)spite_framed_2_items, 4);
    int32_t biggest_ = 0;
    int32_t index_ = 0;
    int32_t spite_temp_1 = List_Integer_count(remainders_);
    int32_t* spite_temp_2 = (int32_t*)(intptr_t)(remainders_)->items_;
    while (index_ < spite_temp_1) {
        #if defined(__clang__)
        #pragma clang fp contract(fast) reassociate(on)
        #endif
        if (!(((List_Integer_get_at(weights_, index_)).has_value))) {
            spite_failed_1(index_, weights_, round_, biggest_);
        }
        int32_t weighted_ = ({ int32_t spite_temp_3 = spite_temp_2[index_]; int32_t spite_temp_4 = (List_Integer_get_at(weights_, index_)).value; int32_t spite_temp_5; if (__builtin_expect(__builtin_mul_overflow(spite_temp_3, spite_temp_4, &spite_temp_5), 0)) spite_overflowed("remainders[index] * weights[index]", "an Integer", "*", (int64_t)spite_temp_3, (int64_t)spite_temp_4, spite_site_1()); spite_temp_5; });
        if (((weighted_ > biggest_))) {
            biggest_ = weighted_;
        }
        index_ = (index_ + 1);
    }
    int32_t spite_temp_6 = biggest_;
    return spite_temp_6;
}

static List_Integer* List_Integer___framed(List_Integer* self, int64_t items, int32_t count) {
    List_Integer___init(self);
    self->header.ref_count = 2;
    self->header.class_id = 119;
    self->items_ = items;
    self->item_count_ = count;
    self->capacity_ = count;
    return self;
}
