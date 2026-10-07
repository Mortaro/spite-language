/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void Naive_step(Naive* self) {
    int32_t index_ = 0;
    while (((index_ < List_Particle_count(self->particles_)))) {
        Particle* particle_ = ({ List_Particle* spite_temp_1 = self->particles_; int32_t spite_temp_2 = index_; if (__builtin_expect(spite_temp_2 < 0 || spite_temp_2 >= (spite_temp_1)->item_count_, 0)) spite_outside_list("particles[index]", spite_site_1()); ((Particle**)(intptr_t)(spite_temp_1)->items_)[spite_temp_2]; });
        (particle_)->left_ = ({ int32_t spite_temp_3 = (particle_)->left_; int32_t spite_temp_4 = (particle_)->speed_; int32_t spite_temp_5; if (__builtin_expect(__builtin_add_overflow(spite_temp_3, spite_temp_4, &spite_temp_5), 0)) spite_overflowed("particle.left + particle.speed", "an Integer", "+", (int64_t)spite_temp_3, (int64_t)spite_temp_4, spite_site_2()); spite_temp_5; });
        index_ = (index_ + 1);
    }
}
