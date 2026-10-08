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

void Naive_Naive(Naive* self) {
    int64_t start_ = Clock_elapsed_nanoseconds(self->clock_);
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
    int64_t height_checksum_ = ({ float spite_temp_2 = height_; if (__builtin_expect(!((double)spite_temp_2 >= -9223372036854775808.0 && (double)spite_temp_2 < 9223372036854775808.0), 0)) spite_narrowed_decimal((double)spite_temp_2, "a Float", "a Long", spite_site_2()); (int64_t)spite_temp_2; });
    int64_t spread_checksum_ = ({ float spite_temp_3 = spread_; if (__builtin_expect(!((double)spite_temp_3 >= -9223372036854775808.0 && (double)spite_temp_3 < 9223372036854775808.0), 0)) spite_narrowed_decimal((double)spite_temp_3, "a Float", "a Long", spite_site_3()); (int64_t)spite_temp_3; });
    int64_t microseconds_ = (({ int64_t spite_temp_4 = Clock_elapsed_nanoseconds(self->clock_); int64_t spite_temp_5 = start_; int64_t spite_temp_6; if (__builtin_expect(__builtin_sub_overflow(spite_temp_4, spite_temp_5, &spite_temp_6), 0)) spite_overflowed("clock.elapsed_nanoseconds() - start", "a Long", "-", (int64_t)spite_temp_4, (int64_t)spite_temp_5, spite_site_4()); spite_temp_6; }) / SpiteInteger_to_long(1000));
    List_Console_Printable spite_framed_1; Console_Printable spite_framed_1_items[4]; int32_t spite_framed_1_count = 0;
    Console_print(self->console_, ({ spite_framed_1_items[0] = spite_tagged_object(0, ((void*)&spite_lit_1_box)); spite_framed_1_items[1] = spite_tagged_SpiteLong(height_checksum_); spite_framed_1_items[2] = spite_tagged_object(0, ((void*)&spite_lit_2_box)); spite_framed_1_items[3] = spite_tagged_SpiteLong(spread_checksum_); spite_framed_1_count = 4; List_Console_Printable___framed(&spite_framed_1, (int64_t)(intptr_t)spite_framed_1_items, 4); }));
    for (int32_t spite_index = 0; spite_index < spite_framed_1_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_1_items[spite_index]); }
    List_Console_Printable spite_framed_2; Console_Printable spite_framed_2_items[1]; int32_t spite_framed_2_count = 0;
    Console_error(self->console_, ({ spite_framed_2_items[0] = spite_tagged_object(0, spite_box_SpiteString(({ char spite_temp_7_digits[24]; SpiteString spite_temp_7 = SPITE_STATIC_STRING(spite_temp_7_digits, spite_long_digits(spite_temp_7_digits, (int64_t)(microseconds_))); SpiteString spite_temp_8[] = {spite_lit_3, spite_temp_7}; SpiteString spite_temp_9 = spite_string_join(2, spite_temp_8); spite_temp_9; }))); spite_framed_2_count = 1; List_Console_Printable___framed(&spite_framed_2, (int64_t)(intptr_t)spite_framed_2_items, 1); }));
    for (int32_t spite_index = 0; spite_index < spite_framed_2_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_2_items[spite_index]); }
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
