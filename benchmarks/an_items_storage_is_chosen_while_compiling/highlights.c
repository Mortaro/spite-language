/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

struct Items__Velocity {
    SpiteHeader header;
    Memory_Heap* heap_;
    InlineMemory__Velocity* inline_;
    TypedMemory__Velocity* references_;
    int64_t items_;
    int32_t item_count_;
    int32_t capacity_;
};

void Items__Velocity_append(Items__Velocity* self, Velocity* value_) {
    Items__Velocity__make_room(self);
    {
        InlineMemory__Velocity_write_item(self->inline_, self->items_, self->item_count_, Velocity___retain(value_));
    }
    self->item_count_ = ({ int32_t spite_temp_1 = self->item_count_; int32_t spite_temp_2 = 1; int32_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("item_count + 1", "an Integer", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
    Velocity___release(value_);
}

void Items__Trail_append(Items__Trail* self, Trail* value_) {
    Items__Trail__make_room(self);
    {
        TypedMemory__Trail_write_value(self->references_, self->items_, self->item_count_, Trail___retain(value_));
    }
    self->item_count_ = ({ int32_t spite_temp_4 = self->item_count_; int32_t spite_temp_5 = 1; int32_t spite_temp_6; if (__builtin_expect(__builtin_add_overflow(spite_temp_4, spite_temp_5, &spite_temp_6), 0)) spite_overflowed("item_count + 1", "an Integer", "+", (int64_t)spite_temp_4, (int64_t)spite_temp_5, spite_site_1()); spite_temp_6; });
    Trail___release(value_);
}

void Items__Velocity_each_integrate(Items__Velocity* self) {
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        {
            Velocity* item_ = InlineMemory__Velocity_item_at(self->inline_, self->items_, index_);
            Velocity_integrate(item_);
        }
        index_ = (index_ + 1);
    }
}

void Items__Trail_each_integrate(Items__Trail* self) {
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        {
            Trail* item_ = ((Trail**)(intptr_t)self->items_)[index_];
            Trail_integrate(item_);
        }
        index_ = (index_ + 1);
    }
}

Velocity* InlineMemory__Velocity_item_at(InlineMemory__Velocity* self, int64_t address_, int32_t index_) {
    return ((Velocity*)((char*)(intptr_t)address_ + (int64_t)index_ * ((int64_t)sizeof(Velocity) - (int64_t)sizeof(SpiteHeader))));
}
