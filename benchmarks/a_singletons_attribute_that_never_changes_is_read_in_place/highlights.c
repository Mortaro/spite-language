/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int64_t Reader_read_round(Reader* self, int32_t rows_, int32_t round_) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t row_ = 0;
    while (((row_ < rows_))) {
        int32_t place_ = ({ int32_t spite_temp_1 = ({ int32_t spite_temp_2 = ({ int32_t spite_temp_3 = row_; int32_t spite_temp_4 = 7; int32_t spite_temp_5; if (__builtin_expect(__builtin_mul_overflow(spite_temp_3, spite_temp_4, &spite_temp_5), 0)) spite_overflowed("row * 7", "an Integer", "*", (int64_t)spite_temp_3, (int64_t)spite_temp_4, spite_site_1()); spite_temp_5; }); int32_t spite_temp_6 = round_; int32_t spite_temp_7; if (__builtin_expect(__builtin_add_overflow(spite_temp_2, spite_temp_6, &spite_temp_7), 0)) spite_overflowed("row * 7 + round", "an Integer", "+", (int64_t)spite_temp_2, (int64_t)spite_temp_6, spite_site_1()); spite_temp_7; }); int32_t spite_temp_8 = rows_; if (spite_temp_8 == 0) spite_divided_by_zero("(row * 7 + round) % rows", spite_site_1()); (int32_t)(spite_temp_8 == -1 ? (int32_t)0 : spite_temp_1 % spite_temp_8); });
        if (!(({ List_Box* spite_temp_9 = (self->shelf_)->boxes_; int32_t spite_temp_10 = place_; (spite_temp_10 >= 0 && spite_temp_10 < (spite_temp_9)->item_count_) && ((((Box**)(intptr_t)(spite_temp_9)->items_)[spite_temp_10]) != 0); }))) {
            spite_failed_1(place_, self, rows_, round_, total_, row_);
        }
        total_ = (total_ + SpiteInteger_to_long((({ List_Box* spite_temp_11 = (self->shelf_)->boxes_; int32_t spite_temp_12 = place_; if (__builtin_expect(spite_temp_12 < 0 || spite_temp_12 >= (spite_temp_11)->item_count_, 0)) spite_outside_list("shelf.boxes[place]", spite_site_2()); ((Box**)(intptr_t)(spite_temp_11)->items_)[spite_temp_12]; }))->weight_));
        row_ = (row_ + 1);
    }
    int64_t spite_temp_13 = total_;
    return spite_temp_13;
}

int32_t Shelf_count(Shelf* self) {
    if (spite_skipped_depth < 16 && __atomic_load_n(&spite_tasks_in_flight, __ATOMIC_ACQUIRE) == 0) {
        int32_t spite_skip = spite_skipped_depth;
        spite_skipped[spite_skip] = (void*)&Shelf___guard;
        spite_skipped_taken[spite_skip] = 0;
        spite_skipped_depth = spite_skip + 1;
        SPITE_GUARDS_COUNT(1);
        int32_t spite_unshared = Shelf_count___unguarded(self);
        spite_skipped_depth = spite_skip;
        if (spite_skipped_taken[spite_skip]) spite_guard_leave(&Shelf___guard);
        SPITE_GUARDS_COUNT(-1);
        return spite_unshared;
    }
    int64_t* spite_reading = spite_read_enter(&Shelf___guard, Shelf___readers);
    SPITE_GUARDS_COUNT(1);
    int32_t spite_guarded = Shelf_count___unguarded(self);
    spite_read_leave(spite_reading);
    SPITE_GUARDS_COUNT(-1);
    return spite_guarded;
}
