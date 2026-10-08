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
        self->total_ = ({ int64_t spite_temp_1 = self->total_; int64_t spite_temp_2 = SpiteInteger_to_long(((index_ % 7) * 2)); int64_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("total + index % 7 * 2", "a Long", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
        index_ = (index_ + 1);
    }
}

void Odds_count(Odds* self) {
    int32_t index_ = 0;
    while (((index_ < 40000000))) {
        self->total_ = ({ int64_t spite_temp_4 = ({ int64_t spite_temp_5 = self->total_; int64_t spite_temp_6 = SpiteInteger_to_long(((index_ % 5) * 2)); int64_t spite_temp_7; if (__builtin_expect(__builtin_add_overflow(spite_temp_5, spite_temp_6, &spite_temp_7), 0)) spite_overflowed("total + index % 5 * 2", "a Long", "+", (int64_t)spite_temp_5, (int64_t)spite_temp_6, spite_site_2()); spite_temp_7; }); int64_t spite_temp_8 = SpiteInteger_to_long(1); int64_t spite_temp_9; if (__builtin_expect(__builtin_add_overflow(spite_temp_4, spite_temp_8, &spite_temp_9), 0)) spite_overflowed("total + index % 5 * 2 + 1", "a Long", "+", (int64_t)spite_temp_4, (int64_t)spite_temp_8, spite_site_2()); spite_temp_9; });
        index_ = (index_ + 1);
    }
}
