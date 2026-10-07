/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

SpiteString Naive_words(Naive* self, int32_t count_) {
    SpiteString text_ = spite_lit_1;
    SpiteString piece_ = spite_lit_2;
    int32_t index_ = 0;
    while (((index_ < count_))) {
        SpiteString spite_temp_1 = piece_;
        text_ = SpiteString_append(text_, spite_temp_1);
        index_ = (index_ + 1);
    }
    SpiteString spite_temp_2 = SpiteString___retain(text_);
    SpiteString___release(piece_);
    SpiteString___release(text_);
    return spite_temp_2;
}

SpiteString SpiteString_append(SpiteString left, SpiteString right) {
    int64_t left_length = spite_string_length(left);
    int64_t right_length = spite_string_length(right);
    int64_t total = left_length + right_length;
    const char* right_bytes = spite_string_bytes(&right);
    if (SPITE_STRING_IS_INLINE(left) && total <= SPITE_STRING_INLINE) {
        if (right_length > 0) memcpy((char*)&left + left_length, right_bytes, (size_t)right_length);
        ((char*)&left)[15] = (char)(SPITE_STRING_INLINE - total);
        return left;
    }
    if (SPITE_STRING_IS_HEAP(left) && left._bytes_ != right._bytes_) {
        SpiteStringBlock* block = SPITE_STRING_BLOCK(left);
        if (block->header.ref_count == 1) {
            if (total > block->capacity) {
                int64_t capacity = block->capacity * 2;
                if (capacity < total) capacity = total;
                size_t size = ((size_t)capacity + 17 + 15) & ~(size_t)15;
                block = (SpiteStringBlock*)SPITE_REALLOC(block, size);
                block->capacity = (int64_t)size - 17;
            }
            memcpy(block->bytes + left_length, right_bytes, (size_t)right_length);
            return spite_string_held(block, total);
        }
    }
    int64_t capacity = left_length * 2;
    if (capacity < total) capacity = total;
    SpiteStringBlock* block = spite_string_block(capacity);
    if (left_length > 0) memcpy(block->bytes, spite_string_bytes(&left), (size_t)left_length);
    if (right_length > 0) memcpy(block->bytes + left_length, right_bytes, (size_t)right_length);
    SpiteString___release(left);
    return spite_string_held(block, total);
}
