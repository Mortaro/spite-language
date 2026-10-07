/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

static void Item___init_constructed(Item* self) {
    self->price_ = 0;
    self->owner_ = 0;
}

static Item* Item___allocate_constructed(void) {
    Item* self = Item___pool_take();
    self->header.ref_count = 1;
    self->header.class_id = 109;
    Item___init_constructed(self);
    #ifdef SPITE_TRACKS_Item
    spite_track_Item(self);
    #endif
    return self;
}

void Item_Item(Item* self, int32_t new_price_, Owner* new_owner_) {
    self->price_ = new_price_;
    Owner* spite_temp_1 = Owner___retain(new_owner_);
    Owner___release(self->owner_);
    self->owner_ = spite_temp_1;
    Owner___release(new_owner_);
}
