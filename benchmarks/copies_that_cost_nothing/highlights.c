/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int64_t Naive_all_trials___held_0(Naive* self, Point* origin_, int32_t trials_) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t trial_ = 0;
    while (((trial_ < trials_))) {
        Point spite_slot_1;
        Point* moved_ = Point___copy_into(origin_, &spite_slot_1);
        Point_move_by(moved_, ((trial_ % 7) - 3), ((trial_ % 5) - 2));
        total_ = ({ int64_t spite_temp_1 = total_; int64_t spite_temp_2 = SpiteInteger_to_long(Point_distance_from_origin(moved_)); int64_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("total + moved.distance_from_origin()", "a Long", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
        trial_ = (trial_ + 1);
    }
    int64_t spite_temp_4 = total_;
    return spite_temp_4;
}

static Point* Point___copy_into(Point* self, Point* spite_result) {
    Point___copy_fields(Point___framed(spite_result), self);
    return spite_result;
}
