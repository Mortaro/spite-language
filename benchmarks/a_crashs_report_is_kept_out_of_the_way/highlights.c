/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int64_t Naive_round_total___held_0_1(Naive* self, List_Integer* values_, List_Integer* picks_, int32_t round_) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t index_ = 0;
    int32_t spite_temp_1 = List_Integer_count(picks_);
    int32_t* spite_temp_2 = (int32_t*)(intptr_t)(picks_)->items_;
    while (index_ < spite_temp_1) {
        #if defined(__clang__)
        #pragma clang fp contract(fast) reassociate(on)
        #endif
        int32_t pick_ = ({ int32_t spite_temp_3 = spite_temp_2[index_]; int32_t spite_temp_4 = round_; int32_t spite_temp_5; if (__builtin_expect(__builtin_add_overflow(spite_temp_3, spite_temp_4, &spite_temp_5), 0)) spite_overflowed("picks[index] + round", "an Integer", "+", (int64_t)spite_temp_3, (int64_t)spite_temp_4, spite_site_1()); spite_temp_5; });
        if (!(((List_Integer_get_at(values_, pick_)).has_value))) {
            spite_failed_1(pick_, values_, round_, total_, index_);
        }
        total_ = (total_ + SpiteInteger_to_long((List_Integer_get_at(values_, pick_)).value));
        index_ = (index_ + 1);
    }
    int64_t spite_temp_6 = total_;
    return spite_temp_6;
}

static SPITE_CRASH_REPORT void spite_failed_1(int32_t pick_, List_Integer* values_, int32_t round_, int64_t total_, int32_t index_) {
    spite_crash_begin();
    fflush(stdout);
    fputs(spite_site_2(), stderr);
    {
        fputs("\tvalues[pick] is missing: index ", stderr);
        { SpiteString spite_temp_7 = SpiteInteger_to_string(pick_); fwrite(spite_string_bytes(&spite_temp_7), 1, (size_t)spite_string_length(spite_temp_7), stderr); SpiteString___release(spite_temp_7); }
        fputs(", count ", stderr);
        { SpiteString spite_temp_8 = SpiteInteger_to_string(((values_)->item_count_)); fwrite(spite_string_bytes(&spite_temp_8), 1, (size_t)spite_string_length(spite_temp_8), stderr); SpiteString___release(spite_temp_8); }
    }
    fputs("\tround=", stderr);
    { SpiteString spite_temp_9 = SpiteInteger_to_string(round_); spite_crash_text(spite_string_bytes(&spite_temp_9), spite_string_length(spite_temp_9)); SpiteString___release(spite_temp_9); }
    fputs("\ttotal=", stderr);
    { SpiteString spite_temp_10 = SpiteLong_to_string(total_); spite_crash_text(spite_string_bytes(&spite_temp_10), spite_string_length(spite_temp_10)); SpiteString___release(spite_temp_10); }
    fputs("\tindex=", stderr);
    { SpiteString spite_temp_11 = SpiteInteger_to_string(index_); spite_crash_text(spite_string_bytes(&spite_temp_11), spite_string_length(spite_temp_11)); SpiteString___release(spite_temp_11); }
    fputs("\n", stderr);
    spite_report_assert_trace();
    exit(1);
}
