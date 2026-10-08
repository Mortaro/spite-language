/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int64_t Naive_decode___held_0(Naive* self, List_Byte* records_) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t position_ = 0;
    int32_t spite_temp_1 = List_Byte_count(records_);
    uint8_t* spite_temp_2 = (uint8_t*)(intptr_t)(records_)->items_;
    while (position_ < spite_temp_1 - 15) {
        #if defined(__clang__)
        #pragma clang fp contract(fast) reassociate(on)
        #endif
        int32_t identity_ = SpiteMemory_Address_read_integer_big_endian((intptr_t)(spite_temp_2), position_);
        int32_t across_ = SpiteMemory_Address_read_integer_big_endian((intptr_t)(spite_temp_2), position_ + 4);
        int32_t down_ = SpiteMemory_Address_read_integer_big_endian((intptr_t)(spite_temp_2), position_ + 8);
        int16_t kind_ = SpiteMemory_Address_read_short_big_endian((intptr_t)(spite_temp_2), position_ + 12);
        int16_t flags_ = SpiteMemory_Address_read_short_big_endian((intptr_t)(spite_temp_2), position_ + 14);
        total_ = ({ int64_t spite_temp_3 = ({ int64_t spite_temp_4 = ({ int64_t spite_temp_5 = ({ int64_t spite_temp_6 = total_; int64_t spite_temp_7 = SpiteInteger_to_long(identity_); int64_t spite_temp_8; if (__builtin_expect(__builtin_add_overflow(spite_temp_6, spite_temp_7, &spite_temp_8), 0)) spite_overflowed("total + identity", "a Long", "+", (int64_t)spite_temp_6, (int64_t)spite_temp_7, spite_site_1()); spite_temp_8; }); int64_t spite_temp_9 = SpiteInteger_to_long(across_); int64_t spite_temp_10; if (__builtin_expect(__builtin_add_overflow(spite_temp_5, spite_temp_9, &spite_temp_10), 0)) spite_overflowed("total + identity + across", "a Long", "+", (int64_t)spite_temp_5, (int64_t)spite_temp_9, spite_site_1()); spite_temp_10; }); int64_t spite_temp_11 = SpiteInteger_to_long(({ int32_t spite_temp_12 = down_; int32_t spite_temp_13 = SpiteShort_to_integer(kind_); int32_t spite_temp_14; if (__builtin_expect(__builtin_mul_overflow(spite_temp_12, spite_temp_13, &spite_temp_14), 0)) spite_overflowed("down * kind", "an Integer", "*", (int64_t)spite_temp_12, (int64_t)spite_temp_13, spite_site_1()); spite_temp_14; })); int64_t spite_temp_15; if (__builtin_expect(__builtin_add_overflow(spite_temp_4, spite_temp_11, &spite_temp_15), 0)) spite_overflowed("total + identity + across + down * kind", "a Long", "+", (int64_t)spite_temp_4, (int64_t)spite_temp_11, spite_site_1()); spite_temp_15; }); int64_t spite_temp_16 = SpiteShort_to_long(flags_); int64_t spite_temp_17; if (__builtin_expect(__builtin_add_overflow(spite_temp_3, spite_temp_16, &spite_temp_17), 0)) spite_overflowed("total + identity + across + down * kind + flags", "a Long", "+", (int64_t)spite_temp_3, (int64_t)spite_temp_16, spite_site_1()); spite_temp_17; });
        position_ = ({ int32_t spite_temp_18 = position_; int32_t spite_temp_19 = 16; int32_t spite_temp_20; if (__builtin_expect(__builtin_add_overflow(spite_temp_18, spite_temp_19, &spite_temp_20), 0)) spite_overflowed("position + 16", "an Integer", "+", (int64_t)spite_temp_18, (int64_t)spite_temp_19, spite_site_2()); spite_temp_20; });
    }
    int64_t spite_temp_21 = total_;
    return spite_temp_21;
}
