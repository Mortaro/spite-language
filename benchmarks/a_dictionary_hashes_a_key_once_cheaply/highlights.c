/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int64_t Naive_look_up___held_0_1(Naive* self, List_String* names_, Dictionary_Integer* by_name_) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t round_ = 0;
    while (((round_ < 20))) {
        int32_t index_ = 0;
        while (((index_ < 100000))) {
            int32_t position_ = (((index_ * 7) + round_) % 5000);
            if (!(({ List_String* spite_temp_1 = names_; int32_t spite_temp_2 = position_; (spite_temp_2 >= 0 && spite_temp_2 < (spite_temp_1)->item_count_) && (!SPITE_STRING_IS_NULL(((SpiteString*)(intptr_t)(spite_temp_1)->items_)[spite_temp_2])); }))) {
                spite_failed_1(position_, names_, total_, round_, index_);
            }
            SpiteString name_ = List_String_get_at(names_, position_);
            Nullable_Integer spite_temp_3;
            if (((spite_temp_3 = Dictionary_Integer_get_at(by_name_, SpiteString___retain(name_))).has_value)) {
                total_ = ({ int64_t spite_temp_4 = total_; int64_t spite_temp_5 = SpiteInteger_to_long(spite_temp_3.value); int64_t spite_temp_6; if (__builtin_expect(__builtin_add_overflow(spite_temp_4, spite_temp_5, &spite_temp_6), 0)) spite_overflowed("total + by_name[name]", "a Long", "+", (int64_t)spite_temp_4, (int64_t)spite_temp_5, spite_site_1()); spite_temp_6; });
            }
            index_ = (index_ + 1);
            SpiteString___release(name_);
        }
        round_ = (round_ + 1);
    }
    int64_t spite_temp_7 = total_;
    return spite_temp_7;
}

int32_t Dictionary_Integer_find_slot(Dictionary_Integer* self, SpiteString key_) {
    if (((self->slot_count_ == 0))) {
        int32_t spite_temp_8 = (-(1));
        SpiteString___release(key_);
        return spite_temp_8;
    }
    uint64_t hash_ = Dictionary_Integer_hash_of(self, SpiteString___retain(key_));
    int32_t fragment_ = Dictionary_Integer_fragment_of(self, hash_);
    int32_t slot_ = Dictionary_Integer_home_slot(self, hash_);
    int32_t stored_ = SpiteMemory_Address_read_integer(self->slots_, SpiteInteger_to_long(({ int32_t spite_temp_9 = slot_; int32_t spite_temp_10 = 8; int32_t spite_temp_11; if (__builtin_expect(__builtin_mul_overflow(spite_temp_9, spite_temp_10, &spite_temp_11), 0)) spite_overflowed("slot * 8", "an Integer", "*", (int64_t)spite_temp_9, (int64_t)spite_temp_10, spite_site_2()); spite_temp_11; })));
    while (((stored_ != 0))) {
        {
            if ((((stored_ > 0))) && (((SpiteMemory_Address_read_integer(self->slots_, SpiteInteger_to_long(({ int32_t spite_temp_12 = ({ int32_t spite_temp_13 = slot_; int32_t spite_temp_14 = 8; int32_t spite_temp_15; if (__builtin_expect(__builtin_mul_overflow(spite_temp_13, spite_temp_14, &spite_temp_15), 0)) spite_overflowed("slot * 8", "an Integer", "*", (int64_t)spite_temp_13, (int64_t)spite_temp_14, spite_site_3()); spite_temp_15; }); int32_t spite_temp_16 = 4; int32_t spite_temp_17; if (__builtin_expect(__builtin_add_overflow(spite_temp_12, spite_temp_16, &spite_temp_17), 0)) spite_overflowed("slot * 8 + 4", "an Integer", "+", (int64_t)spite_temp_12, (int64_t)spite_temp_16, spite_site_3()); spite_temp_17; }))) == fragment_))) && ((({ SpiteString spite_temp_18 = List_String_get_at(self->entry_keys_, (stored_ - 1)); bool spite_equal = ((!SPITE_STRING_IS_NULL(spite_temp_18))) ? (({ SpiteString spite_temp_19 = spite_temp_18; SpiteString spite_temp_20 = key_; bool spite_temp_21 = SpiteString_equals(spite_temp_19, SpiteString___retain(spite_temp_20)); spite_temp_21; })) : false; SpiteString___release(spite_temp_18); spite_equal; })))) {
                int32_t spite_temp_22 = slot_;
                SpiteString___release(key_);
                return spite_temp_22;
            }
        }
        slot_ = Dictionary_Integer_next_slot(self, slot_);
        stored_ = SpiteMemory_Address_read_integer(self->slots_, SpiteInteger_to_long(({ int32_t spite_temp_23 = slot_; int32_t spite_temp_24 = 8; int32_t spite_temp_25; if (__builtin_expect(__builtin_mul_overflow(spite_temp_23, spite_temp_24, &spite_temp_25), 0)) spite_overflowed("slot * 8", "an Integer", "*", (int64_t)spite_temp_23, (int64_t)spite_temp_24, spite_site_4()); spite_temp_25; })));
    }
    int32_t spite_temp_26 = (-(1));
    SpiteString___release(key_);
    return spite_temp_26;
}

