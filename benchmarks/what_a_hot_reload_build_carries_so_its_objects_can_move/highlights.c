/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

struct Particle {
    SpiteHeader header;
    int32_t position_;
    int32_t speed_;
};

Particle* Particle___allocate(void) {
    Particle* self = Particle___pool_take();
    self->header.ref_count = 1;
    self->header.class_id = 112;
    Particle___init(self);
    #ifdef SPITE_TRACKS_Particle
    spite_track_Particle(self);
    #endif
    return self;
}

void List_Particle_each_step(List_Particle* self) {
    int32_t index_ = 0;
    while (((index_ < self->item_count_))) {
        Particle* item_ = ((Particle**)(intptr_t)self->items_)[index_];
        Particle_step(item_);
        index_ = (index_ + 1);
    }
}
