/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

struct Order {
    SpiteHeader header;
    int32_t id_;
    int64_t placed_at_;
    int32_t customer_;
    int32_t product_;
    int32_t quantity_;
    int32_t price_;
    int32_t region_;
    int32_t status_;
};

List_Order* List_Order_sort_by_placed_at(List_Order* self) {
    List_Long* keys_ = List_Long___make();
    List_Integer* order_ = List_Integer___make();
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        Order* item_ = ((Order**)(intptr_t)self->items_)[index_];
        int64_t key_ = (item_)->placed_at_;
        List_Long_append(keys_, key_);
        List_Integer_append(order_, index_);
        index_ = (index_ + 1);
    }
    List_Integer* merged_ = List_Integer_copy(order_);
    int32_t width_ = 1;
    while (((width_ < self->item_count_))) {
        int32_t start_ = 0;
        while (((start_ < self->item_count_))) {
            int32_t middle_ = ({ int32_t spite_temp_1 = start_; int32_t spite_temp_2 = width_; int32_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("start + width", "an Integer", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
            if (((middle_ > self->item_count_))) {
                middle_ = self->item_count_;
            }
            int32_t end_ = ({ int32_t spite_temp_4 = middle_; int32_t spite_temp_5 = width_; int32_t spite_temp_6; if (__builtin_expect(__builtin_add_overflow(spite_temp_4, spite_temp_5, &spite_temp_6), 0)) spite_overflowed("middle + width", "an Integer", "+", (int64_t)spite_temp_4, (int64_t)spite_temp_5, spite_site_2()); spite_temp_6; });
            if (((end_ > self->item_count_))) {
                end_ = self->item_count_;
            }
            int32_t left_ = start_;
            int32_t right_ = middle_;
            int32_t placed_ = start_;
            while (((placed_ < end_))) {
                bool takes_left_ = (right_ >= end_);
                if ((((left_ < middle_))) && (((!(takes_left_))))) {
                    if (!(((List_Integer_get_at(order_, left_)).has_value))) {
                        spite_failed_1(left_, order_, index_, width_, start_, middle_, end_, right_, placed_, takes_left_, self);
                    }
                    if (!(((List_Integer_get_at(order_, right_)).has_value))) {
                        spite_failed_2(right_, order_, index_, width_, start_, middle_, end_, left_, placed_, takes_left_, self);
                    }
                    int32_t left_position_ = (List_Integer_get_at(order_, left_)).value;
                    int32_t right_position_ = (List_Integer_get_at(order_, right_)).value;
                    if (!(((List_Long_get_at(keys_, left_position_)).has_value))) {
                        spite_failed_3(left_position_, keys_, index_, width_, start_, middle_, end_, left_, right_, placed_, takes_left_, right_position_, self);
                    }
                    if (!(((List_Long_get_at(keys_, right_position_)).has_value))) {
                        spite_failed_4(right_position_, keys_, index_, width_, start_, middle_, end_, left_, right_, placed_, takes_left_, left_position_, self);
                    }
                    int64_t left_key_ = (List_Long_get_at(keys_, left_position_)).value;
                    int64_t right_key_ = (List_Long_get_at(keys_, right_position_)).value;
                    bool right_first_ = (left_key_ > right_key_);
                    takes_left_ = (!(right_first_));
                }
                int32_t taken_ = right_;
                if ((takes_left_)) {
                    taken_ = left_;
                    left_ = ({ int32_t spite_temp_7 = left_; int32_t spite_temp_8 = 1; int32_t spite_temp_9; if (__builtin_expect(__builtin_add_overflow(spite_temp_7, spite_temp_8, &spite_temp_9), 0)) spite_overflowed("left + 1", "an Integer", "+", (int64_t)spite_temp_7, (int64_t)spite_temp_8, spite_site_3()); spite_temp_9; });
                }
                else {
                    right_ = ({ int32_t spite_temp_10 = right_; int32_t spite_temp_11 = 1; int32_t spite_temp_12; if (__builtin_expect(__builtin_add_overflow(spite_temp_10, spite_temp_11, &spite_temp_12), 0)) spite_overflowed("right + 1", "an Integer", "+", (int64_t)spite_temp_10, (int64_t)spite_temp_11, spite_site_4()); spite_temp_12; });
                }
                if (!(((List_Integer_get_at(order_, taken_)).has_value))) {
                    spite_failed_5(taken_, order_, index_, width_, start_, middle_, end_, left_, right_, placed_, takes_left_, self);
                }
                int32_t position_ = (List_Integer_get_at(order_, taken_)).value;
                List_Integer_set_at(merged_, placed_, position_);
                placed_ = (placed_ + 1);
            }
            start_ = end_;
        }
        List_Integer* previous_ = List_Integer___retain(order_);
        List_Integer* spite_temp_13 = List_Integer___retain(merged_);
        List_Integer___release(order_);
        order_ = spite_temp_13;
        List_Integer* spite_temp_14 = List_Integer___retain(previous_);
        List_Integer___release(merged_);
        merged_ = spite_temp_14;
        width_ = ({ int32_t spite_temp_15 = width_; int32_t spite_temp_16 = 2; int32_t spite_temp_17; if (__builtin_expect(__builtin_mul_overflow(spite_temp_15, spite_temp_16, &spite_temp_17), 0)) spite_overflowed("width * 2", "an Integer", "*", (int64_t)spite_temp_15, (int64_t)spite_temp_16, spite_site_5()); spite_temp_17; });
        List_Integer___release(previous_);
    }
    List_Order* sorted_ = List_Order___make();
    index_ = 0;
    while (((index_ < self->item_count_))) {
        if (!(((List_Integer_get_at(order_, index_)).has_value))) {
            spite_failed_6(index_, order_, width_, self);
        }
        int32_t source_ = (List_Integer_get_at(order_, index_)).value;
        Order* item_ = ((Order**)(intptr_t)self->items_)[source_];
        List_Order_append(sorted_, Order___retain(item_));
        index_ = (index_ + 1);
    }
    List_Order* spite_temp_18 = List_Order___retain(sorted_);
    List_Order___release(sorted_);
    List_Integer___release(merged_);
    List_Integer___release(order_);
    List_Long___release(keys_);
    return spite_temp_18;
}

int64_t Naive_running_total___held_0(Naive* self, List_Order* orders_) {
    int64_t running_ = SpiteInteger_to_long(0);
    int32_t index_ = 0;
    while (((index_ < spite_folded_List_Order_count(orders_)))) {
        Order* order_ = ({ List_Order* spite_temp_19 = orders_; int32_t spite_temp_20 = index_; if (__builtin_expect(spite_temp_20 < 0 || spite_temp_20 >= (spite_temp_19)->item_count_, 0)) spite_outside_list("orders[index]", spite_site_6()); ((Order**)(intptr_t)(spite_temp_19)->items_)[spite_temp_20]; });
        running_ = (({ int64_t spite_temp_21 = ({ int64_t spite_temp_22 = running_; int64_t spite_temp_23 = SpiteInteger_to_long(31); int64_t spite_temp_24; if (__builtin_expect(__builtin_mul_overflow(spite_temp_22, spite_temp_23, &spite_temp_24), 0)) spite_overflowed("running * 31", "a Long", "*", (int64_t)spite_temp_22, (int64_t)spite_temp_23, spite_site_7()); spite_temp_24; }); int64_t spite_temp_25 = SpiteInteger_to_long(({ int32_t spite_temp_26 = (order_)->quantity_; int32_t spite_temp_27 = (order_)->price_; int32_t spite_temp_28; if (__builtin_expect(__builtin_mul_overflow(spite_temp_26, spite_temp_27, &spite_temp_28), 0)) spite_overflowed("order.quantity * order.price", "an Integer", "*", (int64_t)spite_temp_26, (int64_t)spite_temp_27, spite_site_7()); spite_temp_28; })); int64_t spite_temp_29; if (__builtin_expect(__builtin_add_overflow(spite_temp_21, spite_temp_25, &spite_temp_29), 0)) spite_overflowed("running * 31 + order.quantity * order.price", "a Long", "+", (int64_t)spite_temp_21, (int64_t)spite_temp_25, spite_site_7()); spite_temp_29; }) % SpiteInteger_to_long(1000000007));
        index_ = (index_ + 1);
    }
    int64_t spite_temp_30 = running_;
    return spite_temp_30;
}
