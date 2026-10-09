/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void Naive_advance_ticks(Naive* self) {
    int32_t tick_ = 0;
    while (((tick_ < 10))) {
        
        {
            
            if (({ List_Orbit* spite_band_list = self->orbits_; int32_t spite_band_n = spite_band_list->item_count_; bool spite_band_ok = spite_band_n >= 91; for (int32_t spite_band_i = 0; spite_band_ok && spite_band_i < spite_band_n; spite_band_i++) spite_band_ok = ((SpiteHeader*)(((void**)(intptr_t)spite_band_list->items_)[spite_band_i]))->ref_count == 1; spite_band_ok; })) {
                List_Orbit_spite_band_advance(self->orbits_);
            } else {
                
                List_Orbit_each_advance(self->orbits_);
                
            }}
        
        tick_ = (tick_ + 1);
    }
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
    int32_t bands_ = ({ int32_t spite_temp_1 = ThreadPool_size(self); int32_t spite_temp_2 = 1; int32_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("size() + 1", "an Integer", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
    if (((bands_ > count_))) {
        bands_ = count_;
    }
    if (((bands_ < 2))) {
        ({ Spite_Function* spite_temp_4 = piece_; ((void (*)(void*, int32_t, int32_t))spite_temp_4->spite_typed_call)(spite_temp_4->spite_owner, 0, count_); });
        Spite_Function___release(piece_);
        return;
    }
    int64_t block_ = Memory_Heap_allocate(self->heap_, SpiteInteger_to_long(({ int32_t spite_temp_5 = bands_; int32_t spite_temp_6 = 8; int32_t spite_temp_7; if (__builtin_expect(__builtin_mul_overflow(spite_temp_5, spite_temp_6, &spite_temp_7), 0)) spite_overflowed("bands * 8", "an Integer", "*", (int64_t)spite_temp_5, (int64_t)spite_temp_6, spite_site_2()); spite_temp_7; })));
    int32_t base_ = ({ int32_t spite_temp_8 = count_; int32_t spite_temp_9 = bands_; if (spite_temp_9 == 0) spite_divided_by_zero("count / bands", spite_site_3()); int32_t spite_temp_10 = 0; if (__builtin_expect(spite_temp_9 == -1 && __builtin_sub_overflow((int32_t)0, spite_temp_8, &spite_temp_10), 0)) spite_overflowed("count / bands", "an Integer", "/", (int64_t)spite_temp_8, (int64_t)spite_temp_9, spite_site_3()); (int32_t)(spite_temp_9 == -1 ? spite_temp_10 : spite_temp_8 / spite_temp_9); });
    int32_t extra_ = ({ int32_t spite_temp_11 = count_; int32_t spite_temp_12 = bands_; if (spite_temp_12 == 0) spite_divided_by_zero("count % bands", spite_site_4()); (int32_t)(spite_temp_12 == -1 ? (int32_t)0 : spite_temp_11 % spite_temp_12); });
    int32_t index_ = 1;
    while (((index_ < bands_))) {
        int32_t first_ = ThreadPool_piece_start(self, index_, base_, extra_);
        int32_t end_ = ThreadPool_piece_start(self, (index_ + 1), base_, extra_);
        ThreadPool_submit(self, Spite_Function___retain(piece_), first_, end_, (block_ + ((int64_t)(({ int32_t spite_temp_13 = index_; int32_t spite_temp_14 = 8; int32_t spite_temp_15; if (__builtin_expect(__builtin_mul_overflow(spite_temp_13, spite_temp_14, &spite_temp_15), 0)) spite_overflowed("index * 8", "an Integer", "*", (int64_t)spite_temp_13, (int64_t)spite_temp_14, spite_site_5()); spite_temp_15; })))));
        index_ = (index_ + 1);
    }
    int32_t first_end_ = ThreadPool_piece_start(self, 1, base_, extra_);
    ({ Spite_Function* spite_temp_16 = piece_; ((void (*)(void*, int32_t, int32_t))spite_temp_16->spite_typed_call)(spite_temp_16->spite_owner, 0, first_end_); });
    index_ = 1;
    while (((index_ < bands_))) {
        ThreadPool_join(self, (block_ + ((int64_t)(({ int32_t spite_temp_17 = index_; int32_t spite_temp_18 = 8; int32_t spite_temp_19; if (__builtin_expect(__builtin_mul_overflow(spite_temp_17, spite_temp_18, &spite_temp_19), 0)) spite_overflowed("index * 8", "an Integer", "*", (int64_t)spite_temp_17, (int64_t)spite_temp_18, spite_site_6()); spite_temp_19; })))));
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
            self->turns_ = ({ int32_t spite_temp_20 = self->turns_; int32_t spite_temp_21 = 1; int32_t spite_temp_22; if (__builtin_expect(__builtin_add_overflow(spite_temp_20, spite_temp_21, &spite_temp_22), 0)) spite_overflowed("turns + 1", "an Integer", "+", (int64_t)spite_temp_20, (int64_t)spite_temp_21, spite_site_7()); spite_temp_22; });
        }
        step_ = (step_ + 1);
    }
}
