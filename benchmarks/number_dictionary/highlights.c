/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void Naive_fill_and_look_up(Naive* self) {
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
    self->entries_ = ((int32_t)(Dictionary_Integer_by_Integer_count(scores_)));
    self->found_keys_ = found_;
    self->found_total_ = total_;
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
        Nullable_Integer spite_temp_8 = List_Integer_get_at(self->entry_values_, existing_);
        return spite_temp_8;
    }
    Nullable_Integer spite_temp_9 = ((Nullable_Integer){ .has_value = false, .value = 0 });
    return spite_temp_9;
}

int32_t Dictionary_Integer_by_Integer_find_slot(Dictionary_Integer_by_Integer* self, int32_t key_) {
    if (((self->slot_count_ == 0))) {
        int32_t spite_temp_10 = (-(1));
        return spite_temp_10;
    }
    uint64_t hash_ = Dictionary_Integer_by_Integer_hash_of(self, key_);
    int32_t fragment_ = Dictionary_Integer_by_Integer_fragment_of(self, hash_);
    int32_t slot_ = Dictionary_Integer_by_Integer_home_slot(self, hash_);
    int32_t stored_ = SpiteMemory_Address_read_integer(self->slots_, SpiteInteger_to_long(({ int32_t spite_temp_11 = slot_; int32_t spite_temp_12 = 8; int32_t spite_temp_13; if (__builtin_expect(__builtin_mul_overflow(spite_temp_11, spite_temp_12, &spite_temp_13), 0)) spite_overflowed("slot * 8", "an Integer", "*", (int64_t)spite_temp_11, (int64_t)spite_temp_12, spite_site_3()); spite_temp_13; })));
    while (((stored_ != 0))) {
        {
            if ((((stored_ > 0))) && (((SpiteMemory_Address_read_integer(self->slots_, SpiteInteger_to_long(({ int32_t spite_temp_14 = ({ int32_t spite_temp_15 = slot_; int32_t spite_temp_16 = 8; int32_t spite_temp_17; if (__builtin_expect(__builtin_mul_overflow(spite_temp_15, spite_temp_16, &spite_temp_17), 0)) spite_overflowed("slot * 8", "an Integer", "*", (int64_t)spite_temp_15, (int64_t)spite_temp_16, spite_site_4()); spite_temp_17; }); int32_t spite_temp_18 = 4; int32_t spite_temp_19; if (__builtin_expect(__builtin_add_overflow(spite_temp_14, spite_temp_18, &spite_temp_19), 0)) spite_overflowed("slot * 8 + 4", "an Integer", "+", (int64_t)spite_temp_14, (int64_t)spite_temp_18, spite_site_4()); spite_temp_19; }))) == key_)))) {
                int32_t spite_temp_20 = slot_;
                return spite_temp_20;
            }
        }
        slot_ = Dictionary_Integer_by_Integer_next_slot(self, slot_);
        stored_ = SpiteMemory_Address_read_integer(self->slots_, SpiteInteger_to_long(({ int32_t spite_temp_21 = slot_; int32_t spite_temp_22 = 8; int32_t spite_temp_23; if (__builtin_expect(__builtin_mul_overflow(spite_temp_21, spite_temp_22, &spite_temp_23), 0)) spite_overflowed("slot * 8", "an Integer", "*", (int64_t)spite_temp_21, (int64_t)spite_temp_22, spite_site_5()); spite_temp_23; })));
    }
    int32_t spite_temp_24 = (-(1));
    return spite_temp_24;
}

uint64_t Dictionary_Integer_by_Integer_hash_of(Dictionary_Integer_by_Integer* self, int32_t key_) {
    {
        {
            uint32_t bits_ = SpiteInteger_bits_as_unsigned(key_);
            uint64_t number_ = SpiteUnsignedInteger_to_unsigned_long(bits_);
            uint64_t spite_temp_25 = SpiteUnsignedLong_wrapping_multiply(number_, SpiteLong_to_unsigned_long(6364136223846793005));
            return spite_temp_25;
        }
    }
}

int32_t Dictionary_Integer_by_Integer_home_slot(Dictionary_Integer_by_Integer* self, uint64_t hash_) {
    uint64_t high_ = SpiteUnsignedLong_shifted_right(hash_, 29);
    uint64_t mixed_ = SpiteUnsignedLong_bits_exclusive_or(hash_, high_);
    int32_t spite_temp_26 = ({ uint64_t spite_temp_27 = SpiteUnsignedLong_bits_and(mixed_, ({ int32_t spite_temp_28 = ({ int32_t spite_temp_29 = self->slot_count_; int32_t spite_temp_30 = 1; int32_t spite_temp_31; if (__builtin_expect(__builtin_sub_overflow(spite_temp_29, spite_temp_30, &spite_temp_31), 0)) spite_overflowed("slot_count - 1", "an Integer", "-", (int64_t)spite_temp_29, (int64_t)spite_temp_30, spite_site_6()); spite_temp_31; }); if (__builtin_expect(spite_temp_28 < 0 || spite_temp_28 > UINT64_MAX, 0)) spite_narrowed((int64_t)spite_temp_28, "an Integer", "an UnsignedLong", spite_site_6()); (uint64_t)spite_temp_28; })); if (__builtin_expect(spite_temp_27 > INT32_MAX, 0)) spite_narrowed_unsigned((uint64_t)spite_temp_27, "an UnsignedLong", "an Integer", spite_site_6()); (int32_t)spite_temp_27; });
    return spite_temp_26;
}

int32_t Dictionary_Integer_by_Integer_next_slot(Dictionary_Integer_by_Integer* self, int32_t slot_) {
    if (((({ int32_t spite_temp_32 = slot_; int32_t spite_temp_33 = 1; int32_t spite_temp_34; if (__builtin_expect(__builtin_add_overflow(spite_temp_32, spite_temp_33, &spite_temp_34), 0)) spite_overflowed("slot + 1", "an Integer", "+", (int64_t)spite_temp_32, (int64_t)spite_temp_33, spite_site_7()); spite_temp_34; }) == self->slot_count_))) {
        int32_t spite_temp_35 = 0;
        return spite_temp_35;
    }
    int32_t spite_temp_36 = ({ int32_t spite_temp_37 = slot_; int32_t spite_temp_38 = 1; int32_t spite_temp_39; if (__builtin_expect(__builtin_add_overflow(spite_temp_37, spite_temp_38, &spite_temp_39), 0)) spite_overflowed("slot + 1", "an Integer", "+", (int64_t)spite_temp_37, (int64_t)spite_temp_38, spite_site_8()); spite_temp_39; });
    return spite_temp_36;
}
