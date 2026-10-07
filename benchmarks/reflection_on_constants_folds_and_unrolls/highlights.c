/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Naive_total_of___held_0(Naive* self, Stats* stats_) {
    self->running_ = 0;
    (Naive_add_attribute_for_strength___held_0(self, stats_), Naive_add_attribute_for_agility___held_0(self, stats_), Naive_add_attribute_for_wisdom___held_0(self, stats_), Naive_add_attribute_for_stamina___held_0(self, stats_));
    int32_t spite_temp_1 = self->running_;
    return spite_temp_1;
}

void Naive_add_attribute_for_strength___held_0(Naive* self, Stats* attribute_instance_) {
    {
        self->running_ = ({ int32_t spite_temp_2 = self->running_; int32_t spite_temp_3 = (attribute_instance_)->strength_; int32_t spite_temp_4; if (__builtin_expect(__builtin_add_overflow(spite_temp_2, spite_temp_3, &spite_temp_4), 0)) spite_overflowed("running + attribute.value", "an Integer", "+", (int64_t)spite_temp_2, (int64_t)spite_temp_3, spite_site_1()); spite_temp_4; });
    }
}

void Naive_add_attribute_for_agility___held_0(Naive* self, Stats* attribute_instance_) {
    {
        self->running_ = ({ int32_t spite_temp_5 = self->running_; int32_t spite_temp_6 = (attribute_instance_)->agility_; int32_t spite_temp_7; if (__builtin_expect(__builtin_add_overflow(spite_temp_5, spite_temp_6, &spite_temp_7), 0)) spite_overflowed("running + attribute.value", "an Integer", "+", (int64_t)spite_temp_5, (int64_t)spite_temp_6, spite_site_2()); spite_temp_7; });
    }
}
