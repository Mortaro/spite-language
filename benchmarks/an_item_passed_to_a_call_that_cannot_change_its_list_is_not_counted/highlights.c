/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Playlist_listen(Playlist* self, int32_t count_) {
    int32_t total_ = 0;
    int32_t index_ = 0;
    while (((index_ < count_))) {
        int32_t at_ = (index_ % 16);
        if (!(({ List_Track* spite_temp_1 = self->tracks_; int32_t spite_temp_2 = at_; (spite_temp_2 >= 0 && spite_temp_2 < (spite_temp_1)->item_count_) && ((((Track**)(intptr_t)(spite_temp_1)->items_)[spite_temp_2]) != 0); }))) {
            spite_failed_1(at_, self, count_, total_, index_);
        }
        total_ = ({ int32_t spite_temp_3 = ({ int32_t spite_temp_4 = total_; int32_t spite_temp_5 = (Playlist_length_of___held_0(self, ({ List_Track* spite_temp_6 = self->tracks_; int32_t spite_temp_7 = at_; if (__builtin_expect(spite_temp_7 < 0 || spite_temp_7 >= (spite_temp_6)->item_count_, 0)) spite_outside_list("tracks[at]", spite_site_1()); ((Track**)(intptr_t)(spite_temp_6)->items_)[spite_temp_7]; })) % 7); int32_t spite_temp_8; if (__builtin_expect(__builtin_add_overflow(spite_temp_4, spite_temp_5, &spite_temp_8), 0)) spite_overflowed("total + length_of(tracks[at]) % 7", "an Integer", "+", (int64_t)spite_temp_4, (int64_t)spite_temp_5, spite_site_1()); spite_temp_8; }); int32_t spite_temp_9 = ((({ List_Track* spite_temp_10 = self->tracks_; int32_t spite_temp_11 = at_; if (__builtin_expect(spite_temp_11 < 0 || spite_temp_11 >= (spite_temp_10)->item_count_, 0)) spite_outside_list("tracks[at]", spite_site_1()); ((Track**)(intptr_t)(spite_temp_10)->items_)[spite_temp_11]; }))->seconds_ % 5); int32_t spite_temp_12; if (__builtin_expect(__builtin_add_overflow(spite_temp_3, spite_temp_9, &spite_temp_12), 0)) spite_overflowed("total + length_of(tracks[at]) % 7 + tracks[at].seconds % 5", "an Integer", "+", (int64_t)spite_temp_3, (int64_t)spite_temp_9, spite_site_1()); spite_temp_12; });
        index_ = (index_ + 1);
    }
    int32_t spite_temp_13 = total_;
    return spite_temp_13;
}

int32_t Playlist_length_of___held_0(Playlist* self, Track* track_) {
    int32_t spite_temp_14 = (track_)->seconds_;
    return spite_temp_14;
}
