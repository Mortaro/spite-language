/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int32_t Hero_damage(Hero* self) {
    int32_t spite_temp_1 = ({ int32_t spite_temp_2 = self->strength_; int32_t spite_temp_3 = 1; int32_t spite_temp_4; if (__builtin_expect(__builtin_add_overflow(spite_temp_2, spite_temp_3, &spite_temp_4), 0)) spite_overflowed("strength + 1", "an Integer", "+", (int64_t)spite_temp_2, (int64_t)spite_temp_3, spite_site_1()); spite_temp_4; });
    return spite_temp_1;
}

void Arena_strike_all___held_0_1(Arena* self, List_Monster* monsters_, List_Hero* heroes_) {
    int32_t index_ = 0;
    while (((index_ < spite_folded_List_Monster_count(monsters_)))) {
        Monster* monster_ = ({ List_Monster* spite_temp_5 = monsters_; int32_t spite_temp_6 = index_; if (__builtin_expect(spite_temp_6 < 0 || spite_temp_6 >= (spite_temp_5)->item_count_, 0)) spite_outside_list("monsters[index]", spite_site_2()); ((Monster**)(intptr_t)(spite_temp_5)->items_)[spite_temp_6]; });
        Hero* hero_ = ({ List_Hero* spite_temp_7 = heroes_; int32_t spite_temp_8 = index_; (spite_temp_8 < 0 || spite_temp_8 >= (spite_temp_7)->item_count_) ? 0 : ((Hero**)(intptr_t)(spite_temp_7)->items_)[spite_temp_8]; });
        if (!(((hero_) != 0))) {
            spite_failed_1(index_);
        }
        int32_t amount_ = Hero_damage(hero_);
        Monster_hurt(monster_, amount_);
        index_ = (index_ + 1);
    }
}