uint64_t Dictionary_Integer_hash_of(Dictionary_Integer* self, SpiteString key_) {
    {
        uint64_t hash_ = SpiteLong_to_unsigned_long(1469598103934665603);
        int32_t key_length_ = SpiteString_length(key_);
        int32_t index_ = 0;
        while (((index_ < key_length_))) {
            int32_t code_ = SpiteString_code_at(key_, index_);
            uint64_t mixed_ = SpiteUnsignedLong_bits_exclusive_or(hash_, ({ int32_t spite_temp_27 = code_; if (__builtin_expect(spite_temp_27 < 0 || spite_temp_27 > UINT64_MAX, 0)) spite_narrowed((int64_t)spite_temp_27, "an Integer", "an UnsignedLong", spite_site_5()); (uint64_t)spite_temp_27; }));
            hash_ = SpiteUnsignedLong_wrapping_multiply(mixed_, SpiteLong_to_unsigned_long(1099511628211));
            index_ = (index_ + 1);
        }
        uint64_t spite_temp_28 = hash_;
        SpiteString___release(key_);
        return spite_temp_28;
    }
}

int32_t Dictionary_Integer_home_slot(Dictionary_Integer* self, uint64_t hash_) {
    uint64_t high_ = SpiteUnsignedLong_shifted_right(hash_, 29);
    uint64_t mixed_ = SpiteUnsignedLong_bits_exclusive_or(hash_, high_);
    int32_t spite_temp_29 = ({ uint64_t spite_temp_30 = SpiteUnsignedLong_bits_and(mixed_, ({ int32_t spite_temp_31 = ({ int32_t spite_temp_32 = self->slot_count_; int32_t spite_temp_33 = 1; int32_t spite_temp_34; if (__builtin_expect(__builtin_sub_overflow(spite_temp_32, spite_temp_33, &spite_temp_34), 0)) spite_overflowed("slot_count - 1", "an Integer", "-", (int64_t)spite_temp_32, (int64_t)spite_temp_33, spite_site_6()); spite_temp_34; }); if (__builtin_expect(spite_temp_31 < 0 || spite_temp_31 > UINT64_MAX, 0)) spite_narrowed((int64_t)spite_temp_31, "an Integer", "an UnsignedLong", spite_site_6()); (uint64_t)spite_temp_31; })); if (__builtin_expect(spite_temp_30 > INT32_MAX, 0)) spite_narrowed_unsigned((uint64_t)spite_temp_30, "an UnsignedLong", "an Integer", spite_site_6()); (int32_t)spite_temp_30; });
    return spite_temp_29;
}

int32_t Dictionary_Integer_fragment_of(Dictionary_Integer* self, uint64_t hash_) {
    int32_t spite_temp_35 = ({ uint64_t spite_temp_36 = SpiteUnsignedLong_shifted_right(hash_, 33); if (__builtin_expect(spite_temp_36 > INT32_MAX, 0)) spite_narrowed_unsigned((uint64_t)spite_temp_36, "an UnsignedLong", "an Integer", spite_site_7()); (int32_t)spite_temp_36; });
    return spite_temp_35;
}
