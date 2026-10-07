/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

#define SPITE_TRACKS_Lamp

static Lamp** spite_instances_Lamp = 0;

void spite_track_Lamp(Lamp* self) {
    if (spite_instances_Lamp_count == spite_instances_Lamp_capacity) {
        spite_instances_Lamp_capacity = spite_instances_Lamp_capacity == 0 ? 8 : spite_instances_Lamp_capacity * 2;
        spite_instances_Lamp = (Lamp**)realloc(spite_instances_Lamp, (size_t)spite_instances_Lamp_capacity * sizeof(Lamp*));
    }
    spite_instances_Lamp[spite_instances_Lamp_count] = self;
    spite_instances_Lamp_count = spite_instances_Lamp_count + 1;
}

Lamp* Lamp___allocate(void) {
    Lamp* self = Lamp___pool_take();
    self->header.ref_count = 1;
    self->header.class_id = 110;
    Lamp___init(self);
    #ifdef SPITE_TRACKS_Lamp
    spite_track_Lamp(self);
    #endif
    return self;
}

Door* Door___allocate(void) {
    Door* self = Door___pool_take();
    self->header.ref_count = 1;
    self->header.class_id = 109;
    Door___init(self);
    #ifdef SPITE_TRACKS_Door
    spite_track_Door(self);
    #endif
    return self;
}

List_Lamp* Lamp___instances(void) {
    List_Lamp* result = List_Lamp___make();
    for (int64_t spite_index = 0; spite_index < spite_instances_Lamp_count; spite_index = spite_index + 1) { List_Lamp_append(result, Lamp___retain(spite_instances_Lamp[spite_index])); }
    return result;
}
