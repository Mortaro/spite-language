/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

struct Particle {
    SpiteHeader header;
    void* spite_allocator;
    void (*spite_give_back)(void*, void*);
    int32_t position_;
    int32_t speed_;
};

int32_t Naive_one_round(Naive* self, int32_t round_) {
    Memory_Arena* arena_ = Memory_Arena___make(SpiteInteger_to_long(65536));
    Memory_Arena* spite_temp_1 = Memory_Arena___retain(arena_);
    List_Particle* particles_ = List_Particle___make_in(spite_temp_1, spite_give_back_Memory_Arena, Memory_Arena_allocate(spite_temp_1, (int64_t)sizeof(List_Particle)));
    int32_t index_ = 0;
    while (((index_ < 10000))) {
        Memory_Arena* spite_temp_2 = Memory_Arena___retain(arena_);
        Particle* particle_ = Particle___make_in(spite_temp_2, spite_give_back_Memory_Arena, Memory_Arena_allocate(spite_temp_2, (int64_t)sizeof(Particle)), ({ int32_t spite_temp_3 = index_; int32_t spite_temp_4 = round_; int32_t spite_temp_5; if (__builtin_expect(__builtin_add_overflow(spite_temp_3, spite_temp_4, &spite_temp_5), 0)) spite_overflowed("index + round", "an Integer", "+", (int64_t)spite_temp_3, (int64_t)spite_temp_4, spite_site_1()); spite_temp_5; }), (index_ % 5));
        List_Particle_append(particles_, Particle___retain(particle_));
        index_ = (index_ + 1);
        Particle___release(particle_);
    }
    int32_t spite_temp_6 = ({ int32_t spite_temp_7 = List_Particle_sum_position(particles_); int32_t spite_temp_8 = List_Particle_sum_speed(particles_); int32_t spite_temp_9; if (__builtin_expect(__builtin_add_overflow(spite_temp_7, spite_temp_8, &spite_temp_9), 0)) spite_overflowed("particles.sum_position() + particles.sum_speed()", "an Integer", "+", (int64_t)spite_temp_7, (int64_t)spite_temp_8, spite_site_2()); spite_temp_9; });
    List_Particle___release(particles_);
    Memory_Arena___release(arena_);
    return spite_temp_6;
}

Particle* Particle___make_in(void* spite_allocator, void (*spite_give_back)(void*, void*), int64_t spite_address, int32_t starting_position_, int32_t starting_speed_) {
    Particle* self = (Particle*)(intptr_t)spite_address;
    self->header.ref_count = 1;
    self->header.class_id = 109;
    Particle___init(self);
    self->spite_allocator = spite_allocator;
    self->spite_give_back = spite_give_back;
    #ifdef SPITE_TRACKS_Particle
    spite_track_Particle(self);
    #endif
    Particle_Particle(self, starting_position_, starting_speed_);
    return self;
}
