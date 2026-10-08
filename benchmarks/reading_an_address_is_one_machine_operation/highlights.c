/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

#define SpiteMemory_Address_read_byte(self, offset) ({ uint8_t spite_read; memcpy(&spite_read, ((char*)(intptr_t)(self) + (offset)), sizeof(spite_read)); spite_read; })

#define SpiteMemory_Address_write_byte(self, offset, value) memcpy(((char*)(intptr_t)(self) + (offset)), &(uint8_t){ (value) }, sizeof(uint8_t))

SpiteString Base64__encoded(Base64* self, List_Byte* bytes_, SpiteString alphabet_, bool padded_) {
    int32_t count_ = List_Byte_count(bytes_);
    int64_t room_ = SpiteInteger_to_long(({ int32_t spite_temp_1 = (({ int32_t spite_temp_2 = count_; int32_t spite_temp_3 = 2; int32_t spite_temp_4; if (__builtin_expect(__builtin_add_overflow(spite_temp_2, spite_temp_3, &spite_temp_4), 0)) spite_overflowed("count + 2", "an Integer", "+", (int64_t)spite_temp_2, (int64_t)spite_temp_3, spite_site_1()); spite_temp_4; }) / 3); int32_t spite_temp_5 = 4; int32_t spite_temp_6; if (__builtin_expect(__builtin_mul_overflow(spite_temp_1, spite_temp_5, &spite_temp_6), 0)) spite_overflowed("(count + 2) / 3 * 4", "an Integer", "*", (int64_t)spite_temp_1, (int64_t)spite_temp_5, spite_site_1()); spite_temp_6; }));
    int64_t spite_temp_7[32];
    int64_t spite_temp_8 = (room_ + SpiteInteger_to_long(1));
    int64_t address_ = spite_temp_8 <= 256 ? (int64_t)(intptr_t)spite_temp_7 : Memory_Heap_allocate(self->heap_, spite_temp_8);
    int64_t source_ = (bytes_)->items_;
    int64_t written_ = SpiteInteger_to_long(0);
    int32_t index_ = 0;
    while (((index_ < count_))) {
        int32_t first_ = SpiteByte_to_integer(SpiteMemory_Address_read_byte(source_, SpiteInteger_to_long(index_)));
        int32_t group_ = SpiteInteger_shifted_left(first_, 16);
        int32_t present_ = (count_ - index_);
        if (((present_ > 1))) {
            int32_t second_ = SpiteByte_to_integer(SpiteMemory_Address_read_byte(source_, SpiteInteger_to_long((index_ + 1))));
            int32_t shifted_ = SpiteInteger_shifted_left(second_, 8);
            group_ = SpiteInteger_bits_or(group_, shifted_);
        }
        if (((present_ > 2))) {
            int32_t third_ = SpiteByte_to_integer(SpiteMemory_Address_read_byte(source_, SpiteInteger_to_long(({ int32_t spite_temp_9 = index_; int32_t spite_temp_10 = 2; int32_t spite_temp_11; if (__builtin_expect(__builtin_add_overflow(spite_temp_9, spite_temp_10, &spite_temp_11), 0)) spite_overflowed("index + 2", "an Integer", "+", (int64_t)spite_temp_9, (int64_t)spite_temp_10, spite_site_2()); spite_temp_11; }))));
            group_ = SpiteInteger_bits_or(group_, third_);
        }
        written_ = Base64__write_digit(self, address_, written_, SpiteString___retain(alphabet_), group_, 18);
        written_ = Base64__write_digit(self, address_, written_, SpiteString___retain(alphabet_), group_, 12);
        if (((present_ > 1))) {
            written_ = Base64__write_digit(self, address_, written_, SpiteString___retain(alphabet_), group_, 6);
        }
        else {
            if ((padded_)) {
                SpiteMemory_Address_write_byte(address_, written_, SpiteInteger_to_byte(61));
                written_ = ({ int64_t spite_temp_12 = written_; int64_t spite_temp_13 = SpiteInteger_to_long(1); int64_t spite_temp_14; if (__builtin_expect(__builtin_add_overflow(spite_temp_12, spite_temp_13, &spite_temp_14), 0)) spite_overflowed("written + 1", "a Long", "+", (int64_t)spite_temp_12, (int64_t)spite_temp_13, spite_site_3()); spite_temp_14; });
            }
        }
        if (((present_ > 2))) {
            written_ = Base64__write_digit(self, address_, written_, SpiteString___retain(alphabet_), group_, 0);
        }
        else {
            if ((padded_)) {
                SpiteMemory_Address_write_byte(address_, written_, SpiteInteger_to_byte(61));
                written_ = ({ int64_t spite_temp_15 = written_; int64_t spite_temp_16 = SpiteInteger_to_long(1); int64_t spite_temp_17; if (__builtin_expect(__builtin_add_overflow(spite_temp_15, spite_temp_16, &spite_temp_17), 0)) spite_overflowed("written + 1", "a Long", "+", (int64_t)spite_temp_15, (int64_t)spite_temp_16, spite_site_4()); spite_temp_17; });
            }
        }
        index_ = ({ int32_t spite_temp_18 = index_; int32_t spite_temp_19 = 3; int32_t spite_temp_20; if (__builtin_expect(__builtin_add_overflow(spite_temp_18, spite_temp_19, &spite_temp_20), 0)) spite_overflowed("index + 3", "an Integer", "+", (int64_t)spite_temp_18, (int64_t)spite_temp_19, spite_site_5()); spite_temp_20; });
    }
    SpiteString text_ = SpiteMemory_Address_text(address_, written_);
    if (address_ != (int64_t)(intptr_t)spite_temp_7) Memory_Heap_free(self->heap_, address_);
    SpiteString spite_temp_21 = SpiteString___retain(text_);
    SpiteString___release(text_);
    SpiteString___release(alphabet_);
    List_Byte___release(bytes_);
    return spite_temp_21;
}

int64_t Base64__write_digit(Base64* self, int64_t address_, int64_t at_, SpiteString alphabet_, int32_t group_, int32_t shift_) {
    int32_t value_ = SpiteInteger_bits_and(SpiteInteger_shifted_right(group_, shift_), 63);
    int32_t digit_ = SpiteString_code_at(alphabet_, value_);
    SpiteMemory_Address_write_byte(address_, at_, ({ int32_t spite_temp_22 = digit_; if (__builtin_expect(spite_temp_22 < 0 || spite_temp_22 > UINT8_MAX, 0)) spite_narrowed((int64_t)spite_temp_22, "an Integer", "a Byte", spite_site_6()); (uint8_t)spite_temp_22; }));
    int64_t spite_temp_23 = ({ int64_t spite_temp_24 = at_; int64_t spite_temp_25 = SpiteInteger_to_long(1); int64_t spite_temp_26; if (__builtin_expect(__builtin_add_overflow(spite_temp_24, spite_temp_25, &spite_temp_26), 0)) spite_overflowed("at + 1", "a Long", "+", (int64_t)spite_temp_24, (int64_t)spite_temp_25, spite_site_7()); spite_temp_26; });
    SpiteString___release(alphabet_);
    return spite_temp_23;
}
