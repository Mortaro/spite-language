/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

static SpiteString spite_symbol_1 = { (int64_t)0x000068746c616568ULL, (int64_t)0x0900000000000000ULL };
static SpiteString spite_symbol_2 = SPITE_STATIC_STRING("experience_points_gained", 24);
static SpiteString spite_symbol_3 = { (int64_t)0x797469746e656469ULL, (int64_t)0x0700000000000000ULL };

void Naive_add_name_length_for_health___held_0(Naive* self, Monster* attribute_instance_) {
    SpiteString name_ = SpiteString___retain(spite_symbol_1);
    self->letters_ = ({ int32_t spite_temp_1 = self->letters_; int32_t spite_temp_2 = SpiteString_length(name_); int32_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("letters + name.length()", "an Integer", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
    SpiteString___release(name_);
}

void Naive_add_name_length_for_experience_points_gained___held_0(Naive* self, Monster* attribute_instance_) {
    SpiteString name_ = SpiteString___retain(spite_symbol_2);
    self->letters_ = ({ int32_t spite_temp_4 = self->letters_; int32_t spite_temp_5 = SpiteString_length(name_); int32_t spite_temp_6; if (__builtin_expect(__builtin_add_overflow(spite_temp_4, spite_temp_5, &spite_temp_6), 0)) spite_overflowed("letters + name.length()", "an Integer", "+", (int64_t)spite_temp_4, (int64_t)spite_temp_5, spite_site_2()); spite_temp_6; });
    SpiteString___release(name_);
}
