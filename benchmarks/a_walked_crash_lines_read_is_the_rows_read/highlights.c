/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void Runner__Mover_Moving_run___held_0(Runner__Mover_Moving* self, Mover* system_, int32_t entity_count_) {
    int32_t entity_ = 0;
    while (((entity_ < entity_count_))) {
        List_Integer_clear(self->found_);
        self->missing_ = false;
        Runner__Mover_Moving_find_attributes(self, entity_);
        if (((!(self->missing_)))) {
            Nullable_Integer spite_temp_1 = List_Integer_get_at(self->found_, 1);
            if (!(spite_temp_1.has_value)) {
                spite_failed_1(entity_count_, entity_, self);
            }
            Position* spite_temp_2 = Vector__Position_get_at((spite_singleton_Column__Position())->values_, spite_temp_1.value);
            if (!(((spite_temp_2) != 0))) {
                spite_failed_2(entity_count_, entity_, self);
            }
            Nullable_Integer spite_temp_3 = List_Integer_get_at(self->found_, 2);
            if (!(spite_temp_3.has_value)) {
                spite_failed_3(entity_count_, entity_, self);
            }
            Velocity* spite_temp_4 = Vector__Velocity_get_at((spite_singleton_Column__Velocity())->values_, spite_temp_3.value);
            if (!(((spite_temp_4) != 0))) {
                spite_failed_4(entity_count_, entity_, self);
            }
            Entity spite_temp_5 = { { 1, 111 } };
            Entity___init(&spite_temp_5);
            Entity_Entity(&spite_temp_5, entity_);
            Object_entity_Entity_position_Position_velocity_Velocity spite_temp_6 = { { 1, 180 } };
            spite_temp_6.entity_ = (&spite_temp_5);
            spite_temp_6.position_ = spite_temp_2;
            spite_temp_6.velocity_ = spite_temp_4;
            Mover_Moving row_ = ((Mover_Moving)((&spite_temp_6)));
            
            
            Mover_update_each___lent_0(system_, row_);
        }
        entity_ = (entity_ + 1);
    }
}
