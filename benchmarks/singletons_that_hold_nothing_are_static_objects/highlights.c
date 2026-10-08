/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

Memory_Heap* spite_singleton_Memory_Heap(void) {
    static Memory_Heap spite_object = { { 1, 94 } };
    return &spite_object;
}

TypedMemory__Long* spite_singleton_TypedMemory__Long(void) {
    static TypedMemory__Long spite_object = { { 1, 119 } };
    return &spite_object;
}

Build* spite_singleton_Build(void) {
    static Build spite_object = { { 1, 12 } };
    return &spite_object;
}

void List_Long___init(List_Long* self) {
    self->heap_ = spite_singleton_Memory_Heap();
    self->values_ = spite_singleton_TypedMemory__Long();
    self->items_ = ((int64_t)(0));
    self->item_count_ = 0;
    self->capacity_ = 0;
}

void Memory_Heap___release(Memory_Heap* self) { (void)self; }
