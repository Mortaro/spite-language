/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

struct Dictionary_Integer_by_Integer {
    SpiteHeader header;
    List_Integer* entry_keys_;
    List_Integer* entry_values_;
    Memory_Heap* heap_;
    int64_t slots_;
    int32_t slot_count_;
    int32_t used_slots_;
};

int64_t Naive_look_up___held_0(Naive* self, Dictionary_Integer_by_Integer* by_number_, int32_t count_) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t round_ = 0;
    while (((round_ < 40))) {
        int32_t index_ = 0;
        while (((index_ < count_))) {
            int32_t key_ = Naive_key_of(self, ({ int32_t spite_temp_1 = index_; int32_t spite_temp_2 = round_; int32_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("index + round", "an Integer", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; }));
            Nullable_Integer found_ = Dictionary_Integer_by_Integer_get_at(by_number_, key_);
            if ((found_).has_value) {
                total_ = ({ int64_t spite_temp_4 = total_; int64_t spite_temp_5 = SpiteInteger_to_long((found_).value); int64_t spite_temp_6; if (__builtin_expect(__builtin_add_overflow(spite_temp_4, spite_temp_5, &spite_temp_6), 0)) spite_overflowed("total + found", "a Long", "+", (int64_t)spite_temp_4, (int64_t)spite_temp_5, spite_site_2()); spite_temp_6; });
            }
            index_ = (index_ + 1);
        }
        round_ = (round_ + 1);
    }
    int64_t spite_temp_7 = total_;
    return spite_temp_7;
}

int32_t Dictionary_Integer_by_Integer_find_slot(Dictionary_Integer_by_Integer* self, int32_t key_) {
    if (((self->slot_count_ == 0))) {
        int32_t spite_temp_8 = (-(1));
        return spite_temp_8;
    }
    uint64_t hash_ = Dictionary_Integer_by_Integer_hash_of(self, key_);
    int32_t fragment_ = Dictionary_Integer_by_Integer_fragment_of(self, hash_);
    int32_t slot_ = Dictionary_Integer_by_Integer_home_slot(self, hash_);
    int32_t stored_ = SpiteMemory_Address_read_integer(self->slots_, SpiteInteger_to_long(({ int32_t spite_temp_9 = slot_; int32_t spite_temp_10 = 8; int32_t spite_temp_11; if (__builtin_expect(__builtin_mul_overflow(spite_temp_9, spite_temp_10, &spite_temp_11), 0)) spite_overflowed("slot * 8", "an Integer", "*", (int64_t)spite_temp_9, (int64_t)spite_temp_10, spite_site_3()); spite_temp_11; })));
    while (((stored_ != 0))) {
        {
            if ((((stored_ > 0))) && ((({ Nullable_Integer spite_temp_12 = List_Integer_get_at(self->entry_keys_, (stored_ - 1)); bool spite_equal = (spite_temp_12.has_value) ? ((spite_temp_12.value == key_)) : false; spite_equal; })))) {
                int32_t spite_temp_13 = slot_;
                return spite_temp_13;
            }
        }
        slot_ = Dictionary_Integer_by_Integer_next_slot(self, slot_);
        stored_ = SpiteMemory_Address_read_integer(self->slots_, SpiteInteger_to_long(({ int32_t spite_temp_14 = slot_; int32_t spite_temp_15 = 8; int32_t spite_temp_16; if (__builtin_expect(__builtin_mul_overflow(spite_temp_14, spite_temp_15, &spite_temp_16), 0)) spite_overflowed("slot * 8", "an Integer", "*", (int64_t)spite_temp_14, (int64_t)spite_temp_15, spite_site_4()); spite_temp_16; })));
    }
    int32_t spite_temp_17 = (-(1));
    return spite_temp_17;
}

uint64_t Dictionary_Integer_by_Integer_hash_of(Dictionary_Integer_by_Integer* self, int32_t key_) {
    {
        {
            uint32_t bits_ = SpiteInteger_bits_as_unsigned(key_);
            uint64_t number_ = SpiteUnsignedInteger_to_unsigned_long(bits_);
            uint64_t spite_temp_18 = SpiteUnsignedLong_wrapping_multiply(number_, SpiteLong_to_unsigned_long(6364136223846793005));
            return spite_temp_18;
        }
    }
}

int32_t Dictionary_Integer_by_Integer_home_slot(Dictionary_Integer_by_Integer* self, uint64_t hash_) {
    uint64_t high_ = SpiteUnsignedLong_shifted_right(hash_, 29);
    uint64_t mixed_ = SpiteUnsignedLong_bits_exclusive_or(hash_, high_);
    int32_t spite_temp_19 = ({ uint64_t spite_temp_20 = SpiteUnsignedLong_bits_and(mixed_, ({ int32_t spite_temp_21 = ({ int32_t spite_temp_22 = self->slot_count_; int32_t spite_temp_23 = 1; int32_t spite_temp_24; if (__builtin_expect(__builtin_sub_overflow(spite_temp_22, spite_temp_23, &spite_temp_24), 0)) spite_overflowed("slot_count - 1", "an Integer", "-", (int64_t)spite_temp_22, (int64_t)spite_temp_23, spite_site_5()); spite_temp_24; }); if (__builtin_expect(spite_temp_21 < 0 || spite_temp_21 > UINT64_MAX, 0)) spite_narrowed((int64_t)spite_temp_21, "an Integer", "an UnsignedLong", spite_site_5()); (uint64_t)spite_temp_21; })); if (__builtin_expect(spite_temp_20 > INT32_MAX, 0)) spite_narrowed_unsigned((uint64_t)spite_temp_20, "an UnsignedLong", "an Integer", spite_site_5()); (int32_t)spite_temp_20; });
    return spite_temp_19;
}
