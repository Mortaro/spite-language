/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void Naive_render_all(Naive* self) {
    
    { int64_t spite_row_1_marks[64];
        
        if (({ List_Naive_Track* spite_row_list = self->tracks_; static const unsigned char spite_row_table[2][2] = {{0, 1}, {1, 0}}; static const unsigned char spite_row_heavy[2] = {1, 1}; int32_t spite_row_n = spite_row_list->item_count_; int32_t spite_row_seen[64]; int32_t spite_row_heavies = 0; bool spite_row_ok = spite_row_n >= 2 && spite_row_n <= 2; for (int32_t spite_row_i = 0; spite_row_ok && spite_row_i < spite_row_n; spite_row_i++) { int32_t spite_row_k = -1; switch (((SpiteTagged*)(intptr_t)spite_row_list->items_)[spite_row_i].tag) { case 112: spite_row_k = 0; break; case 111: spite_row_k = 1; break; default: break; } spite_row_ok = spite_row_k >= 0; for (int32_t spite_row_j = 0; spite_row_ok && spite_row_j < spite_row_i; spite_row_j++) spite_row_ok = spite_row_table[spite_row_seen[spite_row_j]][spite_row_k] != 0; if (spite_row_ok) { spite_row_seen[spite_row_i] = spite_row_k; spite_row_1_marks[spite_row_i] = spite_row_heavy[spite_row_k]; spite_row_heavies += spite_row_heavy[spite_row_k]; } } spite_row_ok && spite_row_heavies >= 2; })) {
            SPITE_ROWS_ENTER(); List_Naive_Track_spite_row_render(self->tracks_, ((int64_t)(intptr_t)spite_row_1_marks)); SPITE_ROWS_LEAVE();
        } else {
            
            List_Naive_Track_each_render(self->tracks_);
            
        }}
    
}

void List_Naive_Track_spite_row_render(List_Naive_Track* self, int64_t marks_) {
    ThreadPool* pool_ = spite_singleton_ThreadPool();
    ThreadPool_run_marked(pool_, spite_function_value_List_Naive_Track_spite_row_render_piece(self), self->item_count_, marks_);
    ThreadPool___release(pool_);
}

void List_Naive_Track_spite_row_render_piece(List_Naive_Track* self, int32_t first_, int32_t end_) {
    int32_t index_ = first_;
    while (((index_ < end_))) {
        Naive_Track item_ = ((Naive_Track*)(intptr_t)self->items_)[index_];
        Naive_Track___call_render(item_);
        index_ = (index_ + 1);
    }
}

void Drum_render(Drum* self) {
    int32_t step_ = 0;
    while (((step_ < 20000000))) {
        self->phase_ = (({ int32_t spite_temp_1 = self->phase_; int32_t spite_temp_2 = 7; int32_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("phase + 7", "an Integer", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; }) % 1000);
        Meter_record(self->meter_, self->phase_);
        step_ = (step_ + 1);
    }
}

Meter_record(self->meter_, ({ int32_t spite_temp_4 = folded_; int32_t spite_temp_5 = 2; int32_t spite_temp_6; if (__builtin_expect(__builtin_mul_overflow(spite_temp_4, spite_temp_5, &spite_temp_6), 0)) spite_overflowed("folded * 2", "an Integer", "*", (int64_t)spite_temp_4, (int64_t)spite_temp_5, spite_site_2()); spite_temp_6; }));
