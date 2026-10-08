/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void Naive_Naive(Naive* self) {
    List_Orbit* orbits_ = List_Orbit___make();
    int32_t index_ = 0;
    while (((index_ < 100000))) {
        Orbit* orbit_ = Orbit___make(index_);
        List_Orbit_append(orbits_, Orbit___retain(orbit_));
        index_ = (index_ + 1);
        Orbit___release(orbit_);
    }
    int64_t start_ = Clock_elapsed_nanoseconds(self->clock_);
    int32_t tick_ = 0;
    while (((tick_ < 10))) {
        
        {
            
            if (({ List_Orbit* spite_band_list = orbits_; int32_t spite_band_n = spite_band_list->item_count_; bool spite_band_ok = spite_band_n >= 91; for (int32_t spite_band_i = 0; spite_band_ok && spite_band_i < spite_band_n; spite_band_i++) spite_band_ok = ((SpiteHeader*)(((void**)(intptr_t)spite_band_list->items_)[spite_band_i]))->ref_count == 1; spite_band_ok; })) {
                List_Orbit_spite_band_advance(orbits_);
            } else {
                
                List_Orbit_each_advance(orbits_);
                
            }}
        
        tick_ = (tick_ + 1);
    }
    int64_t microseconds_ = (({ int64_t spite_temp_1 = Clock_elapsed_nanoseconds(self->clock_); int64_t spite_temp_2 = start_; int64_t spite_temp_3; if (__builtin_expect(__builtin_sub_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("clock.elapsed_nanoseconds() - start", "a Long", "-", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; }) / SpiteInteger_to_long(1000));
    int32_t turns_ = List_Orbit_sum_turns(orbits_);
    List_Console_Printable spite_framed_1; Console_Printable spite_framed_1_items[2]; int32_t spite_framed_1_count = 0;
    Console_print(self->console_, ({ spite_framed_1_items[0] = spite_tagged_object(0, ((void*)&spite_lit_1_box)); spite_framed_1_items[1] = spite_tagged_SpiteInteger(turns_); spite_framed_1_count = 2; List_Console_Printable___framed(&spite_framed_1, (int64_t)(intptr_t)spite_framed_1_items, 2); }));
    for (int32_t spite_index = 0; spite_index < spite_framed_1_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_1_items[spite_index]); }
    List_Console_Printable spite_framed_2; Console_Printable spite_framed_2_items[1]; int32_t spite_framed_2_count = 0;
    Console_error(self->console_, ({ spite_framed_2_items[0] = spite_tagged_object(0, spite_box_SpiteString(({ char spite_temp_4_digits[24]; SpiteString spite_temp_4 = SPITE_STATIC_STRING(spite_temp_4_digits, spite_long_digits(spite_temp_4_digits, (int64_t)(microseconds_))); SpiteString spite_temp_5[] = {spite_lit_2, spite_temp_4}; SpiteString spite_temp_6 = spite_string_join(2, spite_temp_5); spite_temp_6; }))); spite_framed_2_count = 1; List_Console_Printable___framed(&spite_framed_2, (int64_t)(intptr_t)spite_framed_2_items, 1); }));
    for (int32_t spite_index = 0; spite_index < spite_framed_2_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_2_items[spite_index]); }
    List_Orbit___release(orbits_);
}

void List_Orbit_spite_band_advance(List_Orbit* self) {
    ThreadPool* pool_ = spite_singleton_ThreadPool();
    ThreadPool_run_bands(pool_, spite_function_value_List_Orbit_spite_band_advance_piece(self), self->item_count_);
    ThreadPool___release(pool_);
}

void List_Orbit_spite_band_advance_piece(List_Orbit* self, int32_t first_, int32_t end_) {
    int32_t index_ = first_;
    while (((index_ < end_))) {
        Orbit* item_ = ((Orbit**)(intptr_t)self->items_)[index_];
        Orbit_advance(item_);
        index_ = (index_ + 1);
    }
}

void ThreadPool_run_bands(ThreadPool* self, Spite_Function* piece_, int32_t count_) {
    int32_t bands_ = ({ int32_t spite_temp_7 = ThreadPool_size(self); int32_t spite_temp_8 = 1; int32_t spite_temp_9; if (__builtin_expect(__builtin_add_overflow(spite_temp_7, spite_temp_8, &spite_temp_9), 0)) spite_overflowed("size() + 1", "an Integer", "+", (int64_t)spite_temp_7, (int64_t)spite_temp_8, spite_site_2()); spite_temp_9; });
    if (((bands_ > count_))) {
        bands_ = count_;
    }
    if (((bands_ < 2))) {
        ({ Spite_Function* spite_temp_10 = piece_; ((void (*)(void*, int32_t, int32_t))spite_temp_10->spite_typed_call)(spite_temp_10->spite_owner, 0, count_); });
        Spite_Function___release(piece_);
        return;
    }
    int64_t block_ = Memory_Heap_allocate(self->heap_, SpiteInteger_to_long(({ int32_t spite_temp_11 = bands_; int32_t spite_temp_12 = 8; int32_t spite_temp_13; if (__builtin_expect(__builtin_mul_overflow(spite_temp_11, spite_temp_12, &spite_temp_13), 0)) spite_overflowed("bands * 8", "an Integer", "*", (int64_t)spite_temp_11, (int64_t)spite_temp_12, spite_site_3()); spite_temp_13; })));
    int32_t base_ = ({ int32_t spite_temp_14 = count_; int32_t spite_temp_15 = bands_; if (spite_temp_15 == 0) spite_divided_by_zero("count / bands", spite_site_4()); int32_t spite_temp_16 = 0; if (__builtin_expect(spite_temp_15 == -1 && __builtin_sub_overflow((int32_t)0, spite_temp_14, &spite_temp_16), 0)) spite_overflowed("count / bands", "an Integer", "/", (int64_t)spite_temp_14, (int64_t)spite_temp_15, spite_site_4()); (int32_t)(spite_temp_15 == -1 ? spite_temp_16 : spite_temp_14 / spite_temp_15); });
    int32_t extra_ = ({ int32_t spite_temp_17 = count_; int32_t spite_temp_18 = bands_; if (spite_temp_18 == 0) spite_divided_by_zero("count % bands", spite_site_5()); (int32_t)(spite_temp_18 == -1 ? (int32_t)0 : spite_temp_17 % spite_temp_18); });
    int32_t index_ = 1;
    while (((index_ < bands_))) {
        int32_t first_ = ThreadPool_piece_start(self, index_, base_, extra_);
        int32_t end_ = ThreadPool_piece_start(self, (index_ + 1), base_, extra_);
        ThreadPool_submit(self, Spite_Function___retain(piece_), first_, end_, (block_ + ((int64_t)(({ int32_t spite_temp_19 = index_; int32_t spite_temp_20 = 8; int32_t spite_temp_21; if (__builtin_expect(__builtin_mul_overflow(spite_temp_19, spite_temp_20, &spite_temp_21), 0)) spite_overflowed("index * 8", "an Integer", "*", (int64_t)spite_temp_19, (int64_t)spite_temp_20, spite_site_6()); spite_temp_21; })))));
        index_ = (index_ + 1);
    }
    int32_t first_end_ = ThreadPool_piece_start(self, 1, base_, extra_);
    ({ Spite_Function* spite_temp_22 = piece_; ((void (*)(void*, int32_t, int32_t))spite_temp_22->spite_typed_call)(spite_temp_22->spite_owner, 0, first_end_); });
    index_ = 1;
    while (((index_ < bands_))) {
        ThreadPool_join(self, (block_ + ((int64_t)(({ int32_t spite_temp_23 = index_; int32_t spite_temp_24 = 8; int32_t spite_temp_25; if (__builtin_expect(__builtin_mul_overflow(spite_temp_23, spite_temp_24, &spite_temp_25), 0)) spite_overflowed("index * 8", "an Integer", "*", (int64_t)spite_temp_23, (int64_t)spite_temp_24, spite_site_7()); spite_temp_25; })))));
        index_ = (index_ + 1);
    }
    Memory_Heap_free(self->heap_, block_);
    Spite_Function___release(piece_);
}

void Orbit_advance(Orbit* self) {
    int32_t step_ = 0;
    while (((step_ < 1000))) {
        self->angle_ = (self->angle_ + (self->speed_ * 0.001f));
        if (((self->angle_ > 6.28318f))) {
            self->angle_ = (self->angle_ - 6.28318f);
            self->turns_ = ({ int32_t spite_temp_26 = self->turns_; int32_t spite_temp_27 = 1; int32_t spite_temp_28; if (__builtin_expect(__builtin_add_overflow(spite_temp_26, spite_temp_27, &spite_temp_28), 0)) spite_overflowed("turns + 1", "an Integer", "+", (int64_t)spite_temp_26, (int64_t)spite_temp_27, spite_site_8()); spite_temp_28; });
        }
        step_ = (step_ + 1);
    }
}
