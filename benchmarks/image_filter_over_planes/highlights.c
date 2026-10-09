/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

struct Pixel {
    SpiteHeader header;
    int32_t red_;
    int32_t green_;
    int32_t blue_;
    int32_t alpha_;
};

void List_Pixel_each_brighten(List_Pixel* self) {
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        Pixel* item_ = ((Pixel**)(intptr_t)self->items_)[index_];
        Pixel_brighten(item_);
        index_ = (index_ + 1);
    }
}

void Pixel_brighten(Pixel* self) {
    self->red_ = Pixel_brightened(self, self->red_);
    self->green_ = Pixel_brightened(self, self->green_);
    self->blue_ = Pixel_brightened(self, self->blue_);
}

int32_t Pixel_brightened(Pixel* self, int32_t channel_) {
    int32_t lifted_ = ({ int32_t spite_temp_1 = channel_; int32_t spite_temp_2 = 16; int32_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("channel + 16", "an Integer", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
    int32_t spite_temp_4 = SpiteInteger_minimum(lifted_, 255);
    return spite_temp_4;
}

List_Integer* Naive_histogram_of___held_0(Naive* self, List_Pixel* pixels_) {
    List_Integer* histogram_ = List_Integer___make();
    int32_t level_ = 0;
    while (((level_ < 256))) {
        List_Integer_append(histogram_, 0);
        level_ = (level_ + 1);
    }
    int32_t index_ = 0;
    while (((index_ < spite_folded_List_Pixel_count(pixels_)))) {
        Pixel* pixel_ = ({ List_Pixel* spite_temp_5 = pixels_; int32_t spite_temp_6 = index_; if (__builtin_expect(spite_temp_6 < 0 || spite_temp_6 >= (spite_temp_5)->item_count_, 0)) spite_outside_list("pixels[index]", spite_site_2()); ((Pixel**)(intptr_t)(spite_temp_5)->items_)[spite_temp_6]; });
        int32_t luminance_ = Pixel_luminance(pixel_);
        if (!(((List_Integer_get_at(histogram_, luminance_)).has_value))) {
            spite_failed_1(luminance_, histogram_, level_, index_, self);
        }
        List_Integer_set_at(histogram_, luminance_, ({ int32_t spite_temp_7 = (List_Integer_get_at(histogram_, luminance_)).value; int32_t spite_temp_8 = 1; int32_t spite_temp_9; if (__builtin_expect(__builtin_add_overflow(spite_temp_7, spite_temp_8, &spite_temp_9), 0)) spite_overflowed("histogram[luminance] + 1", "an Integer", "+", (int64_t)spite_temp_7, (int64_t)spite_temp_8, spite_site_3()); spite_temp_9; }));
        index_ = (index_ + 1);
    }
    List_Integer* spite_temp_10 = List_Integer___retain(histogram_);
    List_Integer___release(histogram_);
    return spite_temp_10;
}

int64_t List_Pixel_sum_weight(List_Pixel* self) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        Pixel* item_ = ((Pixel**)(intptr_t)self->items_)[index_];
        total_ = ({ int64_t spite_temp_11 = total_; int64_t spite_temp_12 = Pixel_weight(item_); int64_t spite_temp_13; if (__builtin_expect(__builtin_add_overflow(spite_temp_11, spite_temp_12, &spite_temp_13), 0)) spite_overflowed("total + item.attributes[member]", "a Long", "+", (int64_t)spite_temp_11, (int64_t)spite_temp_12, spite_site_4()); spite_temp_13; });
        index_ = (index_ + 1);
    }
    int64_t spite_temp_14 = total_;
    return spite_temp_14;
}
