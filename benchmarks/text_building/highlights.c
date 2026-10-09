/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void Naive_build_texts(Naive* self) {
    SpiteString built_ = spite_lit_1;
    int32_t index_ = 0;
    while (((index_ < 3000000))) {
        SpiteString spite_temp_1 = spite_lit_2;
        built_ = SpiteString_append(built_, spite_temp_1);
        SpiteString___release(spite_temp_1);
        { char spite_temp_2_digits[24]; SpiteString spite_temp_2 = SPITE_STATIC_STRING(spite_temp_2_digits, spite_long_digits(spite_temp_2_digits, (int64_t)(index_))); built_ = SpiteString_append(built_, spite_temp_2); }
        SpiteString spite_temp_3 = spite_lit_3;
        built_ = SpiteString_append(built_, spite_temp_3);
        SpiteString___release(spite_temp_3);
        index_ = (index_ + 1);
    }
    List_String* words_ = List_String___make();
    index_ = 0;
    while (((index_ < 1000000))) {
        List_String_append(words_, ({ char spite_temp_4_digits[24]; SpiteString spite_temp_4 = SPITE_STATIC_STRING(spite_temp_4_digits, spite_long_digits(spite_temp_4_digits, (int64_t)(index_))); SpiteString spite_temp_5[] = {spite_lit_4, spite_temp_4}; SpiteString spite_temp_6 = spite_string_join(2, spite_temp_5); spite_temp_6; }));
        index_ = (index_ + 1);
    }
    SpiteString joined_ = List_String_join(words_, spite_lit_5);
    self->built_length_ = SpiteString_length(built_);
    self->joined_length_ = SpiteString_length(joined_);
    SpiteString___release(joined_);
    List_String___release(words_);
    SpiteString___release(built_);
}

SpiteString List_String_join(List_String* self, SpiteString separator_) {
    List_String* pieces_ = List_String___make();
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        SpiteString piece_ = TypedMemory__String_read_value(self->values_, self->items_, index_);
        total_ = (total_ + SpiteInteger_to_long(SpiteString_length(piece_)));
        List_String_append(pieces_, SpiteString___retain(piece_));
        index_ = (index_ + 1);
        SpiteString___release(piece_);
    }
    if (((self->item_count_ > 1))) {
        total_ = ({ int64_t spite_temp_7 = total_; int64_t spite_temp_8 = SpiteInteger_to_long(({ int32_t spite_temp_9 = SpiteString_length(separator_); int32_t spite_temp_10 = ({ int32_t spite_temp_11 = self->item_count_; int32_t spite_temp_12 = 1; int32_t spite_temp_13; if (__builtin_expect(__builtin_sub_overflow(spite_temp_11, spite_temp_12, &spite_temp_13), 0)) spite_overflowed("item_count - 1", "an Integer", "-", (int64_t)spite_temp_11, (int64_t)spite_temp_12, spite_site_1()); spite_temp_13; }); int32_t spite_temp_14; if (__builtin_expect(__builtin_mul_overflow(spite_temp_9, spite_temp_10, &spite_temp_14), 0)) spite_overflowed("separator.length() * (item_count - 1)", "an Integer", "*", (int64_t)spite_temp_9, (int64_t)spite_temp_10, spite_site_1()); spite_temp_14; })); int64_t spite_temp_15; if (__builtin_expect(__builtin_add_overflow(spite_temp_7, spite_temp_8, &spite_temp_15), 0)) spite_overflowed("total + separator.length() * (item_count - 1)", "a Long", "+", (int64_t)spite_temp_7, (int64_t)spite_temp_8, spite_site_1()); spite_temp_15; });
    }
    int64_t spite_temp_16[32];
    int64_t spite_temp_17 = ({ int64_t spite_temp_18 = total_; int64_t spite_temp_19 = SpiteInteger_to_long(1); int64_t spite_temp_20; if (__builtin_expect(__builtin_add_overflow(spite_temp_18, spite_temp_19, &spite_temp_20), 0)) spite_overflowed("total + 1", "a Long", "+", (int64_t)spite_temp_18, (int64_t)spite_temp_19, spite_site_2()); spite_temp_20; });
    int64_t address_ = spite_temp_17 <= 256 ? (int64_t)(intptr_t)spite_temp_16 : Memory_Heap_allocate(self->heap_, spite_temp_17);
    int64_t position_ = SpiteInteger_to_long(0);
    index_ = 0;
    while (((index_ < List_String_count(pieces_)))) {
        if (((index_ > 0))) {
            position_ = List_String_write_text(self, SpiteString___retain(separator_), address_, position_);
        }
        SpiteString next_piece_ = ({ SpiteString spite_temp_21 = List_String_get_at(pieces_, index_); if (__builtin_expect(!((!SPITE_STRING_IS_NULL(spite_temp_21))), 0)) spite_outside_list("pieces[index]", spite_site_3()); spite_temp_21; });
        position_ = List_String_write_text(self, SpiteString___retain(next_piece_), address_, position_);
        index_ = (index_ + 1);
        SpiteString___release(next_piece_);
    }
    SpiteString joined_ = SpiteMemory_Address_text(address_, total_);
    if (address_ != (int64_t)(intptr_t)spite_temp_16) Memory_Heap_free(self->heap_, address_);
    SpiteString spite_temp_22 = SpiteString___retain(joined_);
    SpiteString___release(joined_);
    List_String___release(pieces_);
    SpiteString___release(separator_);
    return spite_temp_22;
}

int64_t List_String_write_text(List_String* self, SpiteString text_, int64_t address_, int64_t position_) {
    int32_t index_ = 0;
    while (((index_ < SpiteString_length(text_)))) {
        int32_t code_ = SpiteString_code_at(text_, index_);
        SpiteMemory_Address_write_byte(address_, ({ int64_t spite_temp_23 = position_; int64_t spite_temp_24 = SpiteInteger_to_long(index_); int64_t spite_temp_25; if (__builtin_expect(__builtin_add_overflow(spite_temp_23, spite_temp_24, &spite_temp_25), 0)) spite_overflowed("position + index", "a Long", "+", (int64_t)spite_temp_23, (int64_t)spite_temp_24, spite_site_4()); spite_temp_25; }), ({ int32_t spite_temp_26 = code_; if (__builtin_expect(spite_temp_26 < 0 || spite_temp_26 > UINT8_MAX, 0)) spite_narrowed((int64_t)spite_temp_26, "an Integer", "a Byte", spite_site_4()); (uint8_t)spite_temp_26; }));
        index_ = (index_ + 1);
    }
    int64_t spite_temp_27 = ({ int64_t spite_temp_28 = position_; int64_t spite_temp_29 = SpiteInteger_to_long(SpiteString_length(text_)); int64_t spite_temp_30; if (__builtin_expect(__builtin_add_overflow(spite_temp_28, spite_temp_29, &spite_temp_30), 0)) spite_overflowed("position + text.length()", "a Long", "+", (int64_t)spite_temp_28, (int64_t)spite_temp_29, spite_site_5()); spite_temp_30; });
    SpiteString___release(text_);
    return spite_temp_27;
}
