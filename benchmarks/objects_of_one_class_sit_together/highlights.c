/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

static void Point___pool_grow(void) {
    if (Point___pool_count == 0) { Point___pool_count = 16; } else if (Point___pool_count * sizeof(Point) < 262144) { Point___pool_count = Point___pool_count * 2; }
    char* chunk = (char*)SPITE_MALLOC(Point___pool_count * sizeof(Point) + 63);
    if (chunk == 0) { fflush(stdout); fputs("spite: out of memory making an object\n", stderr); exit(1); }
    Point___pool_next = (char*)(((uintptr_t)chunk + 63) & ~(uintptr_t)63);
    Point___pool_end = Point___pool_next + Point___pool_count * sizeof(Point);
}

static inline Point* Point___pool_take(void) {
    Point* self = Point___pool_free;
    if (self != 0) { Point___pool_free = *(Point**)self; return self; }
    if (Point___pool_next == Point___pool_end) Point___pool_grow();
    self = (Point*)Point___pool_next;
    Point___pool_next = Point___pool_next + sizeof(Point);
    return self;
}

static inline void Point___pool_give(Point* self) {
    *(Point**)self = Point___pool_free;
    Point___pool_free = self;
}

Point* Point___allocate(void) {
    Point* self = Point___pool_take();
    self->header.ref_count = 1;
    self->header.class_id = 113;
    Point___init(self);
    #ifdef SPITE_TRACKS_Point
    spite_track_Point(self);
    #endif
    return self;
}

Note* Note___allocate(void) {
    Note* self = Note___pool_take();
    self->header.ref_count = 1;
    self->header.class_id = 112;
    Note___init(self);
    #ifdef SPITE_TRACKS_Note
    spite_track_Note(self);
    #endif
    return self;
}

int64_t Naive_sum_points(Naive* self) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t round_ = 0;
    while (((round_ < 30))) {
        int32_t position_ = 0;
        while (((position_ < List_Point_count(self->points_)))) {
            Point* point_ = ({ List_Point* spite_temp_1 = self->points_; int32_t spite_temp_2 = position_; if (__builtin_expect(spite_temp_2 < 0 || spite_temp_2 >= (spite_temp_1)->item_count_, 0)) spite_outside_list("points[position]", spite_site_1()); ((Point**)(intptr_t)(spite_temp_1)->items_)[spite_temp_2]; });
            if ((((point_)->down_ >= round_))) {
                total_ = ({ int64_t spite_temp_3 = ({ int64_t spite_temp_4 = total_; int64_t spite_temp_5 = SpiteInteger_to_long((point_)->across_); int64_t spite_temp_6; if (__builtin_expect(__builtin_add_overflow(spite_temp_4, spite_temp_5, &spite_temp_6), 0)) spite_overflowed("total + point.across", "a Long", "+", (int64_t)spite_temp_4, (int64_t)spite_temp_5, spite_site_2()); spite_temp_6; }); int64_t spite_temp_7 = SpiteInteger_to_long((point_)->down_); int64_t spite_temp_8; if (__builtin_expect(__builtin_add_overflow(spite_temp_3, spite_temp_7, &spite_temp_8), 0)) spite_overflowed("total + point.across + point.down", "a Long", "+", (int64_t)spite_temp_3, (int64_t)spite_temp_7, spite_site_2()); spite_temp_8; });
            }
            position_ = (position_ + 1);
        }
        round_ = (round_ + 1);
    }
    int64_t spite_temp_9 = total_;
    return spite_temp_9;
}
