/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int64_t Naive_look_up___held_0_1(Naive* self, List_String* names_, Dictionary_Integer* by_name_) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t round_ = 0;
    while (((round_ < 20))) {
        int32_t index_ = 0;
        while (((index_ < 100000))) {
            int32_t position_ = (({ int32_t spite_temp_1 = ({ int32_t spite_temp_2 = index_; int32_t spite_temp_3 = 7; int32_t spite_temp_4; if (__builtin_expect(__builtin_mul_overflow(spite_temp_2, spite_temp_3, &spite_temp_4), 0)) spite_overflowed("index * 7", "an Integer", "*", (int64_t)spite_temp_2, (int64_t)spite_temp_3, spite_site_1()); spite_temp_4; }); int32_t spite_temp_5 = round_; int32_t spite_temp_6; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_5, &spite_temp_6), 0)) spite_overflowed("index * 7 + round", "an Integer", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_5, spite_site_1()); spite_temp_6; }) % 5000);
            if (!(({ List_String* spite_temp_7 = names_; int32_t spite_temp_8 = position_; (spite_temp_8 >= 0 && spite_temp_8 < (spite_temp_7)->item_count_) && (!SPITE_STRING_IS_NULL(((SpiteString*)(intptr_t)(spite_temp_7)->items_)[spite_temp_8])); }))) {
                spite_failed_1(position_, names_, total_, round_, index_);
            }
            SpiteString name_ = List_String_get_at(names_, position_);
            if (((Dictionary_Integer_get_at(by_name_, SpiteString___retain(name_))).has_value)) {
                total_ = ({ int64_t spite_temp_9 = total_; int64_t spite_temp_10 = SpiteInteger_to_long(({ Nullable_Integer spite_temp_11 = Dictionary_Integer_get_at(by_name_, SpiteString___retain(name_)); spite_temp_11.has_value ? spite_temp_11.value : 0; })); int64_t spite_temp_12; if (__builtin_expect(__builtin_add_overflow(spite_temp_9, spite_temp_10, &spite_temp_12), 0)) spite_overflowed("total + by_name[name]", "a Long", "+", (int64_t)spite_temp_9, (int64_t)spite_temp_10, spite_site_2()); spite_temp_12; });
            }
            index_ = (index_ + 1);
            SpiteString___release(name_);
        }
        round_ = (round_ + 1);
    }
    int64_t spite_temp_13 = total_;
    return spite_temp_13;
}

int32_t Dictionary_Integer_find_slot(Dictionary_Integer* self, SpiteString key_) {
    if (((self->slot_count_ == 0))) {
        int32_t spite_temp_14 = (-(1));
        SpiteString___release(key_);
        return spite_temp_14;
    }
    uint64_t hash_ = Dictionary_Integer_hash_of(self, SpiteString___retain(key_));
    int32_t fragment_ = Dictionary_Integer_fragment_of(self, hash_);
    int32_t slot_ = Dictionary_Integer_home_slot(self, hash_);
    int32_t stored_ = SpiteMemory_Address_read_integer(self->slots_, SpiteInteger_to_long(({ int32_t spite_temp_15 = slot_; int32_t spite_temp_16 = 8; int32_t spite_temp_17; if (__builtin_expect(__builtin_mul_overflow(spite_temp_15, spite_temp_16, &spite_temp_17), 0)) spite_overflowed("slot * 8", "an Integer", "*", (int64_t)spite_temp_15, (int64_t)spite_temp_16, spite_site_3()); spite_temp_17; })));
    while (((stored_ != 0))) {
        {
            if ((((stored_ > 0))) && (((SpiteMemory_Address_read_integer(self->slots_, SpiteInteger_to_long(({ int32_t spite_temp_18 = ({ int32_t spite_temp_19 = slot_; int32_t spite_temp_20 = 8; int32_t spite_temp_21; if (__builtin_expect(__builtin_mul_overflow(spite_temp_19, spite_temp_20, &spite_temp_21), 0)) spite_overflowed("slot * 8", "an Integer", "*", (int64_t)spite_temp_19, (int64_t)spite_temp_20, spite_site_4()); spite_temp_21; }); int32_t spite_temp_22 = 4; int32_t spite_temp_23; if (__builtin_expect(__builtin_add_overflow(spite_temp_18, spite_temp_22, &spite_temp_23), 0)) spite_overflowed("slot * 8 + 4", "an Integer", "+", (int64_t)spite_temp_18, (int64_t)spite_temp_22, spite_site_4()); spite_temp_23; }))) == fragment_))) && ((({ SpiteString spite_temp_24 = List_String_get_at(self->entry_keys_, (stored_ - 1)); bool spite_equal = ((!SPITE_STRING_IS_NULL(spite_temp_24))) ? (({ SpiteString spite_temp_25 = spite_temp_24; SpiteString spite_temp_26 = key_; bool spite_temp_27 = SpiteString_equals(spite_temp_25, SpiteString___retain(spite_temp_26)); spite_temp_27; })) : false; SpiteString___release(spite_temp_24); spite_equal; })))) {
                int32_t spite_temp_28 = slot_;
                SpiteString___release(key_);
                return spite_temp_28;
            }
        }
        slot_ = Dictionary_Integer_next_slot(self, slot_);
        stored_ = SpiteMemory_Address_read_integer(self->slots_, SpiteInteger_to_long(({ int32_t spite_temp_29 = slot_; int32_t spite_temp_30 = 8; int32_t spite_temp_31; if (__builtin_expect(__builtin_mul_overflow(spite_temp_29, spite_temp_30, &spite_temp_31), 0)) spite_overflowed("slot * 8", "an Integer", "*", (int64_t)spite_temp_29, (int64_t)spite_temp_30, spite_site_5()); spite_temp_31; })));
    }
    int32_t spite_temp_32 = (-(1));
    SpiteString___release(key_);
    return spite_temp_32;
}

