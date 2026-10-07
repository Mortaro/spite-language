/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

double Naive_measure_all(Naive* self, int32_t steps_) {
    double total_ = SpiteFloat_to_double(0.0);
    Crate* crate_ = Crate___make(1.0, 2.0, 3.0);
    Ball* ball_ = Ball___make(1.0);
    int32_t step_ = 0;
    while (((step_ < steps_))) {
        int32_t count_ = (step_ % 10);
        total_ = (total_ + Naive_doubled___for_0_Integer(self, count_));
        float share_ = SpiteInteger_to_float((step_ % 4));
        total_ = (total_ + Naive_doubled___for_0_Float(self, share_));
        (crate_)->width_ = SpiteInteger_to_float((step_ % 7));
        total_ = (total_ + Naive_doubled___for_0_Crate___held_0(self, crate_));
        (ball_)->radius_ = SpiteInteger_to_float((step_ % 5));
        total_ = (total_ + Naive_doubled___for_0_Ball___held_0(self, ball_));
        step_ = (step_ + 1);
    }
    double spite_temp_1 = total_;
    Ball___release(ball_);
    Crate___release(crate_);
    return spite_temp_1;
}

double Naive_doubled___for_0_Integer(Naive* self, int32_t value_) {
    double measure_ = SpiteInteger_to_double(value_);
    double spite_temp_2 = (measure_ + measure_);
    return spite_temp_2;
}

double Naive_doubled___for_0_Float(Naive* self, float value_) {
    double measure_ = SpiteFloat_to_double(value_);
    double spite_temp_3 = (measure_ + measure_);
    return spite_temp_3;
}

double Naive_doubled___for_0_Crate___held_0(Naive* self, Crate* value_) {
    double measure_ = Crate_to_double(value_);
    double spite_temp_4 = (measure_ + measure_);
    return spite_temp_4;
}

double Naive_doubled___for_0_Ball___held_0(Naive* self, Ball* value_) {
    double measure_ = Ball_to_double(value_);
    double spite_temp_5 = (measure_ + measure_);
    return spite_temp_5;
}
