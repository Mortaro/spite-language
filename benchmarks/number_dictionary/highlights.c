/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void Naive_Naive(Naive* self) {
    int64_t start_ = Clock_elapsed_nanoseconds(self->clock_);
    Dictionary_Integer_by_Integer* scores_ = Dictionary_Integer_by_Integer___make();
    int32_t index_ = 0;
    while (((index_ < 500000))) {
        Dictionary_Integer_by_Integer_set_at(scores_, (index_ * 7), index_);
        index_ = (index_ + 1);
    }
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t found_ = 0;
    int32_t round_ = 0;
    while (((round_ < 10))) {
        index_ = 0;
        while (((index_ < 500000))) {
            int32_t key_ = (index_ * 3);
            Nullable_Integer spite_temp_1;
            if (((spite_temp_1 = Dictionary_Integer_by_Integer_get_at(scores_, key_)).has_value)) {
                total_ = ({ int64_t spite_temp_2 = total_; int64_t spite_temp_3 = SpiteInteger_to_long(spite_temp_1.value); int64_t spite_temp_4; if (__builtin_expect(__builtin_add_overflow(spite_temp_2, spite_temp_3, &spite_temp_4), 0)) spite_overflowed("total + scores[key]", "a Long", "+", (int64_t)spite_temp_2, (int64_t)spite_temp_3, spite_site_1()); spite_temp_4; });
                found_ = ({ int32_t spite_temp_5 = found_; int32_t spite_temp_6 = 1; int32_t spite_temp_7; if (__builtin_expect(__builtin_add_overflow(spite_temp_5, spite_temp_6, &spite_temp_7), 0)) spite_overflowed("found + 1", "an Integer", "+", (int64_t)spite_temp_5, (int64_t)spite_temp_6, spite_site_2()); spite_temp_7; });
            }
            index_ = (index_ + 1);
        }
        round_ = (round_ + 1);
    }
    int32_t entries_ = ((int32_t)(Dictionary_Integer_by_Integer_count(scores_)));
    int64_t microseconds_ = (({ int64_t spite_temp_8 = Clock_elapsed_nanoseconds(self->clock_); int64_t spite_temp_9 = start_; int64_t spite_temp_10; if (__builtin_expect(__builtin_sub_overflow(spite_temp_8, spite_temp_9, &spite_temp_10), 0)) spite_overflowed("clock.elapsed_nanoseconds() - start", "a Long", "-", (int64_t)spite_temp_8, (int64_t)spite_temp_9, spite_site_3()); spite_temp_10; }) / SpiteInteger_to_long(1000));
    List_Console_Printable spite_framed_1; Console_Printable spite_framed_1_items[6]; int32_t spite_framed_1_count = 0;
    Console_print(self->console_, ({ spite_framed_1_items[0] = spite_tagged_object(0, ((void*)&spite_lit_1_box)); spite_framed_1_items[1] = spite_tagged_SpiteInteger(entries_); spite_framed_1_items[2] = spite_tagged_object(0, ((void*)&spite_lit_2_box)); spite_framed_1_items[3] = spite_tagged_SpiteInteger(found_); spite_framed_1_items[4] = spite_tagged_object(0, ((void*)&spite_lit_3_box)); spite_framed_1_items[5] = spite_tagged_SpiteLong(total_); spite_framed_1_count = 6; List_Console_Printable___framed(&spite_framed_1, (int64_t)(intptr_t)spite_framed_1_items, 6); }));
    for (int32_t spite_index = 0; spite_index < spite_framed_1_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_1_items[spite_index]); }
    List_Console_Printable spite_framed_2; Console_Printable spite_framed_2_items[1]; int32_t spite_framed_2_count = 0;
    Console_error(self->console_, ({ spite_framed_2_items[0] = spite_tagged_object(0, spite_box_SpiteString(({ char spite_temp_11_digits[24]; SpiteString spite_temp_11 = SPITE_STATIC_STRING(spite_temp_11_digits, spite_long_digits(spite_temp_11_digits, (int64_t)(microseconds_))); SpiteString spite_temp_12[] = {spite_lit_4, spite_temp_11}; SpiteString spite_temp_13 = spite_string_join(2, spite_temp_12); spite_temp_13; }))); spite_framed_2_count = 1; List_Console_Printable___framed(&spite_framed_2, (int64_t)(intptr_t)spite_framed_2_items, 1); }));
    for (int32_t spite_index = 0; spite_index < spite_framed_2_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_2_items[spite_index]); }
    Dictionary_Integer_by_Integer___release(scores_);
}

