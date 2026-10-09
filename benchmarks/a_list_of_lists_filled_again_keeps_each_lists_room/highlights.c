/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void List_List_Order_clear(List_List_Order* self) {
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        { List_Order* spite_slot = &((List_Order*)(intptr_t)self->items_)[index_]; List_Order_clear(spite_slot); spite_slot->header.ref_count = -7; }
        index_ = (index_ + 1);
    }
    self->item_count_ = 0;
}

void List_List_Order_drop(List_List_Order* self) {
    List_List_Order_clear(self);
    if (self->items_ != 0) for (int32_t spite_index = 0; spite_index < self->capacity_; spite_index++) { List_Order* spite_slot = &((List_Order*)(intptr_t)self->items_)[spite_index]; if (spite_slot->header.ref_count == -7) { spite_slot->header.ref_count = 0; List_Order_drop(spite_slot); } }
    if (((self->items_ != ((int64_t)(0))))) {
        ({ Spite_Allocator spite_temp_1 = SPITE_ALLOCATOR_List_List_Order(self, spite_singleton_Memory_Heap); int64_t spite_temp_2 = self->items_; if (((SpiteHeader*)(spite_temp_1))->class_id == 94) { Memory_Arena_free(((Memory_Arena*)spite_temp_1), spite_temp_2); } else if (((SpiteHeader*)(spite_temp_1))->class_id == 95) { Memory_Heap_free(((Memory_Heap*)spite_temp_1), spite_temp_2); } });
    }
}

void List_List_Order__grow(List_List_Order* self) {
    int32_t grown_ = ({ int32_t spite_temp_3 = self->capacity_; int32_t spite_temp_4 = 2; int32_t spite_temp_5; if (__builtin_expect(__builtin_mul_overflow(spite_temp_3, spite_temp_4, &spite_temp_5), 0)) spite_overflowed("capacity * 2", "an Integer", "*", (int64_t)spite_temp_3, (int64_t)spite_temp_4, spite_site_1()); spite_temp_5; });
    if (((self->capacity_ == 0))) {
        grown_ = 4;
    }
    int64_t bytes_ = TypedMemory__List_Order_value_bytes(self->values_);
    self->items_ = List_List_Order__resized(self, ({ int64_t spite_temp_6 = bytes_; int64_t spite_temp_7 = SpiteInteger_to_long(self->capacity_); int64_t spite_temp_8; if (__builtin_expect(__builtin_mul_overflow(spite_temp_6, spite_temp_7, &spite_temp_8), 0)) spite_overflowed("bytes * capacity", "a Long", "*", (int64_t)spite_temp_6, (int64_t)spite_temp_7, spite_site_2()); spite_temp_8; }), ({ int64_t spite_temp_9 = bytes_; int64_t spite_temp_10 = SpiteInteger_to_long(grown_); int64_t spite_temp_11; if (__builtin_expect(__builtin_mul_overflow(spite_temp_9, spite_temp_10, &spite_temp_11), 0)) spite_overflowed("bytes * grown", "a Long", "*", (int64_t)spite_temp_9, (int64_t)spite_temp_10, spite_site_2()); spite_temp_11; }));
    memset((char*)(intptr_t)self->items_ + (int64_t)self->capacity_ * (int64_t)sizeof(List_Order), 0, (size_t)(grown_ - self->capacity_) * sizeof(List_Order));
    self->capacity_ = grown_;
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

void Naive_group(Naive* self, int32_t round_) {
    List_List_Order_clear(self->buckets_);
    int32_t made_ = 0;
    while (((made_ < 32768))) {
        List_Order* bucket_ = List_Order___make();
        List_List_Order_append(self->buckets_, List_Order___retain(bucket_));
        made_ = (made_ + 1);
        List_Order___release(bucket_);
    }
    int32_t index_ = 0;
    while (((index_ < List_Order_count(self->orders_)))) {
        Order* order_ = ({ Order* spite_temp_12 = List_Order_get_at(self->orders_, index_); if (__builtin_expect(!(((spite_temp_12) != 0)), 0)) spite_outside_list("orders[index]", spite_site_3()); spite_temp_12; });
        int32_t customer_ = (({ int32_t spite_temp_13 = (order_)->customer_; int32_t spite_temp_14 = round_; int32_t spite_temp_15; if (__builtin_expect(__builtin_add_overflow(spite_temp_13, spite_temp_14, &spite_temp_15), 0)) spite_overflowed("order.customer + round", "an Integer", "+", (int64_t)spite_temp_13, (int64_t)spite_temp_14, spite_site_4()); spite_temp_15; }) % 32768);
        if (!(({ List_Order* spite_temp_16 = List_List_Order_get_at(self->buckets_, customer_); int path_narrowed = ((spite_temp_16) != 0); List_Order___release(spite_temp_16); path_narrowed; }))) {
            spite_failed_1(customer_, self, round_, made_, index_);
        }
        ({ List_Order* spite_temp_17 = List_List_Order_get_at(self->buckets_, customer_); List_Order_append(spite_temp_17, Order___retain(order_)); List_Order___release(spite_temp_17); });
        index_ = (index_ + 1);
        Order___release(order_);
    }
}
