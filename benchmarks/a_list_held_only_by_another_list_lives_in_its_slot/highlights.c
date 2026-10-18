/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

List_Order* TypedMemory__List_Order_read_value(TypedMemory__List_Order* self, int64_t address_, int32_t index_) {
    return List_Order___retain(&((List_Order*)(intptr_t)address_)[index_]);
}

void TypedMemory__List_Order_write_value(TypedMemory__List_Order* self, int64_t address_, int32_t index_, List_Order* value_) {
    List_Order* spite_slot = &((List_Order*)(intptr_t)address_)[index_];
    if (spite_slot->header.ref_count == -7) {
        if (value_->items_ == 0) {
            int64_t spite_room = spite_slot->items_;
            int32_t spite_capacity = spite_slot->capacity_;
            memcpy(spite_slot, value_, sizeof(List_Order));
            spite_slot->items_ = spite_room;
            spite_slot->capacity_ = spite_capacity;
            spite_slot->item_count_ = 0;
            spite_slot->header.ref_count = 1073741824;
            List_Order___release(value_);
            return;
        }
        spite_slot->header.ref_count = 0;
        List_Order_drop(spite_slot);
    }
    memcpy(spite_slot, value_, sizeof(List_Order));
    spite_slot->header.ref_count = 1073741824;
    value_->items_ = 0;
    value_->item_count_ = 0;
    value_->capacity_ = 0;
    List_Order___release(value_);
}

void TypedMemory__List_Order_release_value(TypedMemory__List_Order* self, int64_t address_, int32_t index_) {
    List_Order_drop(&((List_Order*)(intptr_t)address_)[index_]);
}

int64_t TypedMemory__List_Order_value_bytes(TypedMemory__List_Order* self) {
    return (int64_t)sizeof(List_Order);
}

void Naive_group(Naive* self) {
    List_List_Order_clear(self->buckets_);
    int32_t made_ = 0;
    while (((made_ < 65536))) {
        List_Order* bucket_ = List_Order___make();
        List_List_Order_append(self->buckets_, List_Order___retain(bucket_));
        made_ = (made_ + 1);
        List_Order___release(bucket_);
    }
    int32_t index_ = 0;
    while (((index_ < List_Order_count(self->orders_)))) {
        Order* order_ = ({ Order* spite_temp_1 = List_Order_get_at(self->orders_, index_); if (__builtin_expect(!(((spite_temp_1) != 0)), 0)) spite_outside_list("orders[index]", spite_site_1()); spite_temp_1; });
        if (!(({ List_Order* spite_temp_2 = List_List_Order_get_at(self->buckets_, (order_)->customer_); int path_narrowed = ((spite_temp_2) != 0); List_Order___release(spite_temp_2); path_narrowed; }))) {
            spite_failed_1(order_, self, made_, index_);
        }
        ({ List_Order* spite_temp_3 = List_List_Order_get_at(self->buckets_, (order_)->customer_); List_Order_append(spite_temp_3, Order___retain(order_)); List_Order___release(spite_temp_3); });
        index_ = (index_ + 1);
        Order___release(order_);
    }
}

int64_t Naive_ask(Naive* self, int32_t round_) {
    int64_t asked_ = SpiteInteger_to_long(0);
    int32_t customer_ = round_;
    int32_t query_ = 0;
    while (((query_ < 400000))) {
        customer_ = (({ int32_t spite_temp_4 = ({ int32_t spite_temp_5 = customer_; int32_t spite_temp_6 = 7919; int32_t spite_temp_7; if (__builtin_expect(__builtin_mul_overflow(spite_temp_5, spite_temp_6, &spite_temp_7), 0)) spite_overflowed("customer * 7919", "an Integer", "*", (int64_t)spite_temp_5, (int64_t)spite_temp_6, spite_site_2()); spite_temp_7; }); int32_t spite_temp_8 = 13; int32_t spite_temp_9; if (__builtin_expect(__builtin_add_overflow(spite_temp_4, spite_temp_8, &spite_temp_9), 0)) spite_overflowed("customer * 7919 + 13", "an Integer", "+", (int64_t)spite_temp_4, (int64_t)spite_temp_8, spite_site_2()); spite_temp_9; }) % 65536);
        List_Order* bucket_ = List_List_Order_get_at(self->buckets_, customer_);
        if (!(((bucket_) != 0))) {
            spite_failed_2(round_, asked_, customer_, query_, self);
        }
        int32_t at_ = 0;
        while (((at_ < List_Order_count(bucket_)))) {
            asked_ = ({ int64_t spite_temp_10 = asked_; int64_t spite_temp_11 = SpiteInteger_to_long((({ List_Order* spite_temp_12 = bucket_; int32_t spite_temp_13 = at_; if (__builtin_expect(spite_temp_13 < 0 || spite_temp_13 >= (spite_temp_12)->item_count_, 0)) spite_outside_list("bucket[at]", spite_site_3()); ((Order**)(intptr_t)(spite_temp_12)->items_)[spite_temp_13]; }))->amount_); int64_t spite_temp_14; if (__builtin_expect(__builtin_add_overflow(spite_temp_10, spite_temp_11, &spite_temp_14), 0)) spite_overflowed("asked + bucket[at].amount", "a Long", "+", (int64_t)spite_temp_10, (int64_t)spite_temp_11, spite_site_3()); spite_temp_14; });
            at_ = (at_ + 1);
        }
        query_ = (query_ + 1);
        List_Order___release(bucket_);
    }
    int64_t spite_temp_15 = asked_;
    return spite_temp_15;
}