void Dictionary_Integer_by_Integer_set_at(Dictionary_Integer_by_Integer* self, int32_t key_, int32_t value_) {
    int32_t existing_ = Dictionary_Integer_by_Integer_index_of_key(self, key_);
    if (((existing_ >= 0))) {
        List_Integer_set_at(self->entry_values_, existing_, value_);
    }
    else {
        Dictionary_Integer_by_Integer_make_room(self);
        int32_t position_ = List_Integer_count(self->entry_keys_);
        Dictionary_Integer_by_Integer_place(self, key_, position_);
        List_Integer_append(self->entry_keys_, key_);
        List_Integer_append(self->entry_values_, value_);
    }
}

Nullable_Integer Dictionary_Integer_by_Integer_get_at(Dictionary_Integer_by_Integer* self, int32_t key_) {
    int32_t existing_ = Dictionary_Integer_by_Integer_index_of_key(self, key_);
    if (((existing_ >= 0))) {
        Nullable_Integer spite_temp_14 = List_Integer_get_at(self->entry_values_, existing_);
        return spite_temp_14;
    }
    Nullable_Integer spite_temp_15 = ((Nullable_Integer){ .has_value = false, .value = 0 });
    return spite_temp_15;
}

int32_t Dictionary_Integer_by_Integer_find_slot(Dictionary_Integer_by_Integer* self, int32_t key_) {
    if (((self->slot_count_ == 0))) {
        int32_t spite_temp_16 = (-(1));
        return spite_temp_16;
    }
    uint64_t hash_ = Dictionary_Integer_by_Integer_hash_of(self, key_);
    int32_t fragment_ = Dictionary_Integer_by_Integer_fragment_of(self, hash_);
    int32_t slot_ = Dictionary_Integer_by_Integer_home_slot(self, hash_);
    int32_t stored_ = SpiteMemory_Address_read_integer(self->slots_, SpiteInteger_to_long(({ int32_t spite_temp_17 = slot_; int32_t spite_temp_18 = 8; int32_t spite_temp_19; if (__builtin_expect(__builtin_mul_overflow(spite_temp_17, spite_temp_18, &spite_temp_19), 0)) spite_overflowed("slot * 8", "an Integer", "*", (int64_t)spite_temp_17, (int64_t)spite_temp_18, spite_site_4()); spite_temp_19; })));
    while (((stored_ != 0))) {
        {
            if ((((stored_ > 0))) && (((SpiteMemory_Address_read_integer(self->slots_, SpiteInteger_to_long(({ int32_t spite_temp_20 = ({ int32_t spite_temp_21 = slot_; int32_t spite_temp_22 = 8; int32_t spite_temp_23; if (__builtin_expect(__builtin_mul_overflow(spite_temp_21, spite_temp_22, &spite_temp_23), 0)) spite_overflowed("slot * 8", "an Integer", "*", (int64_t)spite_temp_21, (int64_t)spite_temp_22, spite_site_5()); spite_temp_23; }); int32_t spite_temp_24 = 4; int32_t spite_temp_25; if (__builtin_expect(__builtin_add_overflow(spite_temp_20, spite_temp_24, &spite_temp_25), 0)) spite_overflowed("slot * 8 + 4", "an Integer", "+", (int64_t)spite_temp_20, (int64_t)spite_temp_24, spite_site_5()); spite_temp_25; }))) == key_)))) {
                int32_t spite_temp_26 = slot_;
                return spite_temp_26;
            }
        }
        slot_ = Dictionary_Integer_by_Integer_next_slot(self, slot_);
        stored_ = SpiteMemory_Address_read_integer(self->slots_, SpiteInteger_to_long(({ int32_t spite_temp_27 = slot_; int32_t spite_temp_28 = 8; int32_t spite_temp_29; if (__builtin_expect(__builtin_mul_overflow(spite_temp_27, spite_temp_28, &spite_temp_29), 0)) spite_overflowed("slot * 8", "an Integer", "*", (int64_t)spite_temp_27, (int64_t)spite_temp_28, spite_site_6()); spite_temp_29; })));
    }
    int32_t spite_temp_30 = (-(1));
    return spite_temp_30;
}

