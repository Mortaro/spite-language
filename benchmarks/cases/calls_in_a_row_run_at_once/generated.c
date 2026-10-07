/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void Naive_count_both(Naive* self) {
    
    
    
    
    
    
    {
        
        Parallel__Nothing* spite_overlap_1_0_ = Parallel__Nothing___make(spite_function_value_Evens_count(self->evens_));
        
        
        
        
        
        
        Odds_count(self->odds_);
        
        
        Parallel__Nothing__join(spite_overlap_1_0_);
        Parallel__Nothing___release(spite_overlap_1_0_);
        
        
        
        
        
    }
    
}

void Evens_count(Evens* self) {
    int32_t index_ = 0;
    while (((index_ < 40000000))) {
        self->total_ = ({ int64_t spite_temp_1 = self->total_; int64_t spite_temp_2 = SpiteInteger_to_long(({ int32_t spite_temp_3 = (index_ % 7); int32_t spite_temp_4 = 2; int32_t spite_temp_5; if (__builtin_expect(__builtin_mul_overflow(spite_temp_3, spite_temp_4, &spite_temp_5), 0)) spite_overflowed("index % 7 * 2", "an Integer", "*", (int64_t)spite_temp_3, (int64_t)spite_temp_4, spite_site_1()); spite_temp_5; })); int64_t spite_temp_6; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_6), 0)) spite_overflowed("total + index % 7 * 2", "a Long", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_6; });
        index_ = (index_ + 1);
    }
}

void Odds_count(Odds* self) {
    int32_t index_ = 0;
    while (((index_ < 40000000))) {
        self->total_ = ({ int64_t spite_temp_7 = ({ int64_t spite_temp_8 = self->total_; int64_t spite_temp_9 = SpiteInteger_to_long(({ int32_t spite_temp_10 = (index_ % 5); int32_t spite_temp_11 = 2; int32_t spite_temp_12; if (__builtin_expect(__builtin_mul_overflow(spite_temp_10, spite_temp_11, &spite_temp_12), 0)) spite_overflowed("index % 5 * 2", "an Integer", "*", (int64_t)spite_temp_10, (int64_t)spite_temp_11, spite_site_2()); spite_temp_12; })); int64_t spite_temp_13; if (__builtin_expect(__builtin_add_overflow(spite_temp_8, spite_temp_9, &spite_temp_13), 0)) spite_overflowed("total + index % 5 * 2", "a Long", "+", (int64_t)spite_temp_8, (int64_t)spite_temp_9, spite_site_2()); spite_temp_13; }); int64_t spite_temp_14 = SpiteInteger_to_long(1); int64_t spite_temp_15; if (__builtin_expect(__builtin_add_overflow(spite_temp_7, spite_temp_14, &spite_temp_15), 0)) spite_overflowed("total + index % 5 * 2 + 1", "a Long", "+", (int64_t)spite_temp_7, (int64_t)spite_temp_14, spite_site_2()); spite_temp_15; });
        index_ = (index_ + 1);
    }
}
