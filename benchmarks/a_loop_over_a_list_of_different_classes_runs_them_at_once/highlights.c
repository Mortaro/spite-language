/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void Naive_Naive(Naive* self) {
    List_Naive_Voice_append(self->voices_, Naive_Voice___retain(spite_tagged_object(111, (void*)(self->sine_))));
    List_Naive_Voice_append(self->voices_, Naive_Voice___retain(spite_tagged_object(112, (void*)(self->square_))));
    List_Naive_Voice_append(self->voices_, Naive_Voice___retain(spite_tagged_object(110, (void*)(self->saw_))));
    int64_t start_ = Clock_elapsed_nanoseconds(self->clock_);
    
    { int64_t spite_row_1_marks[64];
        
        if (({ List_Naive_Voice* spite_row_list = self->voices_; static const unsigned char spite_row_table[3][3] = {{0, 1, 1}, {1, 0, 1}, {1, 1, 0}}; static const unsigned char spite_row_heavy[3] = {1, 1, 1}; int32_t spite_row_n = spite_row_list->item_count_; int32_t spite_row_seen[64]; int32_t spite_row_heavies = 0; bool spite_row_ok = spite_row_n >= 2 && spite_row_n <= 3; for (int32_t spite_row_i = 0; spite_row_ok && spite_row_i < spite_row_n; spite_row_i++) { int32_t spite_row_k = -1; switch (((SpiteTagged*)(intptr_t)spite_row_list->items_)[spite_row_i].tag) { case 111: spite_row_k = 0; break; case 112: spite_row_k = 1; break; case 110: spite_row_k = 2; break; default: break; } spite_row_ok = spite_row_k >= 0; for (int32_t spite_row_j = 0; spite_row_ok && spite_row_j < spite_row_i; spite_row_j++) spite_row_ok = spite_row_table[spite_row_seen[spite_row_j]][spite_row_k] != 0; if (spite_row_ok) { spite_row_seen[spite_row_i] = spite_row_k; spite_row_1_marks[spite_row_i] = spite_row_heavy[spite_row_k]; spite_row_heavies += spite_row_heavy[spite_row_k]; } } spite_row_ok && spite_row_heavies >= 2; })) {
            List_Naive_Voice_spite_row_render(self->voices_, ((int64_t)(intptr_t)spite_row_1_marks));
        } else {
            
            List_Naive_Voice_each_render(self->voices_);
            
        }}
    
    int64_t microseconds_ = (({ int64_t spite_temp_1 = Clock_elapsed_nanoseconds(self->clock_); int64_t spite_temp_2 = start_; int64_t spite_temp_3; if (__builtin_expect(__builtin_sub_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("clock.elapsed_nanoseconds() - start", "a Long", "-", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; }) / SpiteInteger_to_long(1000));
    List_Console_Printable spite_framed_1; Console_Printable spite_framed_1_items[3]; int32_t spite_framed_1_count = 0;
    Console_print(self->console_, ({ spite_framed_1_items[0] = spite_tagged_SpiteLong((self->sine_)->level_); spite_framed_1_items[1] = spite_tagged_SpiteLong((self->square_)->level_); spite_framed_1_items[2] = spite_tagged_SpiteLong((self->saw_)->level_); spite_framed_1_count = 3; List_Console_Printable___framed(&spite_framed_1, (int64_t)(intptr_t)spite_framed_1_items, 3); }));
    for (int32_t spite_index = 0; spite_index < spite_framed_1_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_1_items[spite_index]); }
    List_Console_Printable spite_framed_2; Console_Printable spite_framed_2_items[1]; int32_t spite_framed_2_count = 0;
    Console_error(self->console_, ({ spite_framed_2_items[0] = spite_tagged_object(0, spite_box_SpiteString(({ char spite_temp_4_digits[24]; SpiteString spite_temp_4 = SPITE_STATIC_STRING(spite_temp_4_digits, spite_long_digits(spite_temp_4_digits, (int64_t)(microseconds_))); SpiteString spite_temp_5[] = {spite_lit_1, spite_temp_4}; SpiteString spite_temp_6 = spite_string_join(2, spite_temp_5); spite_temp_6; }))); spite_framed_2_count = 1; List_Console_Printable___framed(&spite_framed_2, (int64_t)(intptr_t)spite_framed_2_items, 1); }));
    for (int32_t spite_index = 0; spite_index < spite_framed_2_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_2_items[spite_index]); }
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
    int32_t last_ = ({ int32_t spite_temp_7 = count_; int32_t spite_temp_8 = 1; int32_t spite_temp_9; if (__builtin_expect(__builtin_sub_overflow(spite_temp_7, spite_temp_8, &spite_temp_9), 0)) spite_overflowed("count - 1", "an Integer", "-", (int64_t)spite_temp_7, (int64_t)spite_temp_8, spite_site_2()); spite_temp_9; });
    while (((((last_ >= 0)) && ((SpiteMemory_Address_read_long(marks_, SpiteInteger_to_long(({ int32_t spite_temp_10 = last_; int32_t spite_temp_11 = 8; int32_t spite_temp_12; if (__builtin_expect(__builtin_mul_overflow(spite_temp_10, spite_temp_11, &spite_temp_12), 0)) spite_overflowed("last * 8", "an Integer", "*", (int64_t)spite_temp_10, (int64_t)spite_temp_11, spite_site_3()); spite_temp_12; }))) == SpiteInteger_to_long(0)))))) {
        last_ = ({ int32_t spite_temp_13 = last_; int32_t spite_temp_14 = 1; int32_t spite_temp_15; if (__builtin_expect(__builtin_sub_overflow(spite_temp_13, spite_temp_14, &spite_temp_15), 0)) spite_overflowed("last - 1", "an Integer", "-", (int64_t)spite_temp_13, (int64_t)spite_temp_14, spite_site_4()); spite_temp_15; });
    }
    int64_t block_ = Memory_Heap_allocate(self->heap_, SpiteInteger_to_long(({ int32_t spite_temp_16 = count_; int32_t spite_temp_17 = 8; int32_t spite_temp_18; if (__builtin_expect(__builtin_mul_overflow(spite_temp_16, spite_temp_17, &spite_temp_18), 0)) spite_overflowed("count * 8", "an Integer", "*", (int64_t)spite_temp_16, (int64_t)spite_temp_17, spite_site_5()); spite_temp_18; })));
    int32_t index_ = 0;
    while (((index_ < count_))) {
        if ((((index_ != last_))) && (((SpiteMemory_Address_read_long(marks_, SpiteInteger_to_long(({ int32_t spite_temp_19 = index_; int32_t spite_temp_20 = 8; int32_t spite_temp_21; if (__builtin_expect(__builtin_mul_overflow(spite_temp_19, spite_temp_20, &spite_temp_21), 0)) spite_overflowed("index * 8", "an Integer", "*", (int64_t)spite_temp_19, (int64_t)spite_temp_20, spite_site_6()); spite_temp_21; }))) != SpiteInteger_to_long(0))))) {
            ThreadPool_submit(self, Spite_Function___retain(piece_), index_, (index_ + 1), (block_ + ((int64_t)(({ int32_t spite_temp_22 = index_; int32_t spite_temp_23 = 8; int32_t spite_temp_24; if (__builtin_expect(__builtin_mul_overflow(spite_temp_22, spite_temp_23, &spite_temp_24), 0)) spite_overflowed("index * 8", "an Integer", "*", (int64_t)spite_temp_22, (int64_t)spite_temp_23, spite_site_7()); spite_temp_24; })))));
        }
        else {
            ({ Spite_Function* spite_temp_25 = piece_; ((void (*)(void*, int32_t, int32_t))spite_temp_25->spite_typed_call)(spite_temp_25->spite_owner, index_, (index_ + 1)); });
        }
        index_ = (index_ + 1);
    }
    index_ = 0;
    while (((index_ < count_))) {
        if ((((index_ != last_))) && (((SpiteMemory_Address_read_long(marks_, SpiteInteger_to_long(({ int32_t spite_temp_26 = index_; int32_t spite_temp_27 = 8; int32_t spite_temp_28; if (__builtin_expect(__builtin_mul_overflow(spite_temp_26, spite_temp_27, &spite_temp_28), 0)) spite_overflowed("index * 8", "an Integer", "*", (int64_t)spite_temp_26, (int64_t)spite_temp_27, spite_site_8()); spite_temp_28; }))) != SpiteInteger_to_long(0))))) {
            ThreadPool_join(self, (block_ + ((int64_t)(({ int32_t spite_temp_29 = index_; int32_t spite_temp_30 = 8; int32_t spite_temp_31; if (__builtin_expect(__builtin_mul_overflow(spite_temp_29, spite_temp_30, &spite_temp_31), 0)) spite_overflowed("index * 8", "an Integer", "*", (int64_t)spite_temp_29, (int64_t)spite_temp_30, spite_site_9()); spite_temp_31; })))));
        }
        index_ = (index_ + 1);
    }
    Memory_Heap_free(self->heap_, block_);
    Spite_Function___release(piece_);
}

void Sine_render(Sine* self) {
    int32_t sample_ = 0;
    while (((sample_ < 30000000))) {
        self->phase_ = (({ int32_t spite_temp_32 = self->phase_; int32_t spite_temp_33 = 7; int32_t spite_temp_34; if (__builtin_expect(__builtin_add_overflow(spite_temp_32, spite_temp_33, &spite_temp_34), 0)) spite_overflowed("phase + 7", "an Integer", "+", (int64_t)spite_temp_32, (int64_t)spite_temp_33, spite_site_10()); spite_temp_34; }) % 1000);
        int32_t folded_ = self->phase_;
        if (((folded_ > 500))) {
            folded_ = ({ int32_t spite_temp_35 = 1000; int32_t spite_temp_36 = folded_; int32_t spite_temp_37; if (__builtin_expect(__builtin_sub_overflow(spite_temp_35, spite_temp_36, &spite_temp_37), 0)) spite_overflowed("1000 - folded", "an Integer", "-", (int64_t)spite_temp_35, (int64_t)spite_temp_36, spite_site_11()); spite_temp_37; });
        }
        self->level_ = ({ int64_t spite_temp_38 = self->level_; int64_t spite_temp_39 = SpiteInteger_to_long(folded_); int64_t spite_temp_40; if (__builtin_expect(__builtin_add_overflow(spite_temp_38, spite_temp_39, &spite_temp_40), 0)) spite_overflowed("level + folded", "a Long", "+", (int64_t)spite_temp_38, (int64_t)spite_temp_39, spite_site_12()); spite_temp_40; });
        sample_ = (sample_ + 1);
    }
}
