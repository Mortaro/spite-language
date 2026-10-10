/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void Naive_render_all(Naive* self) {
    
    { int64_t spite_row_1_marks[64];
        
        if (({ List_Naive_Voice* spite_row_list = self->voices_; static const unsigned char spite_row_table[3][3] = {{0, 1, 1}, {1, 0, 1}, {1, 1, 0}}; static const unsigned char spite_row_heavy[3] = {1, 1, 1}; int32_t spite_row_n = spite_row_list->item_count_; int32_t spite_row_seen[64]; int32_t spite_row_heavies = 0; bool spite_row_ok = spite_row_n >= 2 && spite_row_n <= 3; for (int32_t spite_row_i = 0; spite_row_ok && spite_row_i < spite_row_n; spite_row_i++) { int32_t spite_row_k = -1; switch (((SpiteTagged*)(intptr_t)spite_row_list->items_)[spite_row_i].tag) { case 113: spite_row_k = 0; break; case 114: spite_row_k = 1; break; case 112: spite_row_k = 2; break; default: break; } spite_row_ok = spite_row_k >= 0; for (int32_t spite_row_j = 0; spite_row_ok && spite_row_j < spite_row_i; spite_row_j++) spite_row_ok = spite_row_table[spite_row_seen[spite_row_j]][spite_row_k] != 0; if (spite_row_ok) { spite_row_seen[spite_row_i] = spite_row_k; spite_row_1_marks[spite_row_i] = spite_row_heavy[spite_row_k]; spite_row_heavies += spite_row_heavy[spite_row_k]; } } spite_row_ok && spite_row_heavies >= 2; })) {
            SPITE_ROWS_ENTER(); List_Naive_Voice_spite_row_render(self->voices_, ((int64_t)(intptr_t)spite_row_1_marks)); SPITE_ROWS_LEAVE();
        } else {
            
            List_Naive_Voice_each_render(self->voices_);
            
        }}
    
}

void List_Naive_Voice_spite_row_render(List_Naive_Voice* self, int64_t marks_) {
    ThreadPool* pool_ = spite_singleton_ThreadPool();
    ThreadPool_run_marked(pool_, spite_function_value_List_Naive_Voice_spite_row_render_piece(self), self->item_count_, marks_);
    ThreadPool___release(pool_);
}

void List_Naive_Voice_spite_row_render_piece(List_Naive_Voice* self, int32_t first_, int32_t end_) {
    int32_t index_ = first_;
    while (((index_ < end_))) {
        Naive_Voice item_ = ((Naive_Voice*)(intptr_t)self->items_)[index_];
        Naive_Voice___call_render(item_);
        index_ = (index_ + 1);
    }
}

