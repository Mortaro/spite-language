/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void Naive_Naive(Naive* self) {
    List_Monster* monsters_ = List_Monster___make();
    int32_t index_ = 0;
    while (((index_ < 1000))) {
        Monster* monster_ = Monster___make((index_ % 50));
        List_Monster_append(monsters_, Monster___retain(monster_));
        index_ = (index_ + 1);
        Monster___release(monster_);
    }
    int32_t total_ = Naive_total_health___held_0(self, monsters_);
    List_Console_Printable spite_framed_1; Console_Printable spite_framed_1_items[2]; int32_t spite_framed_1_count = 0;
    Console_print(self->console_, ({ spite_framed_1_items[0] = spite_tagged_object(0, ((void*)&spite_lit_1_box)); spite_framed_1_items[1] = spite_tagged_SpiteInteger(total_); spite_framed_1_count = 2; List_Console_Printable___framed(&spite_framed_1, (int64_t)(intptr_t)spite_framed_1_items, 2); }));
    for (int32_t spite_index = 0; spite_index < spite_framed_1_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_1_items[spite_index]); }
    List_Monster___release(monsters_);
}

int32_t Naive_total_health___held_0(Naive* self, List_Monster* monsters_) {
    int32_t total_ = 0;
    int32_t round_ = 0;
    while (((round_ < 10))) {
        total_ = ({ int32_t spite_temp_1 = total_; int32_t spite_temp_2 = List_Monster_sum_health(monsters_); int32_t spite_temp_3; if (__builtin_expect(__builtin_add_overflow(spite_temp_1, spite_temp_2, &spite_temp_3), 0)) spite_overflowed("total + monsters.sum_health()", "an Integer", "+", (int64_t)spite_temp_1, (int64_t)spite_temp_2, spite_site_1()); spite_temp_3; });
        round_ = (round_ + 1);
    }
    int32_t spite_temp_4 = total_;
    return spite_temp_4;
}
