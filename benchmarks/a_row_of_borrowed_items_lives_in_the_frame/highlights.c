/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

struct Object_position_Position_velocity_Velocity {
    SpiteHeader header;
    Position* position_;
    Velocity* velocity_;
};

void Naive_tick_once(Naive* self) {
    int32_t entity_ = 0;
    while (((entity_ < Vector__Position_count(self->positions_)))) {
        if (!(((Vector__Velocity_get_at(self->velocities_, entity_)) != 0))) {
            spite_failed_1(entity_, self);
        }
        if (!(((Vector__Health_get_at(self->healths_, entity_)) != 0))) {
            spite_failed_2(entity_, self);
        }
        if (!(((Vector__Regeneration_get_at(self->regenerations_, entity_)) != 0))) {
            spite_failed_3(entity_, self);
        }
        Object_position_Position_velocity_Velocity spite_temp_1 = { { 1, 182 } };
        spite_temp_1.position_ = ({ Position* spite_temp_2 = Vector__Position_get_at(self->positions_, entity_); if (__builtin_expect(!(((spite_temp_2) != 0)), 0)) spite_outside_list("positions[entity]", spite_site_1()); spite_temp_2; });
        spite_temp_1.velocity_ = Vector__Velocity_get_at(self->velocities_, entity_);
        Object_position_Position_velocity_Velocity* moving_ = (&spite_temp_1);
        Mover_update_each___lent_0(self->mover_, ((Mover_Moving)(moving_)));
        Object_health_Health_regeneration_Regeneration spite_temp_3 = { { 1, 183 } };
        spite_temp_3.health_ = Vector__Health_get_at(self->healths_, entity_);
        spite_temp_3.regeneration_ = Vector__Regeneration_get_at(self->regenerations_, entity_);
        Object_health_Health_regeneration_Regeneration* mending_ = (&spite_temp_3);
        Healer_update_each___lent_0(self->healer_, ((Healer_Mending)(mending_)));
        entity_ = (entity_ + 1);
    }
}

void Mover_update_each___lent_0(Mover* self, Mover_Moving moving_) {
    Position spite_temp_4_scratch;
    Position* spite_temp_4 = Mover_Moving___peek_position(moving_);
    if (spite_temp_4 == 0) spite_temp_4 = &spite_temp_4_scratch;
    (spite_temp_4)->left_ = ({ int32_t spite_temp_5 = ({ Position* spite_temp_6 = Mover_Moving___peek_position(moving_); spite_temp_6 != 0 ? (spite_temp_6)->left_ : (0); }); int32_t spite_temp_7 = ({ Velocity* spite_temp_8 = Mover_Moving___peek_velocity(moving_); spite_temp_8 != 0 ? (spite_temp_8)->across_ : (0); }); int32_t spite_temp_9; if (__builtin_expect(__builtin_add_overflow(spite_temp_5, spite_temp_7, &spite_temp_9), 0)) spite_overflowed("moving.position.left + moving.velocity.across", "an Integer", "+", (int64_t)spite_temp_5, (int64_t)spite_temp_7, spite_site_2()); spite_temp_9; });
    Position spite_temp_10_scratch;
    Position* spite_temp_10 = Mover_Moving___peek_position(moving_);
    if (spite_temp_10 == 0) spite_temp_10 = &spite_temp_10_scratch;
    (spite_temp_10)->top_ = ({ int32_t spite_temp_11 = ({ Position* spite_temp_12 = Mover_Moving___peek_position(moving_); spite_temp_12 != 0 ? (spite_temp_12)->top_ : (0); }); int32_t spite_temp_13 = ({ Velocity* spite_temp_14 = Mover_Moving___peek_velocity(moving_); spite_temp_14 != 0 ? (spite_temp_14)->down_ : (0); }); int32_t spite_temp_15; if (__builtin_expect(__builtin_add_overflow(spite_temp_11, spite_temp_13, &spite_temp_15), 0)) spite_overflowed("moving.position.top + moving.velocity.down", "an Integer", "+", (int64_t)spite_temp_11, (int64_t)spite_temp_13, spite_site_3()); spite_temp_15; });
}
