/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

#define SpiteMemory_Address_read_byte(self, offset) ({ uint8_t spite_read; memcpy(&spite_read, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_read)); spite_read; })

#define SpiteMemory_Address_write_byte(self, offset, value) memcpy(((char*)(intptr_t)(self) + (offset)), &(uint8_t){ (value) }, sizeof(uint8_t))

SpiteString Base64__encoded(Base64* self, Buffer* bytes_, SpiteString alphabet_, bool padded_) {
    int32_t count_ = Buffer_count(bytes_);
    Buffer spite_slot_1;
    Buffer* written_ = Buffer___make_into(&spite_slot_1, ({ int32_t spite_temp_1 = (({ int32_t spite_temp_2 = count_; int32_t spite_temp_3 = 2; int32_t spite_temp_4; if (__builtin_expect(__builtin_add_overflow(spite_temp_2, spite_temp_3, &spite_temp_4), 0)) spite_overflowed("count + 2", "an Integer", "+", (int64_t)spite_temp_2, (int64_t)spite_temp_3, spite_site_1()); spite_temp_4; }) / 3); int32_t spite_temp_5 = 4; int32_t spite_temp_6; if (__builtin_expect(__builtin_mul_overflow(spite_temp_1, spite_temp_5, &spite_temp_6), 0)) spite_overflowed("(count + 2) / 3 * 4", "an Integer", "*", (int64_t)spite_temp_1, (int64_t)spite_temp_5, spite_site_1()); spite_temp_6; }));
    int32_t index_ = 0;
    while (((index_ < count_))) {
        int32_t first_ = SpiteByte_to_integer(Base64__byte(self, Buffer___retain(bytes_), index_));
        int32_t group_ = SpiteInteger_shifted_left(first_, 16);
        int32_t present_ = (count_ - index_);
        if (((present_ > 1))) {
            int32_t second_ = SpiteByte_to_integer(Base64__byte(self, Buffer___retain(bytes_), (index_ + 1)));
            int32_t shifted_ = SpiteInteger_shifted_left(second_, 8);
            group_ = SpiteInteger_bits_or(group_, shifted_);
        }
        if (((present_ > 2))) {
            int32_t third_ = SpiteByte_to_integer(Base64__byte(self, Buffer___retain(bytes_), ({ int32_t spite_temp_7 = index_; int32_t spite_temp_8 = 2; int32_t spite_temp_9; if (__builtin_expect(__builtin_add_overflow(spite_temp_7, spite_temp_8, &spite_temp_9), 0)) spite_overflowed("index + 2", "an Integer", "+", (int64_t)spite_temp_7, (int64_t)spite_temp_8, spite_site_2()); spite_temp_9; })));
            group_ = SpiteInteger_bits_or(group_, third_);
        }
        Base64__write_digit(self, Buffer___retain(written_), SpiteString___retain(alphabet_), group_, 18);
        Base64__write_digit(self, Buffer___retain(written_), SpiteString___retain(alphabet_), group_, 12);
        if (((present_ > 1))) {
            Base64__write_digit(self, Buffer___retain(written_), SpiteString___retain(alphabet_), group_, 6);
        }
        else {
            if ((padded_)) {
                Buffer_append(written_, SpiteInteger_to_byte(61));
            }
        }
        if (((present_ > 2))) {
            Base64__write_digit(self, Buffer___retain(written_), SpiteString___retain(alphabet_), group_, 0);
        }
        else {
            if ((padded_)) {
                Buffer_append(written_, SpiteInteger_to_byte(61));
            }
        }
        index_ = ({ int32_t spite_temp_10 = index_; int32_t spite_temp_11 = 3; int32_t spite_temp_12; if (__builtin_expect(__builtin_add_overflow(spite_temp_10, spite_temp_11, &spite_temp_12), 0)) spite_overflowed("index + 3", "an Integer", "+", (int64_t)spite_temp_10, (int64_t)spite_temp_11, spite_site_3()); spite_temp_12; });
    }
    SpiteString text_ = Buffer_to_utf8_text(written_);
    if (!((!SPITE_STRING_IS_NULL(text_)))) {
        spite_failed_1(alphabet_, padded_, count_, index_, self);
    }
    SpiteString spite_temp_13 = SpiteString___retain(text_);
    SpiteString___release(text_);
    Buffer_drop(written_);
    SpiteString___release(alphabet_);
    Buffer___release(bytes_);
    return spite_temp_13;
}

void Base64__write_digit(Base64* self, Buffer* written_, SpiteString alphabet_, int32_t group_, int32_t shift_) {
    int32_t value_ = SpiteInteger_bits_and(SpiteInteger_shifted_right(group_, shift_), 63);
    uint8_t digit_ = ({ int32_t spite_temp_14 = SpiteString_code_at(alphabet_, value_); if (__builtin_expect(spite_temp_14 < 0 || spite_temp_14 > UINT8_MAX, 0)) spite_narrowed((int64_t)spite_temp_14, "an Integer", "a Byte", spite_site_4()); (uint8_t)spite_temp_14; });
    Buffer_append(written_, digit_);
    SpiteString___release(alphabet_);
    Buffer___release(written_);
}