void ThreadPool_run_marked(ThreadPool* self, Spite_Function* piece_, int32_t count_, int64_t marks_) {
    int32_t last_ = ({ int32_t spite_temp_1 = count_; int32_t spite_temp_2 = 1; int32_t spite_temp_3; if (__builtin_expect(__builtin_sub_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("count - 1", "an Integer", "-", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
    while (((((last_ >= 0)) && ((SpiteMemory_Address_read_long(marks_, SpiteInteger_to_long(({ int32_t spite_temp_4 = last_; int32_t spite_temp_5 = 8; int32_t spite_temp_6; if (__builtin_expect(__builtin_mul_overflow(spite_temp_4, spite_temp_5, &spite_temp_6), 0)) spite_overflowed("last * 8", "an Integer", "*", (int64_t)spite_temp_4, (int64_t)spite_temp_5, spite_site_2()); spite_temp_6; }))) == SpiteInteger_to_long(0)))))) {
        last_ = ({ int32_t spite_temp_7 = last_; int32_t spite_temp_8 = 1; int32_t spite_temp_9; if (__builtin_expect(__builtin_sub_overflow(spite_temp_7, spite_temp_8, &spite_temp_9), 0)) spite_overflowed("last - 1", "an Integer", "-", (int64_t)spite_temp_7, (int64_t)spite_temp_8, spite_site_3()); spite_temp_9; });
    }
    int64_t block_ = Memory_Heap_allocate(self->heap_, SpiteInteger_to_long(({ int32_t spite_temp_10 = count_; int32_t spite_temp_11 = 8; int32_t spite_temp_12; if (__builtin_expect(__builtin_mul_overflow(spite_temp_10, spite_temp_11, &spite_temp_12), 0)) spite_overflowed("count * 8", "an Integer", "*", (int64_t)spite_temp_10, (int64_t)spite_temp_11, spite_site_4()); spite_temp_12; })));
    int32_t index_ = 0;
    while (((index_ < count_))) {
        if ((((index_ != last_))) && (((SpiteMemory_Address_read_long(marks_, SpiteInteger_to_long(({ int32_t spite_temp_13 = index_; int32_t spite_temp_14 = 8; int32_t spite_temp_15; if (__builtin_expect(__builtin_mul_overflow(spite_temp_13, spite_temp_14, &spite_temp_15), 0)) spite_overflowed("index * 8", "an Integer", "*", (int64_t)spite_temp_13, (int64_t)spite_temp_14, spite_site_5()); spite_temp_15; }))) != SpiteInteger_to_long(0))))) {
            ThreadPool_submit(self, Spite_Function___retain(piece_), index_, (index_ + 1), (block_ + ((int64_t)(({ int32_t spite_temp_16 = index_; int32_t spite_temp_17 = 8; int32_t spite_temp_18; if (__builtin_expect(__builtin_mul_overflow(spite_temp_16, spite_temp_17, &spite_temp_18), 0)) spite_overflowed("index * 8", "an Integer", "*", (int64_t)spite_temp_16, (int64_t)spite_temp_17, spite_site_6()); spite_temp_18; })))));
        }
        else {
            ({ Spite_Function* spite_temp_19 = piece_; ((void (*)(void*, int32_t, int32_t))spite_temp_19->spite_typed_call)(spite_temp_19->spite_owner, index_, (index_ + 1)); });
        }
        index_ = (index_ + 1);
    }
    index_ = 0;
    while (((index_ < count_))) {
        if ((((index_ != last_))) && (((SpiteMemory_Address_read_long(marks_, SpiteInteger_to_long(({ int32_t spite_temp_20 = index_; int32_t spite_temp_21 = 8; int32_t spite_temp_22; if (__builtin_expect(__builtin_mul_overflow(spite_temp_20, spite_temp_21, &spite_temp_22), 0)) spite_overflowed("index * 8", "an Integer", "*", (int64_t)spite_temp_20, (int64_t)spite_temp_21, spite_site_7()); spite_temp_22; }))) != SpiteInteger_to_long(0))))) {
            ThreadPool_join(self, (block_ + ((int64_t)(({ int32_t spite_temp_23 = index_; int32_t spite_temp_24 = 8; int32_t spite_temp_25; if (__builtin_expect(__builtin_mul_overflow(spite_temp_23, spite_temp_24, &spite_temp_25), 0)) spite_overflowed("index * 8", "an Integer", "*", (int64_t)spite_temp_23, (int64_t)spite_temp_24, spite_site_8()); spite_temp_25; })))));
        }
        index_ = (index_ + 1);
    }
    Memory_Heap_free(self->heap_, block_);
    Spite_Function___release(piece_);
}

void Sine_render(Sine* self) {
    int32_t sample_ = 0;
    while (((sample_ < 30000000))) {
        self->phase_ = (({ int32_t spite_temp_26 = self->phase_; int32_t spite_temp_27 = 7; int32_t spite_temp_28; if (__builtin_expect(__builtin_add_overflow(spite_temp_26, spite_temp_27, &spite_temp_28), 0)) spite_overflowed("phase + 7", "an Integer", "+", (int64_t)spite_temp_26, (int64_t)spite_temp_27, spite_site_9()); spite_temp_28; }) % 1000);
        int32_t folded_ = self->phase_;
        if (((folded_ > 500))) {
            folded_ = (1000 - folded_);
        }
        self->level_ = ({ int64_t spite_temp_29 = self->level_; int64_t spite_temp_30 = SpiteInteger_to_long(folded_); int64_t spite_temp_31; if (__builtin_expect(__builtin_add_overflow(spite_temp_29, spite_temp_30, &spite_temp_31), 0)) spite_overflowed("level + folded", "a Long", "+", (int64_t)spite_temp_29, (int64_t)spite_temp_30, spite_site_10()); spite_temp_31; });
        sample_ = (sample_ + 1);
    }
}