uint64_t Dictionary_Integer_hash_of(Dictionary_Integer* self, SpiteString key_) {
    {
        uint64_t hash_ = SpiteLong_to_unsigned_long(1469598103934665603);
        int32_t key_length_ = SpiteString_length(key_);
        int32_t index_ = 0;
        while (((index_ < key_length_))) {
            int32_t code_ = SpiteString_code_at(key_, index_);
            uint64_t mixed_ = SpiteUnsignedLong_bits_exclusive_or(hash_, ({ int32_t spite_temp_33 = code_; if (__builtin_expect(spite_temp_33 < 0 || spite_temp_33 > UINT64_MAX, 0)) spite_narrowed((int64_t)spite_temp_33, "an Integer", "an UnsignedLong", spite_site_6()); (uint64_t)spite_temp_33; }));
            hash_ = SpiteUnsignedLong_wrapping_multiply(mixed_, SpiteLong_to_unsigned_long(1099511628211));
            index_ = (index_ + 1);
        }
        uint64_t spite_temp_34 = hash_;
        SpiteString___release(key_);
        return spite_temp_34;
    }
}

int32_t Dictionary_Integer_home_slot(Dictionary_Integer* self, uint64_t hash_) {
    uint64_t high_ = SpiteUnsignedLong_shifted_right(hash_, 29);
    uint64_t mixed_ = SpiteUnsignedLong_bits_exclusive_or(hash_, high_);
    int32_t spite_temp_35 = ({ uint64_t spite_temp_36 = SpiteUnsignedLong_bits_and(mixed_, ({ int32_t spite_temp_37 = ({ int32_t spite_temp_38 = self->slot_count_; int32_t spite_temp_39 = 1; int32_t spite_temp_40; if (__builtin_expect(__builtin_sub_overflow(spite_temp_38, spite_temp_39, &spite_temp_40), 0)) spite_overflowed("slot_count - 1", "an Integer", "-", (int64_t)spite_temp_38, (int64_t)spite_temp_39, spite_site_7()); spite_temp_40; }); if (__builtin_expect(spite_temp_37 < 0 || spite_temp_37 > UINT64_MAX, 0)) spite_narrowed((int64_t)spite_temp_37, "an Integer", "an UnsignedLong", spite_site_7()); (uint64_t)spite_temp_37; })); if (__builtin_expect(spite_temp_36 > INT32_MAX, 0)) spite_narrowed_unsigned((uint64_t)spite_temp_36, "an UnsignedLong", "an Integer", spite_site_7()); (int32_t)spite_temp_36; });
    return spite_temp_35;
}

int32_t Dictionary_Integer_fragment_of(Dictionary_Integer* self, uint64_t hash_) {
    int32_t spite_temp_41 = ({ uint64_t spite_temp_42 = SpiteUnsignedLong_shifted_right(hash_, 33); if (__builtin_expect(spite_temp_42 > INT32_MAX, 0)) spite_narrowed_unsigned((uint64_t)spite_temp_42, "an UnsignedLong", "an Integer", spite_site_8()); (int32_t)spite_temp_42; });
    return spite_temp_41;
}
