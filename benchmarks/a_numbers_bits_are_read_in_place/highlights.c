/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

#define SpiteFloat_bits(self) (((union { float spite_value; uint32_t spite_bits; }) { .spite_value = (self) }).spite_bits)

#define SpiteUnsignedInteger_bits_as_float(self) (((union { uint32_t spite_value; float spite_bits; }) { .spite_value = (self) }).spite_bits)

int64_t Naive_round_trips(Naive* self, int32_t count_) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t index_ = 0;
    while (((index_ < count_))) {
        float value_ = SpiteInteger_to_float(({ int32_t spite_temp_1 = (index_ % 160000); int32_t spite_temp_2 = 20000; int32_t spite_temp_3; if (__builtin_expect(__builtin_sub_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("index % 160000 - 20000", "an Integer", "-", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; }));
        value_ = (value_ * 0.5f);
        uint16_t half_ = SpiteFloat_to_half_precision(value_);
        float back_ = SpiteUnsignedShort_half_precision_to_float(half_);
        uint32_t back_bits_ = SpiteFloat_bits(back_);
        total_ = ({ int64_t spite_temp_4 = ({ int64_t spite_temp_5 = total_; int64_t spite_temp_6 = SpiteUnsignedShort_to_long(half_); int64_t spite_temp_7; if (__builtin_expect(__builtin_add_overflow(spite_temp_5, spite_temp_6, &spite_temp_7), 0)) spite_overflowed("total + half", "a Long", "+", (int64_t)spite_temp_5, (int64_t)spite_temp_6, spite_site_2()); spite_temp_7; }); int64_t spite_temp_8 = SpiteUnsignedInteger_to_long(back_bits_); int64_t spite_temp_9; if (__builtin_expect(__builtin_add_overflow(spite_temp_4, spite_temp_8, &spite_temp_9), 0)) spite_overflowed("total + half + back_bits", "a Long", "+", (int64_t)spite_temp_4, (int64_t)spite_temp_8, spite_site_2()); spite_temp_9; });
        index_ = (index_ + 1);
    }
    int64_t spite_temp_10 = total_;
    return spite_temp_10;
}

uint16_t SpiteFloat_to_half_precision(float self) {
    uint32_t bit_pattern_ = SpiteFloat_bits(self);
    uint32_t sign_part_ = SpiteUnsignedInteger_bits_and(SpiteUnsignedInteger_shifted_right(bit_pattern_, 16), SpiteInteger_to_unsigned_integer(32768));
    uint32_t magnitude_ = SpiteUnsignedInteger_bits_and(bit_pattern_, SpiteInteger_to_unsigned_integer(2147483647));
    uint32_t half_ = SpiteInteger_to_unsigned_integer(0);
    if (((magnitude_ >= SpiteInteger_to_unsigned_integer(1199570944)))) {
        half_ = SpiteInteger_to_unsigned_integer(31744);
        if (((magnitude_ > SpiteInteger_to_unsigned_integer(2139095040)))) {
            half_ = SpiteInteger_to_unsigned_integer(32256);
        }
    }
    else {
        if (((magnitude_ < SpiteInteger_to_unsigned_integer(947912704)))) {
            uint32_t magic_ = SpiteInteger_to_unsigned_integer(1056964608);
            float magnitude_value_ = SpiteUnsignedInteger_bits_as_float(magnitude_);
            float magic_value_ = SpiteUnsignedInteger_bits_as_float(magic_);
            float shifted_ = (magnitude_value_ + magic_value_);
            half_ = ({ uint32_t spite_temp_11 = SpiteFloat_bits(shifted_); uint32_t spite_temp_12 = magic_; uint32_t spite_temp_13; if (__builtin_expect(__builtin_sub_overflow(spite_temp_11, spite_temp_12, &spite_temp_13), 0)) spite_overflowed_unsigned("shifted.bits() - magic", "an UnsignedInteger", "-", (uint64_t)spite_temp_11, (uint64_t)spite_temp_12, spite_site_3()); spite_temp_13; });
        }
        else {
            uint32_t odd_ = SpiteUnsignedInteger_bits_and(SpiteUnsignedInteger_shifted_right(magnitude_, 13), SpiteInteger_to_unsigned_integer(1));
            uint32_t rounded_ = SpiteUnsignedInteger_wrapping_sum(SpiteUnsignedInteger_wrapping_sum(magnitude_, SpiteLong_to_unsigned_integer(3355447295)), odd_);
            half_ = SpiteUnsignedInteger_shifted_right(rounded_, 13);
        }
    }
    uint16_t spite_temp_14 = ({ uint32_t spite_temp_15 = SpiteUnsignedInteger_bits_or(half_, sign_part_); if (__builtin_expect(spite_temp_15 > UINT16_MAX, 0)) spite_narrowed_unsigned((uint64_t)spite_temp_15, "an UnsignedInteger", "an UnsignedShort", spite_site_4()); (uint16_t)spite_temp_15; });
    return spite_temp_14;
}

float SpiteUnsignedShort_half_precision_to_float(uint16_t self) {
    uint32_t whole_ = SpiteUnsignedShort_to_unsigned_integer(self);
    uint32_t shifted_exponent_ = SpiteInteger_to_unsigned_integer(260046848);
    uint32_t bit_pattern_ = SpiteUnsignedInteger_shifted_left(SpiteUnsignedInteger_bits_and(whole_, SpiteInteger_to_unsigned_integer(32767)), 13);
    uint32_t exponent_ = SpiteUnsignedInteger_bits_and(shifted_exponent_, bit_pattern_);
    bit_pattern_ = ({ uint32_t spite_temp_16 = bit_pattern_; uint32_t spite_temp_17 = SpiteInteger_to_unsigned_integer(939524096); uint32_t spite_temp_18; if (__builtin_expect(__builtin_add_overflow(spite_temp_16, spite_temp_17, &spite_temp_18), 0)) spite_overflowed_unsigned("bit_pattern + 939524096", "an UnsignedInteger", "+", (uint64_t)spite_temp_16, (uint64_t)spite_temp_17, spite_site_5()); spite_temp_18; });
    if (((exponent_ == shifted_exponent_))) {
        bit_pattern_ = ({ uint32_t spite_temp_19 = bit_pattern_; uint32_t spite_temp_20 = SpiteInteger_to_unsigned_integer(939524096); uint32_t spite_temp_21; if (__builtin_expect(__builtin_add_overflow(spite_temp_19, spite_temp_20, &spite_temp_21), 0)) spite_overflowed_unsigned("bit_pattern + 939524096", "an UnsignedInteger", "+", (uint64_t)spite_temp_19, (uint64_t)spite_temp_20, spite_site_6()); spite_temp_21; });
    }
    else {
        if (((exponent_ == SpiteInteger_to_unsigned_integer(0)))) {
            bit_pattern_ = ({ uint32_t spite_temp_22 = bit_pattern_; uint32_t spite_temp_23 = SpiteInteger_to_unsigned_integer(8388608); uint32_t spite_temp_24; if (__builtin_expect(__builtin_add_overflow(spite_temp_22, spite_temp_23, &spite_temp_24), 0)) spite_overflowed_unsigned("bit_pattern + 8388608", "an UnsignedInteger", "+", (uint64_t)spite_temp_22, (uint64_t)spite_temp_23, spite_site_7()); spite_temp_24; });
            uint32_t magic_ = SpiteInteger_to_unsigned_integer(947912704);
            float value_ = (SpiteUnsignedInteger_bits_as_float(bit_pattern_) - SpiteUnsignedInteger_bits_as_float(magic_));
            bit_pattern_ = SpiteFloat_bits(value_);
        }
    }
    uint32_t sign_part_ = SpiteUnsignedInteger_shifted_left(SpiteUnsignedInteger_bits_and(whole_, SpiteInteger_to_unsigned_integer(32768)), 16);
    bit_pattern_ = SpiteUnsignedInteger_bits_or(bit_pattern_, sign_part_);
    float spite_temp_25 = SpiteUnsignedInteger_bits_as_float(bit_pattern_);
    return spite_temp_25;
}
