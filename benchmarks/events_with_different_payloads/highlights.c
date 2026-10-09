/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void Naive_handle(Naive* self, Naive_Payload event_) {
    {
        Naive_Payload spite_temp_1 = event_;
        if (((SpiteHeader*)(spite_temp_1))->class_id == 111) {
            self->clicks_ = ({ int64_t spite_temp_2 = ({ int64_t spite_temp_3 = self->clicks_; int64_t spite_temp_4 = SpiteInteger_to_long(({ int32_t spite_temp_5 = (((Click*)spite_temp_1))->x_; int32_t spite_temp_6 = (((Click*)spite_temp_1))->button_; int32_t spite_temp_7; if (__builtin_expect(__builtin_mul_overflow(spite_temp_5, spite_temp_6, &spite_temp_7), 0)) spite_overflowed("event.x * event.button", "an Integer", "*", (int64_t)spite_temp_5, (int64_t)spite_temp_6, spite_site_1()); spite_temp_7; })); int64_t spite_temp_8; if (__builtin_expect(__builtin_add_overflow(spite_temp_3, spite_temp_4, &spite_temp_8), 0)) spite_overflowed("clicks + event.x * event.button", "a Long", "+", (int64_t)spite_temp_3, (int64_t)spite_temp_4, spite_site_1()); spite_temp_8; }); int64_t spite_temp_9 = SpiteInteger_to_long((((Click*)spite_temp_1))->y_); int64_t spite_temp_10; if (__builtin_expect(__builtin_add_overflow(spite_temp_2, spite_temp_9, &spite_temp_10), 0)) spite_overflowed("clicks + event.x * event.button + event.y", "a Long", "+", (int64_t)spite_temp_2, (int64_t)spite_temp_9, spite_site_1()); spite_temp_10; });
        }
        else if (((SpiteHeader*)(spite_temp_1))->class_id == 112) {
            Naive_type_key(self, Key___retain(((Key*)spite_temp_1)));
        }
        else if (((SpiteHeader*)(spite_temp_1))->class_id == 113) {
            Naive_resize(self, Resize___retain(((Resize*)spite_temp_1)));
        }
    }
    Naive_Payload___release(event_);
}

void List_Naive_Payload_each_handle_for_naive(List_Naive_Payload* self, Naive* owner_) {
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        Naive_Payload item_ = ((Naive_Payload*)(intptr_t)self->items_)[index_];
        Naive_handle(owner_, Naive_Payload___retain(item_));
        index_ = (index_ + 1);
    }
    Naive___release(owner_);
}

Click* Click___allocate(void) {
    Click* self = (Click*)SPITE_MALLOC(sizeof(Click));
    self->header.ref_count = 1;
    self->header.class_id = 111;
    Click___init(self);
    #ifdef SPITE_TRACKS_Click
    spite_track_Click(self);
    #endif
    return self;
}
