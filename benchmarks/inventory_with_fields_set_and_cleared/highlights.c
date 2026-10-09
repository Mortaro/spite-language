/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

struct Record {
    SpiteHeader header;
    int32_t price_;
    int32_t stock_;
    Supplier* supplier_;
    Reservation* reservation_;
};

void Record_toggle(Record* self, int64_t seed_) {
    if (((self->reservation_) != 0)) {
        self->stock_ = ({ int32_t spite_temp_1 = self->stock_; int32_t spite_temp_2 = (self->reservation_)->quantity_; int32_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("stock + reservation.quantity", "an Integer", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
        Reservation* spite_temp_4 = 0;
        Reservation___release(self->reservation_);
        self->reservation_ = spite_temp_4;
    }
    else {
        Reservation* made_ = Reservation___make(seed_);
        Reservation* spite_temp_5 = Reservation___retain(made_);
        Reservation___release(self->reservation_);
        self->reservation_ = spite_temp_5;
        Reservation___release(made_);
    }
}

int64_t Record_late(Record* self) {
    if ((((self->supplier_) != 0)) && (((self->reservation_) != 0))) {
        int64_t days_ = SpiteInteger_to_long((self->supplier_)->lead_days_);
        int64_t spite_temp_6 = ({ int64_t spite_temp_7 = days_; int64_t spite_temp_8 = SpiteInteger_to_long((self->reservation_)->quantity_); int64_t spite_temp_9; if (__builtin_expect(__builtin_mul_overflow(spite_temp_7, spite_temp_8, &spite_temp_9), 0)) spite_overflowed("days * reservation.quantity", "a Long", "*", (int64_t)spite_temp_7, (int64_t)spite_temp_8, spite_site_2()); spite_temp_9; });
        return spite_temp_6;
    }
    int64_t spite_temp_10 = SpiteInteger_to_long(0);
    return spite_temp_10;
}

int64_t List_Record_sum_reserved(List_Record* self) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        Record* item_ = ((Record**)(intptr_t)self->items_)[index_];
        total_ = ({ int64_t spite_temp_11 = total_; int64_t spite_temp_12 = Record_reserved(item_); int64_t spite_temp_13; if (__builtin_expect(__builtin_add_overflow(spite_temp_11, spite_temp_12, &spite_temp_13), 0)) spite_overflowed("total + item.attributes[member]", "a Long", "+", (int64_t)spite_temp_11, (int64_t)spite_temp_12, spite_site_3()); spite_temp_13; });
        index_ = (index_ + 1);
    }
    int64_t spite_temp_14 = total_;
    return spite_temp_14;
}