uint64_t Dictionary_Integer_by_Integer_hash_of(Dictionary_Integer_by_Integer* self, int32_t key_) {
    {
        {
            uint32_t bits_ = SpiteInteger_bits_as_unsigned(key_);
            uint64_t number_ = SpiteUnsignedInteger_to_unsigned_long(bits_);
            uint64_t spite_temp_31 = SpiteUnsignedLong_wrapping_multiply(number_, SpiteLong_to_unsigned_long(6364136223846793005));
            return spite_temp_31;
        }
    }
}

int32_t Dictionary_Integer_by_Integer_home_slot(Dictionary_Integer_by_Integer* self, uint64_t hash_) {
    uint64_t high_ = SpiteUnsignedLong_shifted_right(hash_, 29);
    uint64_t mixed_ = SpiteUnsignedLong_bits_exclusive_or(hash_, high_);
    int32_t spite_temp_32 = ({ uint64_t spite_temp_33 = SpiteUnsignedLong_bits_and(mixed_, ({ int32_t spite_temp_34 = ({ int32_t spite_temp_35 = self->slot_count_; int32_t spite_temp_36 = 1; int32_t spite_temp_37; if (__builtin_expect(__builtin_sub_overflow(spite_temp_35, spite_temp_36, &spite_temp_37), 0)) spite_overflowed("slot_count - 1", "an Integer", "-", (int64_t)spite_temp_35, (int64_t)spite_temp_36, spite_site_7()); spite_temp_37; }); if (__builtin_expect(spite_temp_34 < 0 || spite_temp_34 > UINT64_MAX, 0)) spite_narrowed((int64_t)spite_temp_34, "an Integer", "an UnsignedLong", spite_site_7()); (uint64_t)spite_temp_34; })); if (__builtin_expect(spite_temp_33 > INT32_MAX, 0)) spite_narrowed_unsigned((uint64_t)spite_temp_33, "an UnsignedLong", "an Integer", spite_site_7()); (int32_t)spite_temp_33; });
    return spite_temp_32;
}

int32_t Dictionary_Integer_by_Integer_next_slot(Dictionary_Integer_by_Integer* self, int32_t slot_) {
    if (((({ int32_t spite_temp_38 = slot_; int32_t spite_temp_39 = 1; int32_t spite_temp_40; if (__builtin_expect(__builtin_add_overflow(spite_temp_38, spite_temp_39, &spite_temp_40), 0)) spite_overflowed("slot + 1", "an Integer", "+", (int64_t)spite_temp_38, (int64_t)spite_temp_39, spite_site_8()); spite_temp_40; }) == self->slot_count_))) {
        int32_t spite_temp_41 = 0;
        return spite_temp_41;
    }
    int32_t spite_temp_42 = ({ int32_t spite_temp_43 = slot_; int32_t spite_temp_44 = 1; int32_t spite_temp_45; if (__builtin_expect(__builtin_add_overflow(spite_temp_43, spite_temp_44, &spite_temp_45), 0)) spite_overflowed("slot + 1", "an Integer", "+", (int64_t)spite_temp_43, (int64_t)spite_temp_44, spite_site_9()); spite_temp_45; });
    return spite_temp_42;
}
