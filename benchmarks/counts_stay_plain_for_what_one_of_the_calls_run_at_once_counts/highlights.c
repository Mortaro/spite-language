/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void List_Naive_Collection_spite_row_sort(List_Naive_Collection* self, int64_t marks_) {
    ThreadPool* pool_ = spite_singleton_ThreadPool();
    ThreadPool_run_marked(pool_, spite_function_value_List_Naive_Collection_spite_row_sort_piece(self), self->item_count_, marks_);
    ThreadPool___release(pool_);
}

void List_Naive_Collection_spite_row_sort_piece(List_Naive_Collection* self, int32_t first_, int32_t end_) {
    int32_t index_ = first_;
    while (((index_ < end_))) {
        Naive_Collection item_ = ((Naive_Collection*)(intptr_t)self->items_)[index_];
        Naive_Collection___call_sort(item_);
        index_ = (index_ + 1);
    }
}

void Archive_sort(Archive* self) {
    int32_t round_ = 0;
    while (((round_ < 2000))) {
        List_Letter* kept_ = List_Letter___make();
        int32_t index_ = 0;
        while (((index_ < spite_folded_List_Letter_count(self->letters_)))) {
            Letter* letter_ = ({ List_Letter* spite_temp_1 = self->letters_; int32_t spite_temp_2 = index_; if (__builtin_expect(spite_temp_2 < 0 || spite_temp_2 >= (spite_temp_1)->item_count_, 0)) spite_outside_list("letters[index]", spite_site_1()); ((Letter**)(intptr_t)(spite_temp_1)->items_)[spite_temp_2]; });
            if (((((letter_)->words_ % 3) == (round_ % 3)))) {
                List_Letter_append(kept_, Letter___retain(letter_));
            }
            index_ = (index_ + 1);
        }
        self->kept_total_ = ({ int32_t spite_temp_3 = self->kept_total_; int32_t spite_temp_4 = spite_folded_List_Letter_count(kept_); int32_t spite_temp_5; if (__builtin_expect(__builtin_add_overflow(spite_temp_3, spite_temp_4, &spite_temp_5), 0)) spite_overflowed("kept_total + kept.count()", "an Integer", "+", (int64_t)spite_temp_3, (int64_t)spite_temp_4, spite_site_2()); spite_temp_5; });
        round_ = (round_ + 1);
        List_Letter___release(kept_);
    }
}

static inline Letter* Letter___retain(Letter* self) {
    if (self != 0) SPITE_PLAIN_COUNT_UP(self->header.ref_count);
    return self;
}

static inline void Letter___release(Letter* self) {
    if (self == 0) return;
    if (SPITE_PLAIN_COUNT_DOWN(self->header.ref_count) > 0) return;
    Letter___free(self);
}
