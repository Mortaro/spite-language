/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

struct Token {
    SpiteHeader header;
    Token_TokenKind kind_;
    int32_t start_;
    int32_t length_;
    int32_t line_;
    int64_t value_;
};

List_Token* Naive_tokenized(Naive* self, SpiteString source_) {
    List_Token* tokens_ = List_Token___make();
    int32_t length_ = SpiteString_length(source_);
    int32_t line_ = 1;
    int32_t position_ = 0;
    while (((position_ < length_))) {
        int32_t code_ = SpiteString_code_at(source_, position_);
        int32_t first_ = position_;
        if (((code_ == 10))) {
            line_ = ({ int32_t spite_temp_1 = line_; int32_t spite_temp_2 = 1; int32_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("line + 1", "an Integer", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
            position_ = (position_ + 1);
        }
        else {
            if (((code_ == 32))) {
                position_ = ({ int32_t spite_temp_4 = position_; int32_t spite_temp_5 = 1; int32_t spite_temp_6; if (__builtin_expect(__builtin_add_overflow(spite_temp_4, spite_temp_5, &spite_temp_6), 0)) spite_overflowed("position + 1", "an Integer", "+", (int64_t)spite_temp_4, (int64_t)spite_temp_5, spite_site_2()); spite_temp_6; });
            }
            else {
                if ((((code_ >= 97))) && (((code_ <= 122)))) {
                    int64_t hash_ = SpiteInteger_to_long(0);
                    while (((((((position_ < length_)) && ((SpiteString_code_at(source_, position_) >= 97)))) && ((SpiteString_code_at(source_, position_) <= 122))))) {
                        hash_ = (({ int64_t spite_temp_7 = ({ int64_t spite_temp_8 = hash_; int64_t spite_temp_9 = SpiteInteger_to_long(31); int64_t spite_temp_10; if (__builtin_expect(__builtin_mul_overflow(spite_temp_8, spite_temp_9, &spite_temp_10), 0)) spite_overflowed("hash * 31", "a Long", "*", (int64_t)spite_temp_8, (int64_t)spite_temp_9, spite_site_3()); spite_temp_10; }); int64_t spite_temp_11 = SpiteInteger_to_long(SpiteString_code_at(source_, position_)); int64_t spite_temp_12; if (__builtin_expect(__builtin_add_overflow(spite_temp_7, spite_temp_11, &spite_temp_12), 0)) spite_overflowed("hash * 31 + source.code_at(position)", "a Long", "+", (int64_t)spite_temp_7, (int64_t)spite_temp_11, spite_site_3()); spite_temp_12; }) % SpiteInteger_to_long(1000000007));
                        position_ = (position_ + 1);
                    }
                    Token* identifier_ = Token___make(Token_TokenKind_identifier, first_, ({ int32_t spite_temp_13 = position_; int32_t spite_temp_14 = first_; int32_t spite_temp_15; if (__builtin_expect(__builtin_sub_overflow(spite_temp_13, spite_temp_14, &spite_temp_15), 0)) spite_overflowed("position - first", "an Integer", "-", (int64_t)spite_temp_13, (int64_t)spite_temp_14, spite_site_4()); spite_temp_15; }), line_, hash_);
                    List_Token_append(tokens_, Token___retain(identifier_));
                    Token___release(identifier_);
                }
                else {
                    if ((((code_ >= 48))) && (((code_ <= 57)))) {
                        int64_t number_ = SpiteInteger_to_long(0);
                        while (((((((position_ < length_)) && ((SpiteString_code_at(source_, position_) >= 48)))) && ((SpiteString_code_at(source_, position_) <= 57))))) {
                            number_ = ({ int64_t spite_temp_16 = ({ int64_t spite_temp_17 = ({ int64_t spite_temp_18 = number_; int64_t spite_temp_19 = SpiteInteger_to_long(10); int64_t spite_temp_20; if (__builtin_expect(__builtin_mul_overflow(spite_temp_18, spite_temp_19, &spite_temp_20), 0)) spite_overflowed("number * 10", "a Long", "*", (int64_t)spite_temp_18, (int64_t)spite_temp_19, spite_site_5()); spite_temp_20; }); int64_t spite_temp_21 = SpiteInteger_to_long(SpiteString_code_at(source_, position_)); int64_t spite_temp_22; if (__builtin_expect(__builtin_add_overflow(spite_temp_17, spite_temp_21, &spite_temp_22), 0)) spite_overflowed("number * 10 + source.code_at(position)", "a Long", "+", (int64_t)spite_temp_17, (int64_t)spite_temp_21, spite_site_5()); spite_temp_22; }); int64_t spite_temp_23 = SpiteInteger_to_long(48); int64_t spite_temp_24; if (__builtin_expect(__builtin_sub_overflow(spite_temp_16, spite_temp_23, &spite_temp_24), 0)) spite_overflowed("number * 10 + source.code_at(position) - 48", "a Long", "-", (int64_t)spite_temp_16, (int64_t)spite_temp_23, spite_site_5()); spite_temp_24; });
                            position_ = (position_ + 1);
                        }
                        Token* digits_ = Token___make(Token_TokenKind_number, first_, ({ int32_t spite_temp_25 = position_; int32_t spite_temp_26 = first_; int32_t spite_temp_27; if (__builtin_expect(__builtin_sub_overflow(spite_temp_25, spite_temp_26, &spite_temp_27), 0)) spite_overflowed("position - first", "an Integer", "-", (int64_t)spite_temp_25, (int64_t)spite_temp_26, spite_site_6()); spite_temp_27; }), line_, number_);
                        List_Token_append(tokens_, Token___retain(digits_));
                        Token___release(digits_);
                    }
                    else {
                        position_ = ({ int32_t spite_temp_28 = position_; int32_t spite_temp_29 = 1; int32_t spite_temp_30; if (__builtin_expect(__builtin_add_overflow(spite_temp_28, spite_temp_29, &spite_temp_30), 0)) spite_overflowed("position + 1", "an Integer", "+", (int64_t)spite_temp_28, (int64_t)spite_temp_29, spite_site_7()); spite_temp_30; });
                        Token* symbol_ = Token___make(Token_TokenKind_symbol, first_, 1, line_, SpiteInteger_to_long(code_));
                        List_Token_append(tokens_, Token___retain(symbol_));
                        Token___release(symbol_);
                    }
                }
            }
        }
    }
    List_Token* spite_temp_31 = List_Token___retain(tokens_);
    List_Token___release(tokens_);
    SpiteString___release(source_);
    return spite_temp_31;
}

int32_t List_Token_count_identifier(List_Token* self) {
    int32_t counted_ = 0;
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        Token* item_ = ((Token**)(intptr_t)self->items_)[index_];
        if ((((item_)->kind_ == Token_TokenKind_identifier))) {
            counted_ = ({ int32_t spite_temp_32 = counted_; int32_t spite_temp_33 = 1; int32_t spite_temp_34; if (__builtin_expect(__builtin_add_overflow(spite_temp_32, spite_temp_33, &spite_temp_34), 0)) spite_overflowed("counted + 1", "an Integer", "+", (int64_t)spite_temp_32, (int64_t)spite_temp_33, spite_site_8()); spite_temp_34; });
        }
        index_ = (index_ + 1);
    }
    int32_t spite_temp_35 = counted_;
    return spite_temp_35;
}

int64_t Naive_total_of_numbers___held_0(Naive* self, List_Token* tokens_) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t index_ = 0;
    while (((index_ < spite_folded_List_Token_count(tokens_)))) {
        Token* token_ = ({ List_Token* spite_temp_36 = tokens_; int32_t spite_temp_37 = index_; if (__builtin_expect(spite_temp_37 < 0 || spite_temp_37 >= (spite_temp_36)->item_count_, 0)) spite_outside_list("tokens[index]", spite_site_9()); ((Token**)(intptr_t)(spite_temp_36)->items_)[spite_temp_37]; });
        if ((((token_)->kind_ == Token_TokenKind_number))) {
            total_ = ({ int64_t spite_temp_38 = total_; int64_t spite_temp_39 = (token_)->value_; int64_t spite_temp_40; if (__builtin_expect(__builtin_add_overflow(spite_temp_38, spite_temp_39, &spite_temp_40), 0)) spite_overflowed("total + token.value", "a Long", "+", (int64_t)spite_temp_38, (int64_t)spite_temp_39, spite_site_10()); spite_temp_40; });
        }
        index_ = (index_ + 1);
    }
    int64_t spite_temp_41 = total_;
    return spite_temp_41;
}

int64_t List_Token_sum_checksum(List_Token* self) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        Token* item_ = ((Token**)(intptr_t)self->items_)[index_];
        total_ = ({ int64_t spite_temp_42 = total_; int64_t spite_temp_43 = Token_checksum(item_); int64_t spite_temp_44; if (__builtin_expect(__builtin_add_overflow(spite_temp_42, spite_temp_43, &spite_temp_44), 0)) spite_overflowed("total + item.attributes[member]", "a Long", "+", (int64_t)spite_temp_42, (int64_t)spite_temp_43, spite_site_11()); spite_temp_44; });
        index_ = (index_ + 1);
    }
    int64_t spite_temp_45 = total_;
    return spite_temp_45;
}
