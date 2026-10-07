/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void Naive_Naive(Naive* self) {
    int64_t start_ = Clock_elapsed_nanoseconds(self->clock_);
    Vector3__Float spite_slot_1;
    Vector3__Float* position_ = Vector3__Float___make_into(&spite_slot_1, 0.0, 0.0, 0.0);
    Vector3__Float* velocity_ = Vector3__Float___make(1.0, 0.5, 0.25);
    Vector3__Float spite_slot_2;
    Vector3__Float* axis_ = Vector3__Float___make_into(&spite_slot_2, 0.0, 1.0, 0.0);
    float total_ = 0.0;
    int32_t step_ = 0;
    while (((step_ < 5000000))) {
        Vector3__Float spite_slot_3;
        Vector3__Float* moved_ = Vector3__Float_scaled___into(velocity_, 0.001, &spite_slot_3);
        Vector3__Float spite_slot_4;
        Vector3__Float* spite_temp_1 = Vector3__Float_sum___into(position_, Vector3__Float___retain(moved_), &spite_slot_4);
        Vector3__Float___copy_fields(position_, spite_temp_1);
        Vector3__Float spite_slot_5;
        Vector3__Float* turned_ = Vector3__Float_cross___into(velocity_, Vector3__Float___retain(axis_), &spite_slot_5);
        Vector3__Float spite_slot_6;
        Vector3__Float spite_slot_7;
        Vector3__Float* nudged_ = Vector3__Float_sum___into(velocity_, Vector3__Float_scaled___into(turned_, 0.0001, &spite_slot_7), &spite_slot_6);
        Vector3__Float* spite_temp_2 = Vector3__Float_normalized(nudged_);
        Vector3__Float___release(velocity_);
        velocity_ = spite_temp_2;
        total_ = (total_ + Vector3__Float_dot(position_, Vector3__Float___retain(velocity_)));
        step_ = (step_ + 1);
    }
    int64_t checksum_ = ({ float spite_temp_3 = total_; if (__builtin_expect(!((double)spite_temp_3 >= -9223372036854775808.0 && (double)spite_temp_3 < 9223372036854775808.0), 0)) spite_narrowed_decimal((double)spite_temp_3, "a Float", "a Long", spite_site_1()); (int64_t)spite_temp_3; });
    int64_t ended_ = ({ float spite_temp_4 = (Vector3__Float_length(position_) * 1000.0f); if (__builtin_expect(!((double)spite_temp_4 >= -9223372036854775808.0 && (double)spite_temp_4 < 9223372036854775808.0), 0)) spite_narrowed_decimal((double)spite_temp_4, "a Float", "a Long", spite_site_2()); (int64_t)spite_temp_4; });
    int64_t microseconds_ = (({ int64_t spite_temp_5 = Clock_elapsed_nanoseconds(self->clock_); int64_t spite_temp_6 = start_; int64_t spite_temp_7; if (__builtin_expect(__builtin_sub_overflow(spite_temp_5, spite_temp_6, &spite_temp_7), 0)) spite_overflowed("clock.elapsed_nanoseconds() - start", "a Long", "-", (int64_t)spite_temp_5, (int64_t)spite_temp_6, spite_site_3()); spite_temp_7; }) / SpiteInteger_to_long(1000));
    List_Console_Printable spite_framed_1; Console_Printable spite_framed_1_items[4]; int32_t spite_framed_1_count = 0;
    Console_print(self->console_, ({ spite_framed_1_items[0] = spite_tagged_object(0, ((void*)&spite_lit_1_box)); spite_framed_1_items[1] = spite_tagged_SpiteLong(checksum_); spite_framed_1_items[2] = spite_tagged_object(0, ((void*)&spite_lit_2_box)); spite_framed_1_items[3] = spite_tagged_SpiteLong(ended_); spite_framed_1_count = 4; List_Console_Printable___framed(&spite_framed_1, (int64_t)(intptr_t)spite_framed_1_items, 4); }));
    for (int32_t spite_index = 0; spite_index < spite_framed_1_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_1_items[spite_index]); }
    List_Console_Printable spite_framed_2; Console_Printable spite_framed_2_items[1]; int32_t spite_framed_2_count = 0;
    Console_error(self->console_, ({ spite_framed_2_items[0] = spite_tagged_object(0, spite_box_SpiteString(({ char spite_temp_8_digits[24]; SpiteString spite_temp_8 = SPITE_STATIC_STRING(spite_temp_8_digits, spite_long_digits(spite_temp_8_digits, (int64_t)(microseconds_))); SpiteString spite_temp_9[] = {spite_lit_3, spite_temp_8}; SpiteString spite_temp_10 = spite_string_join(2, spite_temp_9); spite_temp_10; }))); spite_framed_2_count = 1; List_Console_Printable___framed(&spite_framed_2, (int64_t)(intptr_t)spite_framed_2_items, 1); }));
    for (int32_t spite_index = 0; spite_index < spite_framed_2_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_2_items[spite_index]); }
    Vector3__Float___release(velocity_);
}

Vector3__Float* Vector3__Float_scaled___into(Vector3__Float* self, float factor_, Vector3__Float* restrict spite_result) {
    Vector3__Float* spite_temp_11 = Vector3__Float___make_into(spite_result, (self->x_ * factor_), (self->y_ * factor_), (self->z_ * factor_));
    return spite_temp_11;
}

Vector3__Float* Vector3__Float_sum___into(Vector3__Float* self, Vector3__Float* other_, Vector3__Float* restrict spite_result) {
    Vector3__Float* spite_temp_12 = Vector3__Float___make_into(spite_result, (self->x_ + (other_)->x_), (self->y_ + (other_)->y_), (self->z_ + (other_)->z_));
    Vector3__Float___release(other_);
    return spite_temp_12;
}

Vector3__Float* Vector3__Float_cross___into(Vector3__Float* self, Vector3__Float* other_, Vector3__Float* restrict spite_result) {
    Vector3__Float* spite_temp_13 = Vector3__Float___make_into(spite_result, ((self->y_ * (other_)->z_) - (self->z_ * (other_)->y_)), ((self->z_ * (other_)->x_) - (self->x_ * (other_)->z_)), ((self->x_ * (other_)->y_) - (self->y_ * (other_)->x_)));
    Vector3__Float___release(other_);
    return spite_temp_13;
}

Vector3__Float* Vector3__Float_normalized(Vector3__Float* self) {
    float squared_ = (((self->x_ * self->x_) + (self->y_ * self->y_)) + (self->z_ * self->z_));
    if (((squared_ == SpiteInteger_to_float(0)))) {
        Vector3__Float* spite_temp_14 = Vector3__Float___make(SpiteInteger_to_float(0), SpiteInteger_to_float(0), SpiteInteger_to_float(0));
        return spite_temp_14;
    }
    {
        float one_ = SpiteInteger_to_float(1);
        float inverse_length_ = (one_ / SpiteFloat_square_root(squared_));
        Vector3__Float* spite_temp_15 = Vector3__Float___make((self->x_ * inverse_length_), (self->y_ * inverse_length_), (self->z_ * inverse_length_));
        return spite_temp_15;
    }
}

float Vector3__Float_dot(Vector3__Float* self, Vector3__Float* other_) {
    float spite_temp_16 = (((self->x_ * (other_)->x_) + (self->y_ * (other_)->y_)) + (self->z_ * (other_)->z_));
    Vector3__Float___release(other_);
    return spite_temp_16;
}
