/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Stream_run(Stream* self) {
    Matcher spite_slot_1;
    Matcher* matcher_ = Matcher___make_into(&spite_slot_1, 3);
    List_Integer* found_ = List_Integer___make();
    List_Integer_append(found_, 0);
    List_Integer_append(found_, 0);
    List_Integer_append(found_, 0);
    int32_t spite_temp_1 = Stream_run_positions___held_0_1(self, matcher_, found_);
    List_Integer___release(found_);
    Matcher___unframe(matcher_);
    return spite_temp_1;
}

int32_t Stream_run_positions___held_0_1(Stream* self, Matcher* matcher_, List_Integer* rows_) {
    int32_t total_ = 0;
    int32_t entity_ = 0;
    while (((entity_ < self->entity_count_))) {
        if ((Matcher_match_into___held_1(matcher_, entity_, rows_))) {
            if (!(((List_Integer_get_at(rows_, 2)).has_value))) {
                spite_failed_1(rows_, total_, entity_, self);
            }
            total_ = ({ int32_t spite_temp_2 = total_; int32_t spite_temp_3 = (List_Integer_get_at(rows_, 2)).value; int32_t spite_temp_4; if (__builtin_expect(__builtin_add_overflow(spite_temp_2, spite_temp_3, &spite_temp_4), 0)) spite_overflowed("total + rows[2]", "an Integer", "+", (int64_t)spite_temp_2, (int64_t)spite_temp_3, spite_site_1()); spite_temp_4; });
        }
        entity_ = (entity_ + 1);
    }
    int32_t spite_temp_5 = total_;
    return spite_temp_5;
}

bool Matcher_match_into___held_1(Matcher* self, int32_t entity_, List_Integer* found_) {
    int32_t index_ = 0;
    bool all_ = true;
    while ((((all_) && ((index_ < List_Integer_count(self->headers_)))))) {
        all_ = Matcher_find_row___held_2(self, index_, entity_, found_);
        index_ = (index_ + 1);
    }
    bool spite_temp_6 = all_;
    return spite_temp_6;
}

bool Matcher_find_row___held_2(Matcher* self, int32_t index_, int32_t entity_, List_Integer* found_) {
    if (!(((List_Integer_get_at(self->headers_, index_)).has_value))) {
        spite_failed_2(index_, self, entity_);
    }
    int32_t place_ = ({ int32_t spite_temp_7 = entity_; int32_t spite_temp_8 = (List_Integer_get_at(self->headers_, index_)).value; if (spite_temp_8 == 0) spite_divided_by_zero("entity % headers[index]", spite_site_2()); (int32_t)(spite_temp_8 == -1 ? (int32_t)0 : spite_temp_7 % spite_temp_8); });
    List_Integer_set_at(found_, index_, place_);
    bool spite_temp_9 = (place_ >= 0);
    return spite_temp_9;
}
