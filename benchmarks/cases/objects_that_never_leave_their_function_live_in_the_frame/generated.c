/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

struct Offset {
    SpiteHeader header;
    float x_;
    float y_;
    float z_;
};

Offset* Naive_simulate___into(Naive* self, int32_t steps_, Offset* restrict spite_result) {
    Offset* position_ = Offset___make_into(spite_result, 0.0, 0.0, 0.0);
    Offset spite_slot_1;
    Offset* velocity_ = Offset___make_into(&spite_slot_1, 1.0, 0.5, 0.25);
    Offset spite_slot_2;
    Offset* gravity_ = Offset___make_into(&spite_slot_2, 0.0, (-(0.5)), 0.0);
    int32_t step_ = 0;
    while (((step_ < steps_))) {
        Offset spite_slot_3;
        Offset* pulled_ = Offset_scaled___into(gravity_, 9.9999999999999995e-07, &spite_slot_3);
        Offset spite_slot_4;
        Offset* spite_temp_1 = Offset_sum___into(velocity_, Offset___retain(pulled_), &spite_slot_4);
        Offset___copy_fields(velocity_, spite_temp_1);
        Offset spite_slot_5;
        Offset* moved_ = Offset_scaled___into(velocity_, 0.001, &spite_slot_5);
        Offset spite_slot_6;
        Offset* spite_temp_2 = Offset_sum___into(position_, Offset___retain(moved_), &spite_slot_6);
        Offset___copy_fields(position_, spite_temp_2);
        step_ = (step_ + 1);
    }
    return spite_result;
}

Offset* Offset_scaled___into(Offset* self, float factor_, Offset* restrict spite_result) {
    Offset* spite_temp_3 = Offset___make_into(spite_result, (self->x_ * factor_), (self->y_ * factor_), (self->z_ * factor_));
    return spite_temp_3;
}

Offset* Offset_sum___into(Offset* self, Offset* other_, Offset* restrict spite_result) {
    Offset* spite_temp_4 = Offset___make_into(spite_result, (self->x_ + (other_)->x_), (self->y_ + (other_)->y_), (self->z_ + (other_)->z_));
    Offset___release(other_);
    return spite_temp_4;
}
