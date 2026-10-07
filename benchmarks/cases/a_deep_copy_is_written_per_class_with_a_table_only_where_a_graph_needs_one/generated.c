/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int64_t Naive_copy_rounds___held_0(Naive* self, List_Order* orders_) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t round_ = 0;
    while (((round_ < 40))) {
        List_Order* copies_ = List_Order___deep_copy(orders_);
        int32_t value_ = List_Order_sum_value(copies_);
        total_ = ({ int64_t spite_temp_1 = total_; int64_t spite_temp_2 = SpiteInteger_to_long(value_); int64_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("total + value", "a Long", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
        round_ = (round_ + 1);
        List_Order___release(copies_);
    }
    int64_t spite_temp_4 = total_;
    return spite_temp_4;
}

List_Order* List_Order___deep_copy(List_Order* self) {
    if (self == 0) return 0;
    List_Order* copied = List_Order___make();
    for (int64_t spite_index = 0; spite_index < self->item_count_; spite_index = spite_index + 1) { List_Order_append(copied, Order___deep_copy(((Order**)(intptr_t)(self)->items_)[spite_index])); }
    return copied;
}

Order* Order___deep_copy(Order* self) {
    if (self == 0) return 0;
    Order* copied = Order___allocate();
    
    copied->id_ = self->id_;
    Customer___release(copied->customer_);
    copied->customer_ = Customer___deep_copy(self->customer_);
    List_Line___release(copied->lines_);
    copied->lines_ = List_Line___deep_copy(self->lines_);
    return copied;
}

Customer* Customer___deep_copy(Customer* self) {
    if (self == 0) return 0;
    Customer* copied = Customer___allocate();
    SpiteString___release(copied->name_);
    copied->name_ = SpiteString___retain(self->name_);
    
    copied->level_ = self->level_;
    return copied;
}

List_Line* List_Line___deep_copy(List_Line* self) {
    if (self == 0) return 0;
    List_Line* copied = List_Line___make();
    for (int64_t spite_index = 0; spite_index < self->item_count_; spite_index = spite_index + 1) { List_Line_append(copied, Line___deep_copy(((Line**)(intptr_t)(self)->items_)[spite_index])); }
    return copied;
}

Line* Line___deep_copy(Line* self) {
    if (self == 0) return 0;
    Line* copied = Line___allocate();
    
    copied->quantity_ = self->quantity_;
    
    copied->price_ = self->price_;
    return copied;
}

void Order___init(Order* self) {
    self->id_ = 0;
    self->customer_ = Customer___make(spite_lit_1, 0);
    self->lines_ = List_Line___make();
}
