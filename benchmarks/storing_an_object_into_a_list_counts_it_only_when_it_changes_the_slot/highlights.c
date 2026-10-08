/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Rack_place___held_1(Rack* self, int32_t at_, Crate* crate_) {
    if (!(({ List_Crate* spite_temp_1 = self->crates_; int32_t spite_temp_2 = at_; (spite_temp_2 >= 0 && spite_temp_2 < (spite_temp_1)->item_count_) && ((((Crate**)(intptr_t)(spite_temp_1)->items_)[spite_temp_2]) != 0); }))) {
        spite_failed_1(at_, self);
    }
    int32_t before_ = (({ List_Crate* spite_temp_3 = self->crates_; int32_t spite_temp_4 = at_; if (__builtin_expect(spite_temp_4 < 0 || spite_temp_4 >= (spite_temp_3)->item_count_, 0)) spite_outside_list("crates[at]", spite_site_1()); ((Crate**)(intptr_t)(spite_temp_3)->items_)[spite_temp_4]; }))->weight_;
    { List_Crate* spite_temp_5 = self->crates_; int32_t spite_temp_6 = at_; Crate* spite_temp_7 = crate_; if (__builtin_expect(spite_temp_6 < 0 || spite_temp_6 >= (spite_temp_5)->item_count_, 0)) spite_outside_list("crates[at]", spite_site_2()); Crate* spite_temp_8 = ((Crate**)(intptr_t)(spite_temp_5)->items_)[spite_temp_6]; if (spite_temp_8 != spite_temp_7) { ((Crate**)(intptr_t)(spite_temp_5)->items_)[spite_temp_6] = Crate___retain(spite_temp_7); Crate___release(spite_temp_8); } }
    if (((before_ == (crate_)->weight_))) {
        int32_t spite_temp_9 = 0;
        return spite_temp_9;
    }
    int32_t spite_temp_10 = 1;
    return spite_temp_10;
}
