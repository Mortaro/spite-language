/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void Naive_scale_list___held_0_1(Naive* self, List_Float* from_, List_Float* into_, float offset_) {
    int32_t index_ = 0;
    int32_t spite_temp_1 = List_Float_count(from_);
    float* spite_temp_2 = (float*)(intptr_t)(from_)->items_;
    int32_t spite_temp_3 = (into_)->item_count_;
    float* spite_temp_4 = (float*)(intptr_t)(into_)->items_;
    if (spite_temp_1 <= spite_temp_3) {
        while (index_ < spite_temp_1) {
            #if defined(__clang__)
            #pragma clang fp contract(fast) reassociate(on)
            #endif
            spite_temp_4[index_] = ((spite_temp_2[index_] * 1.5f) + offset_);
            index_ = (index_ + 1);
        }
    } else {
        while (index_ < spite_temp_1) {
            #if defined(__clang__)
            #pragma clang fp contract(fast) reassociate(on)
            #endif
            List_Float_set_at(into_, index_, ((spite_temp_2[index_] * 1.5f) + offset_));
            index_ = (index_ + 1);
        }
    }
}

float Naive_add_up___held_0(Naive* self, List_Float* values_) {
    float total_ = 0.0;
    int32_t index_ = 0;
    int32_t spite_temp_5 = List_Float_count(values_);
    float* spite_temp_6 = (float*)(intptr_t)(values_)->items_;
    while (index_ < spite_temp_5) {
        #if defined(__clang__)
        #pragma clang fp contract(fast) reassociate(on)
        #endif
        total_ = (total_ + spite_temp_6[index_]);
        index_ = (index_ + 1);
    }
    float spite_temp_7 = total_;
    return spite_temp_7;
}
