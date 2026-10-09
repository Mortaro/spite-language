/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

struct Entity {
    SpiteHeader header;
    int32_t x_;
    int32_t y_;
    Velocity* velocity_;
    Health* health_;
    Burning* burning_;
};

int64_t Entity_burn(Entity* self) {
    if ((((self->health_) != 0)) && (((self->burning_) != 0))) {
        Health* spite_temp_1 = self->health_;
        (spite_temp_1)->points_ = ({ int32_t spite_temp_2 = (self->health_)->points_; int32_t spite_temp_3 = (self->burning_)->damage_; int32_t spite_temp_4; if (__builtin_expect(__builtin_sub_overflow(spite_temp_2, spite_temp_3, &spite_temp_4), 0)) spite_overflowed("health.points - burning.damage", "an Integer", "-", (int64_t)spite_temp_2, (int64_t)spite_temp_3, spite_site_1()); spite_temp_4; });
        int64_t spite_temp_5 = SpiteInteger_to_long((self->burning_)->damage_);
        return spite_temp_5;
    }
    int64_t spite_temp_6 = SpiteInteger_to_long(0);
    return spite_temp_6;
}

void Entity_cool(Entity* self) {
    if (!(((self->burning_) != 0))) {
        SPITE_TRACE_ASSERT(spite_site_2());
        return;
    }
    Burning* spite_temp_7 = self->burning_;
    (spite_temp_7)->ticks_ = ({ int32_t spite_temp_8 = (self->burning_)->ticks_; int32_t spite_temp_9 = 1; int32_t spite_temp_10; if (__builtin_expect(__builtin_sub_overflow(spite_temp_8, spite_temp_9, &spite_temp_10), 0)) spite_overflowed("burning.ticks - 1", "an Integer", "-", (int64_t)spite_temp_8, (int64_t)spite_temp_9, spite_site_3()); spite_temp_10; });
    if ((((self->burning_)->ticks_ == 0))) {
        Burning* spite_temp_11 = 0;
        Burning___release(self->burning_);
        self->burning_ = spite_temp_11;
    }
}

int64_t List_Entity_sum_burn(List_Entity* self) {
    int64_t total_ = SpiteInteger_to_long(0);
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        Entity* item_ = ((Entity**)(intptr_t)self->items_)[index_];
        total_ = ({ int64_t spite_temp_12 = total_; int64_t spite_temp_13 = Entity_burn(item_); int64_t spite_temp_14; if (__builtin_expect(__builtin_add_overflow(spite_temp_12, spite_temp_13, &spite_temp_14), 0)) spite_overflowed("total + item.attributes[member]", "a Long", "+", (int64_t)spite_temp_12, (int64_t)spite_temp_13, spite_site_4()); spite_temp_14; });
        index_ = (index_ + 1);
    }
    int64_t spite_temp_15 = total_;
    return spite_temp_15;
}
