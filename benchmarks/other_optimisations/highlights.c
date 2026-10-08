/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void Naive_Naive(Naive* self) {
    Vector__Velocity* velocities_ = Vector__Velocity___make();
    int32_t index_ = 0;
    while (((index_ < 1000))) {
        Velocity spite_slot_1;
        Velocity* made_ = Velocity___make_into(&spite_slot_1, (index_ % 7), (index_ % 5));
        Vector__Velocity_append(velocities_, Velocity___retain(made_));
        index_ = (index_ + 1);
    }
    int32_t across_ = Vector__Velocity_sum_across(velocities_);
    int32_t down_ = Vector__Velocity_sum_down(velocities_);
    List_Console_Printable spite_framed_1; Console_Printable spite_framed_1_items[4]; int32_t spite_framed_1_count = 0;
    Console_print(self->console_, ({ spite_framed_1_items[0] = spite_tagged_object(0, ((void*)&spite_lit_1_box)); spite_framed_1_items[1] = spite_tagged_SpiteInteger(across_); spite_framed_1_items[2] = spite_tagged_object(0, ((void*)&spite_lit_2_box)); spite_framed_1_items[3] = spite_tagged_SpiteInteger(down_); spite_framed_1_count = 4; List_Console_Printable___framed(&spite_framed_1, (int64_t)(intptr_t)spite_framed_1_items, 4); }));
    for (int32_t spite_index = 0; spite_index < spite_framed_1_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_1_items[spite_index]); }
    Vector__Velocity___release(velocities_);
}

void Vector__Velocity_append(Vector__Velocity* self, Velocity* value_) {
    Vector__Velocity_make_room(self);
    InlineMemory__Velocity_write_item(self->values_, self->items_, self->item_count_, Velocity___retain(value_));
    self->item_count_ = ({ int32_t spite_temp_1 = self->item_count_; int32_t spite_temp_2 = 1; int32_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("item_count + 1", "an Integer", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
    Velocity___release(value_);
}
