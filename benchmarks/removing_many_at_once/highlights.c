/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int64_t Naive_all_rounds(Naive* self) {
    int64_t checksum_ = SpiteInteger_to_long(0);
    int32_t round_ = 0;
    while (((round_ < 40))) {
        int32_t removed_in_ten_ = 5;
        if ((((round_ % 2) == 1))) {
            removed_in_ten_ = 9;
        }
        Despawns_mark_every(self->despawns_, self->item_total_, removed_in_ten_);
        Naive_fill(self);
        Items__Velocity_remove_where_despawned(self->velocities_);
        List_Integer_remove_where_marked_for_despawns(self->entities_, Despawns___retain(self->despawns_));
        checksum_ = ({ int64_t spite_temp_1 = checksum_; int64_t spite_temp_2 = Naive_kept_sum(self); int64_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("checksum + kept_sum()", "a Long", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
        round_ = (round_ + 1);
    }
    int64_t spite_temp_4 = checksum_;
    return spite_temp_4;
}

void Naive_fill(Naive* self) {
    Items__Velocity_clear(self->velocities_);
    List_Integer_clear(self->entities_);
    int32_t entity_ = 0;
    while (((entity_ < self->item_total_))) {
        bool despawned_now_ = Despawns_marked(self->despawns_, entity_);
        Velocity spite_slot_1;
        Velocity* velocity_ = Velocity___make_into(&spite_slot_1, entity_, (1.0 * SpiteInteger_to_float(entity_)), despawned_now_);
        Items__Velocity_append(self->velocities_, Velocity___retain(velocity_));
        List_Integer_append(self->entities_, entity_);
        entity_ = (entity_ + 1);
    }
}

void Items__Velocity_remove_where_despawned(Items__Velocity* self) {
    int32_t kept_ = 0;
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        bool keep_ = true;
        {
            Velocity* item_ = InlineMemory__Velocity_item_at(self->inline_, self->items_, index_);
            if (((item_)->despawned_)) {
                keep_ = false;
            }
        }
        if ((keep_)) {
            if ((((kept_ != index_))) && (((index_ < self->item_count_)))) {
                Items__Velocity__swap(self, kept_, index_);
            }
            kept_ = (kept_ + 1);
        }
        index_ = (index_ + 1);
    }
    Items__Velocity_truncate(self, kept_);
}

void List_Integer_remove_where_marked_for_despawns(List_Integer* self, Despawns* owner_) {
    int32_t kept_ = 0;
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        int32_t item_ = TypedMemory__Integer_read_value(self->values_, self->items_, index_);
        bool keep_ = true;
        if ((Despawns_marked(owner_, item_))) {
            keep_ = false;
        }
        if ((keep_)) {
            if ((((kept_ != index_))) && (((index_ < self->item_count_)))) {
                TypedMemory__Integer_swap_values(self->values_, self->items_, kept_, index_);
            }
            kept_ = (kept_ + 1);
        }
        index_ = (index_ + 1);
    }
    List_Integer_truncate(self, kept_);
    Despawns___release(owner_);
}

int64_t Naive_kept_sum(Naive* self) {
    int64_t sum_ = SpiteInteger_to_long(0);
    int32_t row_ = 0;
    while (((row_ < Items__Velocity_count(self->velocities_)))) {
        Velocity* velocity_ = ({ Velocity* spite_temp_5 = Items__Velocity_get_at(self->velocities_, row_); if (__builtin_expect(!(((spite_temp_5) != 0)), 0)) spite_outside_list("velocities[row]", spite_site_2()); spite_temp_5; });
        if (!(((List_Integer_get_at(self->entities_, row_)).has_value))) {
            spite_failed_1(row_, self, sum_);
        }
        int32_t entity_ = (List_Integer_get_at(self->entities_, row_)).value;
        sum_ = ({ int64_t spite_temp_6 = ({ int64_t spite_temp_7 = sum_; int64_t spite_temp_8 = SpiteInteger_to_long((velocity_)->entity_); int64_t spite_temp_9; if (__builtin_expect(__builtin_add_overflow(spite_temp_7, spite_temp_8, &spite_temp_9), 0)) spite_overflowed("sum + velocity.entity", "a Long", "+", (int64_t)spite_temp_7, (int64_t)spite_temp_8, spite_site_3()); spite_temp_9; }); int64_t spite_temp_10 = SpiteInteger_to_long(entity_); int64_t spite_temp_11; if (__builtin_expect(__builtin_add_overflow(spite_temp_6, spite_temp_10, &spite_temp_11), 0)) spite_overflowed("sum + velocity.entity + entity", "a Long", "+", (int64_t)spite_temp_6, (int64_t)spite_temp_10, spite_site_3()); spite_temp_11; });
        row_ = (row_ + 1);
    }
    int64_t spite_temp_12 = sum_;
    return spite_temp_12;
}
