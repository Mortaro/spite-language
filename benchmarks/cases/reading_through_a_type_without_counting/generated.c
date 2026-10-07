/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

Position* Mover_Moving___peek_position(Mover_Moving self) {
    if (((SpiteHeader*)(self))->class_id == 171) return ((Object_position_Position_velocity_Velocity*)self)->position_;
    return 0;
}

Velocity* Mover_Moving___peek_velocity(Mover_Moving self) {
    if (((SpiteHeader*)(self))->class_id == 171) return ((Object_position_Position_velocity_Velocity*)self)->velocity_;
    return 0;
}

void Mover_advance___lent_0(Mover* self, Mover_Moving moving_, int32_t steps_) {
    Position spite_temp_1_scratch;
    Position* spite_temp_1 = Mover_Moving___peek_position(moving_);
    if (spite_temp_1 == 0) spite_temp_1 = &spite_temp_1_scratch;
    (spite_temp_1)->left_ = ({ int32_t spite_temp_2 = ({ Position* spite_temp_3 = Mover_Moving___peek_position(moving_); spite_temp_3 != 0 ? (spite_temp_3)->left_ : (0); }); int32_t spite_temp_4 = ({ int32_t spite_temp_5 = ({ Velocity* spite_temp_6 = Mover_Moving___peek_velocity(moving_); spite_temp_6 != 0 ? (spite_temp_6)->across_ : (0); }); int32_t spite_temp_7 = steps_; int32_t spite_temp_8; if (__builtin_expect(__builtin_mul_overflow(spite_temp_5, spite_temp_7, &spite_temp_8), 0)) spite_overflowed("moving.velocity.across * steps", "an Integer", "*", (int64_t)spite_temp_5, (int64_t)spite_temp_7, spite_site_1()); spite_temp_8; }); int32_t spite_temp_9; if (__builtin_expect(__builtin_add_overflow(spite_temp_2, spite_temp_4, &spite_temp_9), 0)) spite_overflowed("moving.position.left + moving.velocity.across * steps", "an Integer", "+", (int64_t)spite_temp_2, (int64_t)spite_temp_4, spite_site_1()); spite_temp_9; });
    Position spite_temp_10_scratch;
    Position* spite_temp_10 = Mover_Moving___peek_position(moving_);
    if (spite_temp_10 == 0) spite_temp_10 = &spite_temp_10_scratch;
    (spite_temp_10)->top_ = ({ int32_t spite_temp_11 = ({ Position* spite_temp_12 = Mover_Moving___peek_position(moving_); spite_temp_12 != 0 ? (spite_temp_12)->top_ : (0); }); int32_t spite_temp_13 = ({ int32_t spite_temp_14 = ({ Velocity* spite_temp_15 = Mover_Moving___peek_velocity(moving_); spite_temp_15 != 0 ? (spite_temp_15)->down_ : (0); }); int32_t spite_temp_16 = steps_; int32_t spite_temp_17; if (__builtin_expect(__builtin_mul_overflow(spite_temp_14, spite_temp_16, &spite_temp_17), 0)) spite_overflowed("moving.velocity.down * steps", "an Integer", "*", (int64_t)spite_temp_14, (int64_t)spite_temp_16, spite_site_2()); spite_temp_17; }); int32_t spite_temp_18; if (__builtin_expect(__builtin_add_overflow(spite_temp_11, spite_temp_13, &spite_temp_18), 0)) spite_overflowed("moving.position.top + moving.velocity.down * steps", "an Integer", "+", (int64_t)spite_temp_11, (int64_t)spite_temp_13, spite_site_2()); spite_temp_18; });
}
