/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

void Naive_turn(Naive* self, Point* point_) {
    float turned_across_ = (((point_)->across_ * (0x1.c152800000000p-1f)) - ((point_)->down_ * (0x1.eaee880000000p-2f)));
    float turned_down_ = (((point_)->across_ * (0x1.eaee880000000p-2f)) + ((point_)->down_ * (0x1.c152800000000p-1f)));
    (point_)->across_ = turned_across_;
    (point_)->down_ = turned_down_;
    Point___release(point_);
}

void List_Point_each_turn_for_naive(List_Point* self, Naive* owner_) {
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        Point* item_ = ((Point**)(intptr_t)self->items_)[index_];
        Naive_turn(owner_, Point___retain(item_));
        index_ = (index_ + 1);
    }
    Naive___release(owner_);
}
