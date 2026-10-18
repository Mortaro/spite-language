/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

static inline Point* Point___retain(Point* self) {
    if (self != 0) SPITE_PLAIN_COUNT_UP(self->header.ref_count);
    return self;
}

static inline void Point___release(Point* self) {
    if (self == 0) return;
    if (SPITE_PLAIN_COUNT_DOWN(self->header.ref_count) > 0) return;
    Point___free(self);
}

static inline void Summer___release(Summer* self) {
    if (self == 0) return;
    if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
    Summer___free(self);
}

int64_t Naive_count_near___held_0(Naive* self, List_Point* points_) {
    int64_t near_ = SpiteInteger_to_long(0);
    int32_t round_ = 0;
    while (((round_ < 20))) {
        List_Point* kept_ = List_Point___make();
        int32_t index_ = 0;
        while (((index_ < spite_folded_List_Point_count(points_)))) {
            Point* point_ = ({ List_Point* spite_temp_1 = points_; int32_t spite_temp_2 = index_; if (__builtin_expect(spite_temp_2 < 0 || spite_temp_2 >= (spite_temp_1)->item_count_, 0)) spite_outside_list("points[index]", spite_site_1()); ((Point**)(intptr_t)(spite_temp_1)->items_)[spite_temp_2]; });
            if (((({ int32_t spite_temp_3 = (point_)->across_; int32_t spite_temp_4 = (point_)->down_; int32_t spite_temp_5; if (__builtin_expect(__builtin_add_overflow(spite_temp_3, spite_temp_4, &spite_temp_5), 0)) spite_overflowed("point.across + point.down", "an Integer", "+", (int64_t)spite_temp_3, (int64_t)spite_temp_4, spite_site_2()); spite_temp_5; }) > (round_ * 80)))) {
                List_Point_append(kept_, Point___retain(point_));
            }
            index_ = (index_ + 1);
        }
        near_ = (near_ + SpiteInteger_to_long(spite_folded_List_Point_count(kept_)));
        round_ = (round_ + 1);
        List_Point___release(kept_);
    }
    int64_t spite_temp_6 = near_;
    return spite_temp_6;
}
