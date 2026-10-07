/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

Vector3__Float* Naive_vector_pass(Naive* self) {
    Vector3__Float spite_slot_1;
    Vector3__Float* position_ = Vector3__Float___make_into(&spite_slot_1, 0.0, 0.0, 0.0);
    Vector3__Float spite_slot_2;
    Vector3__Float* velocity_ = Vector3__Float___make_into(&spite_slot_2, 1.0, 0.5, 0.25);
    int32_t step_ = 0;
    while (((step_ < 1000000))) {
        Vector3__Float spite_slot_3;
        Vector3__Float* moved_ = Vector3__Float_scaled___into(velocity_, 0.001, &spite_slot_3);
        Vector3__Float spite_slot_4;
        Vector3__Float* spite_temp_1 = Vector3__Float_sum___into(position_, Vector3__Float___retain(moved_), &spite_slot_4);
        Vector3__Float___copy_fields(position_, spite_temp_1);
        step_ = (step_ + 1);
    }
    Vector3__Float* spite_temp_2 = Vector3__Float___allocate();
    Vector3__Float___copy_fields(spite_temp_2, position_);
    return spite_temp_2;
}

float Naive_matrix_pass(Naive* self) {
    Quaternion__Float* turn_ = Quaternion__Float___make();
    Vector3__Float spite_slot_5;
    Vector3__Float* axis_ = Vector3__Float___make_into(&spite_slot_5, 0.0, 1.0, 0.0);
    Quaternion__Float_set_axis_angle(turn_, Vector3__Float___retain(axis_), 0.001);
    Matrix4__Float* step_matrix_ = Quaternion__Float_to_matrix(turn_);
    Matrix4__Float* accumulated_ = Matrix4__Float___make();
    int32_t step_ = 0;
    while (((step_ < 200000))) {
        Matrix4__Float* spite_temp_3 = Matrix4__Float_multiply(accumulated_, Matrix4__Float___retain(step_matrix_));
        Matrix4__Float___release(accumulated_);
        accumulated_ = spite_temp_3;
        step_ = (step_ + 1);
    }
    float spite_temp_4 = (((accumulated_)->column_0_row_0_ + (accumulated_)->column_1_row_1_) + (accumulated_)->column_2_row_2_);
    Matrix4__Float___release(accumulated_);
    Matrix4__Float___release(step_matrix_);
    Quaternion__Float___release(turn_);
    return spite_temp_4;
}

float Naive_transform_pass(Naive* self) {
    Matrix4__Float* model_ = Matrix4__Float___make();
    Vector3__Float* place_ = Vector3__Float___make(1.0, 2.0, 3.0);
    Matrix4__Float_set_translation(model_, Vector3__Float___retain(place_));
    Vector3__Float spite_slot_6;
    Vector3__Float* point_ = Vector3__Float___make_into(&spite_slot_6, 0.5, 0.5, 0.5);
    float total_ = 0.0;
    int32_t step_ = 0;
    while (((step_ < 1000000))) {
        Vector3__Float spite_slot_7;
        Vector3__Float* moved_ = Matrix4__Float_transform_point___into(model_, Vector3__Float___retain(point_), &spite_slot_7);
        total_ = (total_ + (moved_)->x_);
        step_ = (step_ + 1);
    }
    float spite_temp_5 = total_;
    Vector3__Float___release(place_);
    Matrix4__Float___release(model_);
    return spite_temp_5;
}

