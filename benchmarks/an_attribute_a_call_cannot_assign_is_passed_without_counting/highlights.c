/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Meter_measure(Meter* self, int32_t at_) {
    int32_t spite_temp_1 = ({ int32_t spite_temp_2 = Meter_reading_at___held_0(self, self->readings_, (at_ % 16)); int32_t spite_temp_3 = Meter_offset_of___held_0(self, (self->settings_)->limits_); int32_t spite_temp_4; if (__builtin_expect(__builtin_add_overflow(spite_temp_2, spite_temp_3, &spite_temp_4), 0)) spite_overflowed("reading_at(readings, at % 16) + offset_of(settings.limits)", "an Integer", "+", (int64_t)spite_temp_2, (int64_t)spite_temp_3, spite_site_1()); spite_temp_4; });
    return spite_temp_1;
}

int32_t Meter_reading_at___held_0(Meter* self, List_Integer* values_, int32_t at_) {
    if (!(((List_Integer_get_at(values_, at_)).has_value))) {
        spite_failed_1(at_, values_);
    }
    int32_t spite_temp_5 = (List_Integer_get_at(values_, at_)).value;
    return spite_temp_5;
}

int32_t Meter_offset_of___held_0(Meter* self, Limits* limits_) {
    int32_t spite_temp_6 = (limits_)->offset_;
    return spite_temp_6;
}
