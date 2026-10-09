/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

struct Shape {
    SpiteHeader header;
    int32_t x_;
    int32_t y_;
    Texture* texture_;
    Velocity* velocity_;
};

void Shape_move(Shape* self) {
    if (!(((self->velocity_) != 0))) {
        SPITE_TRACE_ASSERT(spite_site_1());
        return;
    }
    self->x_ = (({ int32_t spite_temp_1 = self->x_; int32_t spite_temp_2 = (self->velocity_)->across_; int32_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("x + velocity.across", "an Integer", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_2()); spite_temp_3; }) % 1024);
    self->y_ = (({ int32_t spite_temp_4 = self->y_; int32_t spite_temp_5 = (self->velocity_)->down_; int32_t spite_temp_6; if (__builtin_expect(__builtin_add_overflow(spite_temp_4, spite_temp_5, &spite_temp_6), 0)) spite_overflowed("y + velocity.down", "an Integer", "+", (int64_t)spite_temp_4, (int64_t)spite_temp_5, spite_site_3()); spite_temp_6; }) % 1024);
}

int64_t Shape_drawn(Shape* self) {
    if ((((self->texture_) != 0)) && (((self->velocity_) != 0))) {
        int64_t total_ = SpiteInteger_to_long(({ int32_t spite_temp_7 = self->x_; int32_t spite_temp_8 = self->y_; int32_t spite_temp_9; if (__builtin_expect(__builtin_add_overflow(spite_temp_7, spite_temp_8, &spite_temp_9), 0)) spite_overflowed("x + y", "an Integer", "+", (int64_t)spite_temp_7, (int64_t)spite_temp_8, spite_site_4()); spite_temp_9; }));
        int64_t spite_temp_10 = ({ int64_t spite_temp_11 = total_; int64_t spite_temp_12 = SpiteInteger_to_long((self->texture_)->scale_); int64_t spite_temp_13; if (__builtin_expect(__builtin_mul_overflow(spite_temp_11, spite_temp_12, &spite_temp_13), 0)) spite_overflowed("total * texture.scale", "a Long", "*", (int64_t)spite_temp_11, (int64_t)spite_temp_12, spite_site_5()); spite_temp_13; });
        return spite_temp_10;
    }
    int64_t spite_temp_14 = SpiteInteger_to_long(0);
    return spite_temp_14;
}

int64_t List_Shape_sum_drawn(List_Shape* self) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        Shape* item_ = ((Shape**)(intptr_t)self->items_)[index_];
        total_ = ({ int64_t spite_temp_15 = total_; int64_t spite_temp_16 = Shape_drawn(item_); int64_t spite_temp_17; if (__builtin_expect(__builtin_add_overflow(spite_temp_15, spite_temp_16, &spite_temp_17), 0)) spite_overflowed("total + item.attributes[member]", "a Long", "+", (int64_t)spite_temp_15, (int64_t)spite_temp_16, spite_site_6()); spite_temp_17; });
        index_ = (index_ + 1);
    }
    int64_t spite_temp_18 = total_;
    return spite_temp_18;
}
