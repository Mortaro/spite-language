/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void Naive_edit_every_page(Naive* self) {
    int32_t round_ = 0;
    while (((round_ < 100))) {
        int32_t index_ = 0;
        while (((index_ < 100000))) {
            Desk_take(self->desk_, index_);
            Page* spite_temp_1 = ({ Desk* spite_temp_2 = self->desk_; Desk___outside_read_enter(); Page* spite_temp_3 = Page___retain((spite_temp_2)->open_); Desk___outside_read_leave(); spite_temp_3; });
            (spite_temp_1)->words_ = ({ int32_t spite_temp_4 = ({ Page* spite_temp_5 = ({ Desk* spite_temp_6 = self->desk_; Desk___outside_read_enter(); Page* spite_temp_7 = Page___retain((spite_temp_6)->open_); Desk___outside_read_leave(); spite_temp_7; }); int32_t spite_temp_8 = (spite_temp_5)->words_; Page___release(spite_temp_5); spite_temp_8; }); int32_t spite_temp_9 = 1; int32_t spite_temp_10; if (__builtin_expect(__builtin_add_overflow(spite_temp_4, spite_temp_9, &spite_temp_10), 0)) spite_overflowed("desk.open.words + 1", "an Integer", "+", (int64_t)spite_temp_4, (int64_t)spite_temp_9, spite_site_1()); spite_temp_10; });
            Page___release(spite_temp_1);
            index_ = (index_ + 1);
        }
        round_ = (round_ + 1);
    }
    self->total_ = Desk_total(self->desk_);
}

void Desk_take(Desk* self, int32_t index_) {
    SPITE_SINGLETON_STORE(Desk, self->at_, index_);
    if (!(({ List_Page* spite_temp_11 = self->pages_; int32_t spite_temp_12 = SPITE_SINGLETON_LOAD(Desk, self->at_); (spite_temp_12 >= 0 && spite_temp_12 < (spite_temp_11)->item_count_) && ((((Page**)(intptr_t)(spite_temp_11)->items_)[spite_temp_12]) != 0); }))) {
        spite_failed_1(self, index_);
    }
    Page* spite_temp_13 = List_Page_get_at(self->pages_, SPITE_SINGLETON_LOAD(Desk, self->at_));
    Page___release(self->open_);
    self->open_ = spite_temp_13;
}
