/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

static inline void Mover___release(Mover* self) {
    if (self == 0) return;
    if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
    Mover___free(self);
}

void Mover___free(Mover* self) {
    #ifdef SPITE_TRACKS_Mover
    spite_untrack_Mover(self);
    #endif
    #ifdef SPITE_WEAK_Mover
    spite_weak_object_freed(self);
    #endif
    Mover___pool_give(self);
}

void TypedMemory__Mover_release_value(TypedMemory__Mover* self, int64_t address_, int32_t index_) {
    Mover___release(((Mover**)(intptr_t)address_)[index_]);
}

int32_t Naive_keep_every_other___held_0_1(Naive* self, List_Mover* movers_, List_Mover* every_other_) {
    int32_t total_ = 0;
    int32_t position_ = 0;
    while (((position_ < spite_folded_List_Mover_count(movers_)))) {
        Mover* mover_ = ({ List_Mover* spite_temp_1 = movers_; int32_t spite_temp_2 = position_; if (__builtin_expect(spite_temp_2 < 0 || spite_temp_2 >= (spite_temp_1)->item_count_, 0)) spite_outside_list("movers[position]", spite_site_1()); ((Mover**)(intptr_t)(spite_temp_1)->items_)[spite_temp_2]; });
        total_ = ({ int32_t spite_temp_3 = total_; int32_t spite_temp_4 = (mover_)->speed_; int32_t spite_temp_5; if (__builtin_expect(__builtin_add_overflow(spite_temp_3, spite_temp_4, &spite_temp_5), 0)) spite_overflowed("total + mover.speed", "an Integer", "+", (int64_t)spite_temp_3, (int64_t)spite_temp_4, spite_site_2()); spite_temp_5; });
        List_Mover_append(every_other_, Mover___retain(mover_));
        position_ = ({ int32_t spite_temp_6 = position_; int32_t spite_temp_7 = 2; int32_t spite_temp_8; if (__builtin_expect(__builtin_add_overflow(spite_temp_6, spite_temp_7, &spite_temp_8), 0)) spite_overflowed("position + 2", "an Integer", "+", (int64_t)spite_temp_6, (int64_t)spite_temp_7, spite_site_3()); spite_temp_8; });
    }
    int32_t spite_temp_9 = total_;
    return spite_temp_9;
}
