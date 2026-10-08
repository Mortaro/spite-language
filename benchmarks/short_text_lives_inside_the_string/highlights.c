/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

struct SpiteString {
    int64_t _bytes_;
    int64_t _length_;
};

#define SPITE_STRING_INLINE 15
#define SPITE_STRING_HEAP ((int64_t)0x4000000000000000)
#define SPITE_STRING_FORM(text) ((uint64_t)(text)._length_ >> 56)
#define SPITE_STRING_IS_INLINE(text) (SPITE_STRING_FORM(text) <= SPITE_STRING_INLINE)
#define SPITE_STRING_IS_HEAP(text) (SPITE_STRING_FORM(text) == 0x40)
#define SPITE_STRING_IS_CONSTANT(text) (SPITE_STRING_FORM(text) == 0x80)
#define SPITE_STRING_BLOCK(text) ((SpiteStringBlock*)(intptr_t)((text)._bytes_ - 16))
#define SPITE_STRING_NULL ((SpiteString){ 0, INT64_MIN })
#define SPITE_STRING_EMPTY ((SpiteString){ 0, (int64_t)SPITE_STRING_INLINE << 56 })
#define SPITE_STRING_IS_NULL(text) ((text)._bytes_ == 0 && (text)._length_ == INT64_MIN)

static inline int64_t spite_string_length(SpiteString text) {
    uint64_t form = (uint64_t)text._length_ >> 56;
    if (form <= SPITE_STRING_INLINE) return (int64_t)(SPITE_STRING_INLINE - form);
    return (int64_t)((uint64_t)text._length_ & 0x00FFFFFFFFFFFFFF);
}

SpiteString spite_string_join(int32_t count, const SpiteString* pieces) {
    int64_t total = 0;
    for (int32_t index = 0; index < count; index++) total += spite_string_length(pieces[index]);
    SpiteString made = { 0, 0 };
    SpiteStringBlock* block = 0;
    char* at = (char*)&made;
    if (total > SPITE_STRING_INLINE) { block = spite_string_block(total); at = block->bytes; }
    for (int32_t index = 0; index < count; index++) {
        int64_t length = spite_string_length(pieces[index]);
        if (length > 0) memcpy(at, spite_string_bytes(&pieces[index]), (size_t)length);
        at += length;
    }
    if (block != 0) return spite_string_held(block, total);
    ((char*)&made)[15] = (char)(SPITE_STRING_INLINE - total);
    return made;
}

void SpiteString___release(SpiteString self) {
    if (!SPITE_STRING_IS_HEAP(self)) return;
    SpiteStringBlock* block = SPITE_STRING_BLOCK(self);
    if (SPITE_COUNT_DOWN(block->header.ref_count) > 0) return;
    SPITE_FREE(block);
}

List_String* Naive_make_labels(Naive* self, int32_t count_, int32_t round_) {
    List_String* labels_ = List_String___make();
    int32_t index_ = 0;
    while (((index_ < count_))) {
        List_String_append(labels_, ({ char spite_temp_1_digits[24]; SpiteString spite_temp_1 = SPITE_STATIC_STRING(spite_temp_1_digits, spite_long_digits(spite_temp_1_digits, (int64_t)(round_))); char spite_temp_2_digits[24]; SpiteString spite_temp_2 = SPITE_STATIC_STRING(spite_temp_2_digits, spite_long_digits(spite_temp_2_digits, (int64_t)(index_))); SpiteString spite_temp_3[] = {spite_lit_1, spite_temp_1, spite_lit_2, spite_temp_2}; SpiteString spite_temp_4 = spite_string_join(4, spite_temp_3); spite_temp_4; }));
        index_ = (index_ + 1);
    }
    List_String* spite_temp_5 = List_String___retain(labels_);
    List_String___release(labels_);
    return spite_temp_5;
}

int32_t Naive_checksum___held_0(Naive* self, List_String* labels_) {
    int32_t total_ = 0;
    int32_t index_ = 0;
    while (((index_ < List_String_count(labels_)))) {
        SpiteString label_ = ({ SpiteString spite_temp_6 = List_String_get_at(labels_, index_); if (__builtin_expect(!((!SPITE_STRING_IS_NULL(spite_temp_6))), 0)) spite_outside_list("labels[index]", spite_site_1()); spite_temp_6; });
        int32_t last_ = (SpiteString_length(label_) - 1);
        total_ = ({ int32_t spite_temp_7 = ({ int32_t spite_temp_8 = total_; int32_t spite_temp_9 = SpiteString_length(label_); int32_t spite_temp_10; if (__builtin_expect(__builtin_add_overflow(spite_temp_8, spite_temp_9, &spite_temp_10), 0)) spite_overflowed("total + label.length()", "an Integer", "+", (int64_t)spite_temp_8, (int64_t)spite_temp_9, spite_site_2()); spite_temp_10; }); int32_t spite_temp_11 = SpiteString_code_at(label_, last_); int32_t spite_temp_12; if (__builtin_expect(__builtin_add_overflow(spite_temp_7, spite_temp_11, &spite_temp_12), 0)) spite_overflowed("total + label.length() + label.code_at(last)", "an Integer", "+", (int64_t)spite_temp_7, (int64_t)spite_temp_11, spite_site_2()); spite_temp_12; });
        index_ = (index_ + 1);
        SpiteString___release(label_);
    }
    int32_t spite_temp_13 = total_;
    return spite_temp_13;
}
