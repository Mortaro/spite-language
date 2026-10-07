/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Naive_lines(Naive* self, int32_t count_, int32_t round_) {
    int32_t total_ = 0;
    int32_t index_ = 0;
    while (((index_ < count_))) {
        SpiteString line_ = ({ char spite_temp_1_digits[24]; SpiteString spite_temp_1 = SPITE_STATIC_STRING(spite_temp_1_digits, spite_long_digits(spite_temp_1_digits, (int64_t)(index_))); char spite_temp_2_digits[24]; SpiteString spite_temp_2 = SPITE_STATIC_STRING(spite_temp_2_digits, spite_long_digits(spite_temp_2_digits, (int64_t)(round_))); SpiteString spite_temp_3[] = {spite_lit_1, spite_temp_1, spite_lit_2, spite_temp_2, spite_lit_3}; SpiteString spite_temp_4 = spite_string_join(5, spite_temp_3); spite_temp_4; });
        total_ = ({ int32_t spite_temp_5 = total_; int32_t spite_temp_6 = SpiteString_length(line_); int32_t spite_temp_7; if (__builtin_expect(__builtin_add_overflow(spite_temp_5, spite_temp_6, &spite_temp_7), 0)) spite_overflowed("total + line.length()", "an Integer", "+", (int64_t)spite_temp_5, (int64_t)spite_temp_6, spite_site_1()); spite_temp_7; });
        index_ = (index_ + 1);
        SpiteString___release(line_);
    }
    int32_t spite_temp_8 = total_;
    return spite_temp_8;
}

static int64_t spite_long_digits(char* digits, int64_t value) {
    char reversed[24];
    int64_t count = 0;
    uint64_t rest = value < 0 ? (uint64_t)0 - (uint64_t)value : (uint64_t)value;
    do { reversed[count] = (char)('0' + rest % 10); count = count + 1; rest = rest / 10; } while (rest != 0);
    int64_t length = 0;
    if (value < 0) { digits[0] = '-'; length = 1; }
    while (count > 0) { count = count - 1; digits[length] = reversed[count]; length = length + 1; }
    return length;
}