Matrix4__Float* Matrix4__Float_multiply(Matrix4__Float* self, Matrix4__Float* other_) {
    Matrix4__Float* product_ = Matrix4__Float___make();
    (product_)->column_0_row_0_ = ((((self->column_0_row_0_ * (other_)->column_0_row_0_) + (self->column_1_row_0_ * (other_)->column_0_row_1_)) + (self->column_2_row_0_ * (other_)->column_0_row_2_)) + (self->column_3_row_0_ * (other_)->column_0_row_3_));
    (product_)->column_0_row_1_ = ((((self->column_0_row_1_ * (other_)->column_0_row_0_) + (self->column_1_row_1_ * (other_)->column_0_row_1_)) + (self->column_2_row_1_ * (other_)->column_0_row_2_)) + (self->column_3_row_1_ * (other_)->column_0_row_3_));
    (product_)->column_0_row_2_ = ((((self->column_0_row_2_ * (other_)->column_0_row_0_) + (self->column_1_row_2_ * (other_)->column_0_row_1_)) + (self->column_2_row_2_ * (other_)->column_0_row_2_)) + (self->column_3_row_2_ * (other_)->column_0_row_3_));
    (product_)->column_0_row_3_ = ((((self->column_0_row_3_ * (other_)->column_0_row_0_) + (self->column_1_row_3_ * (other_)->column_0_row_1_)) + (self->column_2_row_3_ * (other_)->column_0_row_2_)) + (self->column_3_row_3_ * (other_)->column_0_row_3_));
    (product_)->column_1_row_0_ = ((((self->column_0_row_0_ * (other_)->column_1_row_0_) + (self->column_1_row_0_ * (other_)->column_1_row_1_)) + (self->column_2_row_0_ * (other_)->column_1_row_2_)) + (self->column_3_row_0_ * (other_)->column_1_row_3_));
    (product_)->column_1_row_1_ = ((((self->column_0_row_1_ * (other_)->column_1_row_0_) + (self->column_1_row_1_ * (other_)->column_1_row_1_)) + (self->column_2_row_1_ * (other_)->column_1_row_2_)) + (self->column_3_row_1_ * (other_)->column_1_row_3_));
    (product_)->column_1_row_2_ = ((((self->column_0_row_2_ * (other_)->column_1_row_0_) + (self->column_1_row_2_ * (other_)->column_1_row_1_)) + (self->column_2_row_2_ * (other_)->column_1_row_2_)) + (self->column_3_row_2_ * (other_)->column_1_row_3_));
    (product_)->column_1_row_3_ = ((((self->column_0_row_3_ * (other_)->column_1_row_0_) + (self->column_1_row_3_ * (other_)->column_1_row_1_)) + (self->column_2_row_3_ * (other_)->column_1_row_2_)) + (self->column_3_row_3_ * (other_)->column_1_row_3_));
    (product_)->column_2_row_0_ = ((((self->column_0_row_0_ * (other_)->column_2_row_0_) + (self->column_1_row_0_ * (other_)->column_2_row_1_)) + (self->column_2_row_0_ * (other_)->column_2_row_2_)) + (self->column_3_row_0_ * (other_)->column_2_row_3_));
    (product_)->column_2_row_1_ = ((((self->column_0_row_1_ * (other_)->column_2_row_0_) + (self->column_1_row_1_ * (other_)->column_2_row_1_)) + (self->column_2_row_1_ * (other_)->column_2_row_2_)) + (self->column_3_row_1_ * (other_)->column_2_row_3_));
    (product_)->column_2_row_2_ = ((((self->column_0_row_2_ * (other_)->column_2_row_0_) + (self->column_1_row_2_ * (other_)->column_2_row_1_)) + (self->column_2_row_2_ * (other_)->column_2_row_2_)) + (self->column_3_row_2_ * (other_)->column_2_row_3_));
    (product_)->column_2_row_3_ = ((((self->column_0_row_3_ * (other_)->column_2_row_0_) + (self->column_1_row_3_ * (other_)->column_2_row_1_)) + (self->column_2_row_3_ * (other_)->column_2_row_2_)) + (self->column_3_row_3_ * (other_)->column_2_row_3_));
    (product_)->column_3_row_0_ = ((((self->column_0_row_0_ * (other_)->column_3_row_0_) + (self->column_1_row_0_ * (other_)->column_3_row_1_)) + (self->column_2_row_0_ * (other_)->column_3_row_2_)) + (self->column_3_row_0_ * (other_)->column_3_row_3_));
    (product_)->column_3_row_1_ = ((((self->column_0_row_1_ * (other_)->column_3_row_0_) + (self->column_1_row_1_ * (other_)->column_3_row_1_)) + (self->column_2_row_1_ * (other_)->column_3_row_2_)) + (self->column_3_row_1_ * (other_)->column_3_row_3_));
    (product_)->column_3_row_2_ = ((((self->column_0_row_2_ * (other_)->column_3_row_0_) + (self->column_1_row_2_ * (other_)->column_3_row_1_)) + (self->column_2_row_2_ * (other_)->column_3_row_2_)) + (self->column_3_row_2_ * (other_)->column_3_row_3_));
    (product_)->column_3_row_3_ = ((((self->column_0_row_3_ * (other_)->column_3_row_0_) + (self->column_1_row_3_ * (other_)->column_3_row_1_)) + (self->column_2_row_3_ * (other_)->column_3_row_2_)) + (self->column_3_row_3_ * (other_)->column_3_row_3_));
    Matrix4__Float* spite_temp_6 = Matrix4__Float___retain(product_);
    Matrix4__Float___release(product_);
    Matrix4__Float___release(other_);
    return spite_temp_6;
}

Vector3__Float* Matrix4__Float_transform_point___into(Matrix4__Float* self, Vector3__Float* point_, Vector3__Float* restrict spite_result) {
    float x_part_ = ((((self->column_0_row_0_ * (point_)->x_) + (self->column_1_row_0_ * (point_)->y_)) + (self->column_2_row_0_ * (point_)->z_)) + self->column_3_row_0_);
    float y_part_ = ((((self->column_0_row_1_ * (point_)->x_) + (self->column_1_row_1_ * (point_)->y_)) + (self->column_2_row_1_ * (point_)->z_)) + self->column_3_row_1_);
    float z_part_ = ((((self->column_0_row_2_ * (point_)->x_) + (self->column_1_row_2_ * (point_)->y_)) + (self->column_2_row_2_ * (point_)->z_)) + self->column_3_row_2_);
    Vector3__Float* spite_temp_7 = Vector3__Float___make_into(spite_result, x_part_, y_part_, z_part_);
    Vector3__Float___release(point_);
    return spite_temp_7;
}
