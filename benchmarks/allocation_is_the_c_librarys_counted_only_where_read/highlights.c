/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

#define SPITE_MALLOC(size) spite_host_realloc(0, size)
#define SPITE_MALLOC(size) malloc(size)

#define SPITE_FREE(pointer) spite_host_free(pointer)
#define SPITE_FREE(pointer) free(pointer)

Node* Node___allocate(void) {
    Node* self = Node___pool_take();
    self->header.ref_count = 1;
    self->header.class_id = 112;
    Node___init(self);
    #ifdef SPITE_TRACKS_Node
    spite_track_Node(self);
    #endif
    return self;
}

void Node___free(Node* self) {
    Node___release(self->next_);
    #ifdef SPITE_TRACKS_Node
    spite_untrack_Node(self);
    #endif
    #ifdef SPITE_WEAK_Node
    spite_weak_object_freed(self);
    #endif
    Node___pool_give(self);
}
