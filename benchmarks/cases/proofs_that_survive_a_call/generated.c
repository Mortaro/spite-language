/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Hunter_hunt(Hunter* self) {
    if (!(((self->target_) != 0))) {
        spite_failed_1(self);
    }
    int32_t rounds_ = 0;
    while ((((self->target_)->health_ > 0))) {
        Hunter_record_hit(self);
        Monster_hurt(self->target_, 3);
        rounds_ = ({ int32_t spite_temp_1 = rounds_; int32_t spite_temp_2 = 1; int32_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("rounds + 1", "an Integer", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
    }
    int32_t spite_temp_4 = rounds_;
    return spite_temp_4;
}

void Hunter_record_hit(Hunter* self) {
    self->hits_ = ({ int32_t spite_temp_5 = self->hits_; int32_t spite_temp_6 = 1; int32_t spite_temp_7; if (__builtin_expect(__builtin_add_overflow(spite_temp_5, spite_temp_6, &spite_temp_7), 0)) spite_overflowed("hits + 1", "an Integer", "+", (int64_t)spite_temp_5, (int64_t)spite_temp_6, spite_site_2()); spite_temp_7; });
}

void Monster_hurt(Monster* self, int32_t amount_) {
    self->health_ = ({ int32_t spite_temp_8 = self->health_; int32_t spite_temp_9 = amount_; int32_t spite_temp_10; if (__builtin_expect(__builtin_sub_overflow(spite_temp_8, spite_temp_9, &spite_temp_10), 0)) spite_overflowed("health - amount", "an Integer", "-", (int64_t)spite_temp_8, (int64_t)spite_temp_9, spite_site_3()); spite_temp_10; });
}
