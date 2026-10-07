/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Naive_meetings___held_0(Naive* self, List_String* names_) {
    int32_t total_ = 0;
    int32_t index_ = 0;
    while (((({ int32_t spite_temp_1 = index_; int32_t spite_temp_2 = 1; int32_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("index + 1", "an Integer", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; }) < List_String_count(names_)))) {
        SpiteString line_ = ({ SpiteString spite_temp_4 = ({ SpiteString spite_temp_5 = List_String_get_at(names_, index_); if (__builtin_expect(!((!SPITE_STRING_IS_NULL(spite_temp_5))), 0)) spite_outside_list("names[index]", spite_site_2()); spite_temp_5; }); SpiteString spite_temp_6 = ({ SpiteString spite_temp_7 = List_String_get_at(names_, ({ int32_t spite_temp_8 = index_; int32_t spite_temp_9 = 1; int32_t spite_temp_10; if (__builtin_expect(__builtin_add_overflow(spite_temp_8, spite_temp_9, &spite_temp_10), 0)) spite_overflowed("index + 1", "an Integer", "+", (int64_t)spite_temp_8, (int64_t)spite_temp_9, spite_site_2()); spite_temp_10; })); if (__builtin_expect(!((!SPITE_STRING_IS_NULL(spite_temp_7))), 0)) spite_outside_list("names[index + 1]", spite_site_2()); spite_temp_7; }); SpiteString spite_temp_11[] = {spite_temp_4, spite_lit_1, spite_temp_6, spite_lit_2}; SpiteString spite_temp_12 = spite_string_join(4, spite_temp_11); SpiteString___release(spite_temp_4); SpiteString___release(spite_temp_6); spite_temp_12; });
        total_ = ({ int32_t spite_temp_13 = total_; int32_t spite_temp_14 = SpiteString_length(line_); int32_t spite_temp_15; if (__builtin_expect(__builtin_add_overflow(spite_temp_13, spite_temp_14, &spite_temp_15), 0)) spite_overflowed("total + line.length()", "an Integer", "+", (int64_t)spite_temp_13, (int64_t)spite_temp_14, spite_site_3()); spite_temp_15; });
        index_ = ({ int32_t spite_temp_16 = index_; int32_t spite_temp_17 = 1; int32_t spite_temp_18; if (__builtin_expect(__builtin_add_overflow(spite_temp_16, spite_temp_17, &spite_temp_18), 0)) spite_overflowed("index + 1", "an Integer", "+", (int64_t)spite_temp_16, (int64_t)spite_temp_17, spite_site_4()); spite_temp_18; });
        SpiteString___release(line_);
    }
    int32_t spite_temp_19 = total_;
    return spite_temp_19;
}

SpiteString spite_string_join(int32_t count, const SpiteString* pieces) {
    int64_t total = 0;
    for (int32_t index = 0; index < count; index++) total += spite_string_length(pieces[index]);
    SpiteString made = { 0, 0 };
    SpiteStringBlock* block = 0;
    char* at = (char*)&made;
    if (total > SPITE_STRING_INLINE) { block = spite_string_block(total); at = block->bytes; }
    for (int32_t index = 0; index < count; index++) {
        int64_t length = spite_string_length(pieces[index]);
        if (length > 0) memcpy(at, spite_string_bytes(&pieces[index]), (size_t)length);
        at += length;
    }
    if (block != 0) return spite_string_held(block, total);
    ((char*)&made)[15] = (char)(SPITE_STRING_INLINE - total);
    return made;
}
