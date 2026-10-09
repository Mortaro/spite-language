/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

struct Particle {
    SpiteHeader header;
    float position_x_;
    float position_y_;
    float position_z_;
    float velocity_x_;
    float velocity_y_;
    float velocity_z_;
};

void Naive_simulate(Naive* self) {
    Vector__Particle* particles_ = Vector__Particle___make();
    int32_t index_ = 0;
    while (((index_ < 100000))) {
        Particle spite_slot_1;
        Particle* particle_ = Particle___make_into(&spite_slot_1, index_);
        Vector__Particle_append(particles_, Particle___retain(particle_));
        index_ = (index_ + 1);
    }
    int32_t tick_ = 0;
    while (((tick_ < 300))) {
        
        
        
        
        
        
        
        Vector__Particle_each_step(particles_);
        
        
        
        tick_ = (tick_ + 1);
    }
    float height_ = 0.0;
    float spread_ = 0.0;
    index_ = 0;
    while (((index_ < spite_folded_Vector__Particle_count(particles_)))) {
        Particle* particle_ = ({ Particle* spite_temp_1 = Vector__Particle_get_at(particles_, index_); if (__builtin_expect(!(((spite_temp_1) != 0)), 0)) spite_outside_list("particles[index]", spite_site_1()); spite_temp_1; });
        height_ = (height_ + (particle_)->position_y_);
        spread_ = ((spread_ + (particle_)->position_x_) + (particle_)->position_z_);
        index_ = (index_ + 1);
    }
    self->height_checksum_ = ({ float spite_temp_2 = height_; if (__builtin_expect(!((double)spite_temp_2 >= -9223372036854775808.0 && (double)spite_temp_2 < 9223372036854775808.0), 0)) spite_narrowed_decimal((double)spite_temp_2, "a Float", "a Long", spite_site_2()); (int64_t)spite_temp_2; });
    self->spread_checksum_ = ({ float spite_temp_3 = spread_; if (__builtin_expect(!((double)spite_temp_3 >= -9223372036854775808.0 && (double)spite_temp_3 < 9223372036854775808.0), 0)) spite_narrowed_decimal((double)spite_temp_3, "a Float", "a Long", spite_site_3()); (int64_t)spite_temp_3; });
    Vector__Particle___release(particles_);
}

void Vector__Particle_each_step(Vector__Particle* self) {
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        Particle* item_ = InlineMemory__Particle_item_at(self->values_, self->items_, index_);
        Particle_step(item_);
        index_ = (index_ + 1);
    }
}

void Particle_step(Particle* self) {
    self->velocity_y_ = (self->velocity_y_ - (9.8 * 0.016));
    self->position_x_ = (self->position_x_ + (self->velocity_x_ * 0.016f));
    self->position_y_ = (self->position_y_ + (self->velocity_y_ * 0.016f));
    self->position_z_ = (self->position_z_ + (self->velocity_z_ * 0.016f));
    if (((self->position_y_ < 0.0f))) {
        self->position_y_ = (-(self->position_y_));
        self->velocity_y_ = ((-(self->velocity_y_)) * 0.8f);
    }
}
