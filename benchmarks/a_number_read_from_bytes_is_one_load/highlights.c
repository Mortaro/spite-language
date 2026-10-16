/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int64_t Naive_decode___held_0(Naive* self, Buffer* records_) {
    (records_)->byte_order_ = Buffer_ByteOrder_big_endian;
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t position_ = 0;
    int32_t spite_temp_1 = Buffer_count(records_);
    uint8_t* spite_temp_2 = (uint8_t*)(intptr_t)(records_)->items_;
    int32_t spite_temp_3 = (records_)->byte_order_;
    while (position_ < spite_temp_1 - 15) {
        #if defined(__clang__)
        #pragma clang fp contract(fast) reassociate(on)
        #endif
        int32_t identity_ = ((spite_temp_3) == Buffer_ByteOrder_big_endian ? SpiteMemory_Address_read_integer_big_endian((intptr_t)(spite_temp_2), position_) : SpiteMemory_Address_read_integer((intptr_t)(spite_temp_2), position_));
        int32_t across_ = ((spite_temp_3) == Buffer_ByteOrder_big_endian ? SpiteMemory_Address_read_integer_big_endian((intptr_t)(spite_temp_2), position_ + 4) : SpiteMemory_Address_read_integer((intptr_t)(spite_temp_2), position_ + 4));
        int32_t down_ = ((spite_temp_3) == Buffer_ByteOrder_big_endian ? SpiteMemory_Address_read_integer_big_endian((intptr_t)(spite_temp_2), position_ + 8) : SpiteMemory_Address_read_integer((intptr_t)(spite_temp_2), position_ + 8));
        int16_t kind_ = ((spite_temp_3) == Buffer_ByteOrder_big_endian ? SpiteMemory_Address_read_short_big_endian((intptr_t)(spite_temp_2), position_ + 12) : SpiteMemory_Address_read_short((intptr_t)(spite_temp_2), position_ + 12));
        int16_t flags_ = ((spite_temp_3) == Buffer_ByteOrder_big_endian ? SpiteMemory_Address_read_short_big_endian((intptr_t)(spite_temp_2), position_ + 14) : SpiteMemory_Address_read_short((intptr_t)(spite_temp_2), position_ + 14));
        total_ = ({ int64_t spite_temp_4 = ({ int64_t spite_temp_5 = ({ int64_t spite_temp_6 = ({ int64_t spite_temp_7 = total_; int64_t spite_temp_8 = SpiteInteger_to_long(identity_); int64_t spite_temp_9; if (__builtin_expect(__builtin_add_overflow(spite_temp_7, spite_temp_8, &spite_temp_9), 0)) spite_overflowed("total + identity", "a Long", "+", (int64_t)spite_temp_7, (int64_t)spite_temp_8, spite_site_1()); spite_temp_9; }); int64_t spite_temp_10 = SpiteInteger_to_long(across_); int64_t spite_temp_11; if (__builtin_expect(__builtin_add_overflow(spite_temp_6, spite_temp_10, &spite_temp_11), 0)) spite_overflowed("total + identity + across", "a Long", "+", (int64_t)spite_temp_6, (int64_t)spite_temp_10, spite_site_1()); spite_temp_11; }); int64_t spite_temp_12 = SpiteInteger_to_long(({ int32_t spite_temp_13 = down_; int32_t spite_temp_14 = SpiteShort_to_integer(kind_); int32_t spite_temp_15; if (__builtin_expect(__builtin_mul_overflow(spite_temp_13, spite_temp_14, &spite_temp_15), 0)) spite_overflowed("down * kind", "an Integer", "*", (int64_t)spite_temp_13, (int64_t)spite_temp_14, spite_site_1()); spite_temp_15; })); int64_t spite_temp_16; if (__builtin_expect(__builtin_add_overflow(spite_temp_5, spite_temp_12, &spite_temp_16), 0)) spite_overflowed("total + identity + across + down * kind", "a Long", "+", (int64_t)spite_temp_5, (int64_t)spite_temp_12, spite_site_1()); spite_temp_16; }); int64_t spite_temp_17 = SpiteShort_to_long(flags_); int64_t spite_temp_18; if (__builtin_expect(__builtin_add_overflow(spite_temp_4, spite_temp_17, &spite_temp_18), 0)) spite_overflowed("total + identity + across + down * kind + flags", "a Long", "+", (int64_t)spite_temp_4, (int64_t)spite_temp_17, spite_site_1()); spite_temp_18; });
        position_ = ({ int32_t spite_temp_19 = position_; int32_t spite_temp_20 = 16; int32_t spite_temp_21; if (__builtin_expect(__builtin_add_overflow(spite_temp_19, spite_temp_20, &spite_temp_21), 0)) spite_overflowed("position + 16", "an Integer", "+", (int64_t)spite_temp_19, (int64_t)spite_temp_20, spite_site_2()); spite_temp_21; });
    }
    int64_t spite_temp_22 = total_;
    return spite_temp_22;
}
