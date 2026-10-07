/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

static __typeof__(&Velocity___init) spite_folded_Velocity___init = ((__typeof__(&Velocity___init))&Position___init);
static __typeof__(&TypedMemory__Console_Printable___release) spite_folded_TypedMemory__Console_Printable___release = ((__typeof__(&TypedMemory__Console_Printable___release))&Memory_Heap___release);
static __typeof__(&TypedMemory__Position___release) spite_folded_TypedMemory__Position___release = ((__typeof__(&TypedMemory__Position___release))&Memory_Heap___release);
static __typeof__(&TypedMemory__Velocity___release) spite_folded_TypedMemory__Velocity___release = ((__typeof__(&TypedMemory__Velocity___release))&Memory_Heap___release);
static __typeof__(&Velocity_Velocity) spite_folded_Velocity_Velocity = ((__typeof__(&Velocity_Velocity))&Position_Position);
static __typeof__(&List_Position_count) spite_folded_List_Position_count = ((__typeof__(&List_Position_count))&List_Console_Printable_count);
static __typeof__(&List_Velocity_count) spite_folded_List_Velocity_count = ((__typeof__(&List_Velocity_count))&List_Console_Printable_count);
static __typeof__(&List_Velocity_sum_across) spite_folded_List_Velocity_sum_across = ((__typeof__(&List_Velocity_sum_across))&List_Position_sum_across);
static __typeof__(&List_Velocity_sum_down) spite_folded_List_Velocity_sum_down = ((__typeof__(&List_Velocity_sum_down))&List_Position_sum_down);
static __typeof__(&TypedMemory__Velocity_write_value) spite_folded_TypedMemory__Velocity_write_value = ((__typeof__(&TypedMemory__Velocity_write_value))&TypedMemory__Position_write_value);
static __typeof__(&TypedMemory__Velocity_value_bytes) spite_folded_TypedMemory__Velocity_value_bytes = ((__typeof__(&TypedMemory__Velocity_value_bytes))&TypedMemory__Position_value_bytes);

void Naive_Naive(Naive* self) {
    List_Position* positions_ = List_Position___make();
    List_Velocity* velocities_ = List_Velocity___make();
    int32_t index_ = 0;
    while (((index_ < 1000))) {
        Position* position_ = Position___make(index_, ({ int32_t spite_temp_1 = index_; int32_t spite_temp_2 = 2; int32_t spite_temp_3; if (__builtin_expect(__builtin_mul_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("index * 2", "an Integer", "*", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; }));
        List_Position_append(positions_, Position___retain(position_));
        Velocity* velocity_ = Velocity___make((index_ % 7), (index_ % 5));
        List_Velocity_append(velocities_, Velocity___retain(velocity_));
        index_ = (index_ + 1);
        Velocity___release(velocity_);
        Position___release(position_);
    }
    int32_t across_ = ({ int32_t spite_temp_4 = List_Position_sum_across(positions_); int32_t spite_temp_5 = spite_folded_List_Velocity_sum_across(velocities_); int32_t spite_temp_6; if (__builtin_expect(__builtin_add_overflow(spite_temp_4, spite_temp_5, &spite_temp_6), 0)) spite_overflowed("positions.sum_across() + velocities.sum_across()", "an Integer", "+", (int64_t)spite_temp_4, (int64_t)spite_temp_5, spite_site_2()); spite_temp_6; });
    int32_t down_ = ({ int32_t spite_temp_7 = List_Position_sum_down(positions_); int32_t spite_temp_8 = spite_folded_List_Velocity_sum_down(velocities_); int32_t spite_temp_9; if (__builtin_expect(__builtin_add_overflow(spite_temp_7, spite_temp_8, &spite_temp_9), 0)) spite_overflowed("positions.sum_down() + velocities.sum_down()", "an Integer", "+", (int64_t)spite_temp_7, (int64_t)spite_temp_8, spite_site_3()); spite_temp_9; });
    int32_t items_ = ({ int32_t spite_temp_10 = spite_folded_List_Position_count(positions_); int32_t spite_temp_11 = spite_folded_List_Velocity_count(velocities_); int32_t spite_temp_12; if (__builtin_expect(__builtin_add_overflow(spite_temp_10, spite_temp_11, &spite_temp_12), 0)) spite_overflowed("positions.count() + velocities.count()", "an Integer", "+", (int64_t)spite_temp_10, (int64_t)spite_temp_11, spite_site_4()); spite_temp_12; });
    List_Console_Printable spite_framed_1; Console_Printable spite_framed_1_items[6]; int32_t spite_framed_1_count = 0;
    Console_print(self->console_, ({ spite_framed_1_items[0] = spite_tagged_object(0, ((void*)&spite_lit_1_box)); spite_framed_1_items[1] = spite_tagged_SpiteInteger(across_); spite_framed_1_items[2] = spite_tagged_object(0, ((void*)&spite_lit_2_box)); spite_framed_1_items[3] = spite_tagged_SpiteInteger(down_); spite_framed_1_items[4] = spite_tagged_object(0, ((void*)&spite_lit_3_box)); spite_framed_1_items[5] = spite_tagged_SpiteInteger(items_); spite_framed_1_count = 6; List_Console_Printable___framed(&spite_framed_1, (int64_t)(intptr_t)spite_framed_1_items, 6); }));
    for (int32_t spite_index = 0; spite_index < spite_framed_1_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_1_items[spite_index]); }
    List_Velocity___release(velocities_);
    List_Position___release(positions_);
}

int32_t List_Position_sum_across(List_Position* self) {
    int32_t total_ = 0;
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        Position* item_ = ((Position**)(intptr_t)self->items_)[index_];
        total_ = ({ int32_t spite_temp_13 = total_; int32_t spite_temp_14 = (item_)->across_; int32_t spite_temp_15; if (__builtin_expect(__builtin_add_overflow(spite_temp_13, spite_temp_14, &spite_temp_15), 0)) spite_overflowed("total + item.attributes[member]", "an Integer", "+", (int64_t)spite_temp_13, (int64_t)spite_temp_14, spite_site_5()); spite_temp_15; });
        index_ = (index_ + 1);
    }
    int32_t spite_temp_16 = total_;
    return spite_temp_16;
}
