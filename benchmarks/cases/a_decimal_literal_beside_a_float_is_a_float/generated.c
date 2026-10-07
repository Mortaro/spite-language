/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

double Naive_rounds___held_0(Naive* self, List_Float* values_) {
    double total_ = SpiteFloat_to_double(0.0);
    int32_t round_ = 0;
    while (((round_ < 4000))) {
        float shift_ = SpiteInteger_to_float((round_ % 8));
        float sum_ = Naive_scaled_sum___held_0(self, values_, shift_);
        total_ = (total_ + SpiteFloat_to_double(sum_));
        round_ = (round_ + 1);
    }
    double spite_temp_1 = total_;
    return spite_temp_1;
}

float Naive_scaled_sum___held_0(Naive* self, List_Float* values_, float shift_) {
    float total_ = 0.0;
    int32_t index_ = 0;
    int32_t spite_temp_2 = spite_folded_List_Float_count(values_);
    float* spite_temp_3 = (float*)(intptr_t)(values_)->items_;
    while (index_ < spite_temp_2) {
        #if defined(__clang__)
        #pragma clang fp contract(fast) reassociate(on)
        #endif
        total_ = ((total_ + ((spite_temp_3[index_] + shift_) * 1.5f)) + 0.25f);
        index_ = (index_ + 1);
    }
    float spite_temp_4 = total_;
    return spite_temp_4;
}
