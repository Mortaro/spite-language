/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int64_t Naive_sum_on_two(Naive* self, int32_t limit_) {
    Summer* evens_ = Summer___make(limit_, 0);
    Parallel__Long* even_sum_ = Parallel__Long___make(spite_function_value_Summer_total(evens_));
    Summer spite_slot_1;
    Summer* odds_ = Summer___make_into(&spite_slot_1, limit_, 1);
    int64_t odd_sum_ = Summer_total(odds_);
    int64_t spite_temp_1 = ({ int64_t spite_temp_2 = Parallel__Long__result(even_sum_); int64_t spite_temp_3 = odd_sum_; int64_t spite_temp_4; if (__builtin_expect(__builtin_add_overflow(spite_temp_2, spite_temp_3, &spite_temp_4), 0)) spite_overflowed("even_sum + odd_sum", "a Long", "+", (int64_t)spite_temp_2, (int64_t)spite_temp_3, spite_site_1()); spite_temp_4; });
    Parallel__Long___release(even_sum_);
    Summer___release(evens_);
    return spite_temp_1;
}

ThreadPool* spite_singleton_ThreadPool(void) {
    ThreadPool* found = SPITE_SINGLETON_FOUND(spite_singleton_ThreadPool_cache);
    if (found != 0) return found;
    spite_singleton_check_circle("ThreadPool");
    SPITE_LOCK(spite_singleton_ThreadPool_lock);
    if (spite_singleton_ThreadPool_cache == 0) {
        if (spite_singleton_ThreadPool_destroyed) spite_singleton_used_after_exit("ThreadPool");
        spite_singleton_making("ThreadPool");
        ThreadPool* made = ThreadPool___make();
        spite_singleton_made();
        spite_singleton_created(spite_singleton_ThreadPool_teardown);
        SPITE_SINGLETON_PUBLISH(spite_singleton_ThreadPool_cache, made);
    }
    SPITE_UNLOCK(spite_singleton_ThreadPool_lock);
    return spite_singleton_ThreadPool_cache;
}

void ThreadPool_submit(ThreadPool* self, Spite_Function* job_, int32_t first_, int32_t end_, int64_t state_) {
    ThreadPool_start(self);
    SpiteMemory_Address_write_long_atomically(state_, SpiteInteger_to_long(0), SpiteInteger_to_long(0));
    ThreadPoolJob* queued_ = ThreadPoolJob___make();
    Spite_Function* spite_temp_5 = Spite_Function___retain(job_);
    Spite_Function___release((queued_)->work_);
    (queued_)->work_ = spite_temp_5;
    (queued_)->first_ = first_;
    (queued_)->end_ = end_;
    (queued_)->state_ = state_;
    ThreadPool__task_begun(self);
    ThreadPool_lock_queue(self);
    List_ThreadPoolJob_append(self->jobs_, ThreadPoolJob___retain(queued_));
    ThreadPool_signal_one(self, self->work_ready_);
    ThreadPool_unlock_queue(self);
    ThreadPoolJob___release(queued_);
    Spite_Function___release(job_);
}

void ThreadPool_start(ThreadPool* self) {
    if (((self->worker_count_ == 0))) {
        self->queue_lock_ = ThreadPool_create_lock(self);
        self->work_ready_ = ThreadPool_create_condition(self);
        self->work_done_ = ThreadPool_create_condition(self);
        int32_t wanted_ = ({ int32_t spite_temp_6 = ThreadPool_processor_count(self); int32_t spite_temp_7 = 1; int32_t spite_temp_8; if (__builtin_expect(__builtin_sub_overflow(spite_temp_6, spite_temp_7, &spite_temp_8), 0)) spite_overflowed("processor_count() - 1", "an Integer", "-", (int64_t)spite_temp_6, (int64_t)spite_temp_7, spite_site_2()); spite_temp_8; });
        if (((wanted_ < 1))) {
            wanted_ = 1;
        }
        self->workers_ = Memory_Heap_allocate(self->heap_, SpiteInteger_to_long(({ int32_t spite_temp_9 = wanted_; int32_t spite_temp_10 = 8; int32_t spite_temp_11; if (__builtin_expect(__builtin_mul_overflow(spite_temp_9, spite_temp_10, &spite_temp_11), 0)) spite_overflowed("wanted * 8", "an Integer", "*", (int64_t)spite_temp_9, (int64_t)spite_temp_10, spite_site_3()); spite_temp_11; })));
        self->worker_threads_ = Memory_Heap_allocate(self->heap_, SpiteInteger_to_long(({ int32_t spite_temp_12 = wanted_; int32_t spite_temp_13 = 8; int32_t spite_temp_14; if (__builtin_expect(__builtin_mul_overflow(spite_temp_12, spite_temp_13, &spite_temp_14), 0)) spite_overflowed("wanted * 8", "an Integer", "*", (int64_t)spite_temp_12, (int64_t)spite_temp_13, spite_site_4()); spite_temp_14; })));
        int64_t entry_ = ThreadPool_entry_address(self);
        int64_t pool_ = ThreadPool_address(self);
        int32_t index_ = 0;
        while (((index_ < wanted_))) {
            int64_t thread_ = ThreadPool_start_thread(self, entry_, pool_);
            if (!(((thread_ != SpiteInteger_to_long(0))))) {
                spite_failed_1(thread_, wanted_, entry_, pool_, index_, self);
            }
            SpiteMemory_Address_write_long(self->workers_, SpiteInteger_to_long(({ int32_t spite_temp_15 = index_; int32_t spite_temp_16 = 8; int32_t spite_temp_17; if (__builtin_expect(__builtin_mul_overflow(spite_temp_15, spite_temp_16, &spite_temp_17), 0)) spite_overflowed("index * 8", "an Integer", "*", (int64_t)spite_temp_15, (int64_t)spite_temp_16, spite_site_5()); spite_temp_17; })), thread_);
            index_ = (index_ + 1);
        }
        self->worker_count_ = wanted_;
    }
}
