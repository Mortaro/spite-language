/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void Naive_Naive(Naive* self) {
    int64_t start_ = Clock_elapsed_nanoseconds(self->clock_);
    int64_t checksum_ = SpiteInteger_to_long(0);
    int32_t round_ = 0;
    while (((round_ < 40))) {
        int32_t removed_in_ten_ = 5;
        if ((((round_ % 2) == 1))) {
            removed_in_ten_ = 9;
        }
        Despawns_mark_every(self->despawns_, self->item_total_, removed_in_ten_);
        Naive_fill(self);
        Items__Velocity_remove_where_despawned(self->velocities_);
        List_Integer_remove_where_marked_for_despawns(self->entities_, Despawns___retain(self->despawns_));
        checksum_ = ({ int64_t spite_temp_1 = checksum_; int64_t spite_temp_2 = Naive_kept_sum(self); int64_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("checksum + kept_sum()", "a Long", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
        round_ = (round_ + 1);
    }
    int64_t microseconds_ = (({ int64_t spite_temp_4 = Clock_elapsed_nanoseconds(self->clock_); int64_t spite_temp_5 = start_; int64_t spite_temp_6; if (__builtin_expect(__builtin_sub_overflow(spite_temp_4, spite_temp_5, &spite_temp_6), 0)) spite_overflowed("clock.elapsed_nanoseconds() - start", "a Long", "-", (int64_t)spite_temp_4, (int64_t)spite_temp_5, spite_site_2()); spite_temp_6; }) / SpiteInteger_to_long(1000));
    List_Console_Printable spite_framed_1; Console_Printable spite_framed_1_items[2]; int32_t spite_framed_1_count = 0;
    Console_print(self->console_, ({ spite_framed_1_items[0] = spite_tagged_object(0, ((void*)&spite_lit_1_box)); spite_framed_1_items[1] = spite_tagged_SpiteLong(checksum_); spite_framed_1_count = 2; List_Console_Printable___framed(&spite_framed_1, (int64_t)(intptr_t)spite_framed_1_items, 2); }));
    for (int32_t spite_index = 0; spite_index < spite_framed_1_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_1_items[spite_index]); }
    List_Console_Printable spite_framed_2; Console_Printable spite_framed_2_items[1]; int32_t spite_framed_2_count = 0;
    Console_error(self->console_, ({ spite_framed_2_items[0] = spite_tagged_object(0, spite_box_SpiteString(({ char spite_temp_7_digits[24]; SpiteString spite_temp_7 = SPITE_STATIC_STRING(spite_temp_7_digits, spite_long_digits(spite_temp_7_digits, (int64_t)(microseconds_))); SpiteString spite_temp_8[] = {spite_lit_2, spite_temp_7}; SpiteString spite_temp_9 = spite_string_join(2, spite_temp_8); spite_temp_9; }))); spite_framed_2_count = 1; List_Console_Printable___framed(&spite_framed_2, (int64_t)(intptr_t)spite_framed_2_items, 1); }));
    for (int32_t spite_index = 0; spite_index < spite_framed_2_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_2_items[spite_index]); }
}

void Naive_fill(Naive* self) {
    Items__Velocity_clear(self->velocities_);
    List_Integer_clear(self->entities_);
    int32_t entity_ = 0;
    while (((entity_ < self->item_total_))) {
        bool despawned_now_ = Despawns_marked(self->despawns_, entity_);
        Velocity* velocity_ = Velocity___make(entity_, (1.0 * SpiteInteger_to_float(entity_)), despawned_now_);
        Items__Velocity_append(self->velocities_, Velocity___retain(velocity_));
        List_Integer_append(self->entities_, entity_);
        entity_ = (entity_ + 1);
        Velocity___release(velocity_);
    }
}

void Items__Velocity_remove_where_despawned(Items__Velocity* self) {
    int32_t kept_ = 0;
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        bool keep_ = true;
        {
            Velocity* item_ = InlineMemory__Velocity_item_at(self->inline_, self->items_, index_);
            if (((item_)->despawned_)) {
                keep_ = false;
            }
        }
        if ((keep_)) {
            if ((((kept_ != index_))) && (((index_ < self->item_count_)))) {
                Items__Velocity__swap(self, kept_, index_);
            }
            kept_ = ({ int32_t spite_temp_10 = kept_; int32_t spite_temp_11 = 1; int32_t spite_temp_12; if (__builtin_expect(__builtin_add_overflow(spite_temp_10, spite_temp_11, &spite_temp_12), 0)) spite_overflowed("kept + 1", "an Integer", "+", (int64_t)spite_temp_10, (int64_t)spite_temp_11, spite_site_3()); spite_temp_12; });
        }
        index_ = (index_ + 1);
    }
    Items__Velocity_truncate(self, kept_);
}

void List_Integer_remove_where_marked_for_despawns(List_Integer* self, Despawns* owner_) {
    int32_t kept_ = 0;
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        int32_t item_ = TypedMemory__Integer_read_value(self->values_, self->items_, index_);
        bool keep_ = true;
        if ((Despawns_marked(owner_, item_))) {
            keep_ = false;
        }
        if ((keep_)) {
            if ((((kept_ != index_))) && (((index_ < self->item_count_)))) {
                TypedMemory__Integer_swap_values(self->values_, self->items_, kept_, index_);
            }
            kept_ = ({ int32_t spite_temp_13 = kept_; int32_t spite_temp_14 = 1; int32_t spite_temp_15; if (__builtin_expect(__builtin_add_overflow(spite_temp_13, spite_temp_14, &spite_temp_15), 0)) spite_overflowed("kept + 1", "an Integer", "+", (int64_t)spite_temp_13, (int64_t)spite_temp_14, spite_site_4()); spite_temp_15; });
        }
        index_ = (index_ + 1);
    }
    List_Integer_truncate(self, kept_);
    Despawns___release(owner_);
}

int64_t Naive_kept_sum(Naive* self) {
    int64_t sum_ = SpiteInteger_to_long(0);
    int32_t row_ = 0;
    while (((row_ < Items__Velocity_count(self->velocities_)))) {
        Velocity* velocity_ = ({ Velocity* spite_temp_16 = Items__Velocity_get_at(self->velocities_, row_); if (__builtin_expect(!(((spite_temp_16) != 0)), 0)) spite_outside_list("velocities[row]", spite_site_5()); spite_temp_16; });
        if (!(((List_Integer_get_at(self->entities_, row_)).has_value))) {
            spite_failed_1(row_, self, sum_);
        }
        int32_t entity_ = (List_Integer_get_at(self->entities_, row_)).value;
        sum_ = ({ int64_t spite_temp_17 = ({ int64_t spite_temp_18 = sum_; int64_t spite_temp_19 = SpiteInteger_to_long((velocity_)->entity_); int64_t spite_temp_20; if (__builtin_expect(__builtin_add_overflow(spite_temp_18, spite_temp_19, &spite_temp_20), 0)) spite_overflowed("sum + velocity.entity", "a Long", "+", (int64_t)spite_temp_18, (int64_t)spite_temp_19, spite_site_6()); spite_temp_20; }); int64_t spite_temp_21 = SpiteInteger_to_long(entity_); int64_t spite_temp_22; if (__builtin_expect(__builtin_add_overflow(spite_temp_17, spite_temp_21, &spite_temp_22), 0)) spite_overflowed("sum + velocity.entity + entity", "a Long", "+", (int64_t)spite_temp_17, (int64_t)spite_temp_21, spite_site_6()); spite_temp_22; });
        row_ = (row_ + 1);
    }
    int64_t spite_temp_23 = sum_;
    return spite_temp_23;
}
