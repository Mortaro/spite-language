/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

#define SPITE_TAGGED_NULL ((SpiteTagged){ 0, 0, { 0 } })
#define SPITE_TAGGED_PRESENT(tagged) ((tagged).plain != 0 || (tagged).value.object != 0)
#define SPITE_TAGGED_VALUE(tagged, type) ({ SpiteTagged spite_tagged_read = (tagged); type spite_tagged_value; memcpy(&spite_tagged_value, &spite_tagged_read.value, sizeof(type)); spite_tagged_value; })

static inline SpiteTagged spite_tagged_SpiteInteger(int32_t value) {
    SpiteTagged tagged;
    tagged.tag = 167;
    tagged.plain = 1;
    tagged.value.bits = 0;
    memcpy(&tagged.value, &value, sizeof(value));
    return tagged;
}

void* spite_box_SpiteString(SpiteString value) {
    SpiteBox_SpiteString* self = (SpiteBox_SpiteString*)SPITE_MALLOC(sizeof(SpiteBox_SpiteString));
    self->header.ref_count = 1;
    self->header.class_id = 0;
    self->value = value;
    return self;
}

void Naive_add_value___held_0(Naive* self, List_Console_Printable* values_, int32_t index_) {
    int32_t kind_ = (index_ % 3);
    if (((kind_ == 0))) {
        List_Console_Printable_append(values_, spite_tagged_SpiteInteger(index_));
    }
    else {
        if (((kind_ == 1))) {
            List_Console_Printable_append(values_, spite_tagged_SpiteBoolean(((index_ % 2) == 0)));
        }
        else {
            List_Console_Printable_append(values_, spite_tagged_object(0, spite_box_SpiteString(({ char spite_temp_1_digits[24]; SpiteString spite_temp_1 = SPITE_STATIC_STRING(spite_temp_1_digits, spite_long_digits(spite_temp_1_digits, (int64_t)(index_))); SpiteString spite_temp_2[] = {spite_lit_1, spite_temp_1}; SpiteString spite_temp_3 = spite_string_join(2, spite_temp_2); spite_temp_3; }))));
        }
    }
}

SpiteString Console_Printable___call_to_string(Console_Printable self) {
    if (((self).tag == 0) && ((self).plain == 0)) return SpiteString_to_string((((SpiteBox_SpiteString*)(self).value.object)->value));
    if ((self).tag == 151) return SpiteLong_to_string(SPITE_TAGGED_VALUE(self, int64_t));
    if ((self).tag == 167) return SpiteInteger_to_string(SPITE_TAGGED_VALUE(self, int32_t));
    if ((self).tag == 168) return SpiteBoolean_to_string(SPITE_TAGGED_VALUE(self, bool));
    fputs("spite.crash\tPrintable.to_string was called on a value of a class it was not compiled for\n", stderr);
    abort();
}

int32_t Naive_measure___held_0(Naive* self, List_Console_Printable* values_) {
    int32_t total_ = 0;
    int32_t index_ = 0;
    while (((index_ < List_Console_Printable_count(values_)))) {
        Console_Printable value_ = ({ Console_Printable spite_temp_4 = List_Console_Printable_get_at(values_, index_); if (__builtin_expect(!(SPITE_TAGGED_PRESENT(spite_temp_4)), 0)) spite_outside_list("values[index]", spite_site_1()); spite_temp_4; });
        SpiteString text_ = Console_Printable___call_to_string(value_);
        total_ = ({ int32_t spite_temp_5 = total_; int32_t spite_temp_6 = SpiteString_length(text_); int32_t spite_temp_7; if (__builtin_expect(__builtin_add_overflow(spite_temp_5, spite_temp_6, &spite_temp_7), 0)) spite_overflowed("total + text.length()", "an Integer", "+", (int64_t)spite_temp_5, (int64_t)spite_temp_6, spite_site_2()); spite_temp_7; });
        index_ = (index_ + 1);
        SpiteString___release(text_);
        Console_Printable___release(value_);
    }
    int32_t spite_temp_8 = total_;
    return spite_temp_8;
}
