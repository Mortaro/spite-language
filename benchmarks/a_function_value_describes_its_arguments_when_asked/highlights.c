/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

Spite_Function* spite_function_value_Scorer_score(Scorer* owner) {
    Spite_Function* described = Spite_Function___make(spite_symbol_1, spite_class_object_Integer());
    described->spite_add_arguments = spite_function_value_Scorer_score___arguments;
    described->spite_owner = Scorer___retain(owner);
    described->spite_release_owner = (void (*)(void*))Scorer___release;
    described->spite_typed_call = (void*)Scorer_score;
    return described;
}

static void spite_function_value_Scorer_score___arguments(Spite_Function* described) {
    List_Spite_Argument_append(described->_arguments_, Spite_Argument___make(spite_symbol_2, spite_class_object_Integer()));
}

static void spite_function_arguments_described(Spite_Function* self) {
    if (SPITE_SINGLETON_FOUND(self->spite_add_arguments) == 0) return;
    SPITE_LOCK(self->spite_arguments_lock);
    void (*adding)(Spite_Function*) = self->spite_add_arguments;
    if (adding != 0) {
        adding(self);
        SPITE_SINGLETON_PUBLISH(self->spite_add_arguments, (void (*)(Spite_Function*))0);
    }
    SPITE_UNLOCK(self->spite_arguments_lock);
}

List_Spite_Argument* Spite_Function_get_arguments(Spite_Function* self) {
    spite_function_arguments_described(self);
    List_Spite_Argument* spite_temp_1 = List_Spite_Argument___retain(self->_arguments_);
    return spite_temp_1;
}

int64_t Naive_apply_all___held_0(Naive* self, Scorer* scorer_, int32_t count_) {
    int64_t applied_ = SpiteInteger_to_long(0);
    int32_t index_ = 0;
    while (((index_ < count_))) {
        int32_t scored_ = Naive_apply___held_0(self, &(Spite_Function){ .spite_owner = (void*)(scorer_), .spite_typed_call = (void*)Scorer_score }, (index_ % 100));
        applied_ = ({ int64_t spite_temp_2 = applied_; int64_t spite_temp_3 = SpiteInteger_to_long(scored_); int64_t spite_temp_4; if (__builtin_expect(__builtin_add_overflow(spite_temp_2, spite_temp_3, &spite_temp_4), 0)) spite_overflowed("applied + scored", "a Long", "+", (int64_t)spite_temp_2, (int64_t)spite_temp_3, spite_site_1()); spite_temp_4; });
        index_ = (index_ + 1);
    }
    int64_t spite_temp_5 = applied_;
    return spite_temp_5;
}

int32_t Naive_apply___held_0(Naive* self, Spite_Function* change_, int32_t value_) {
    int32_t spite_temp_6 = ({ Spite_Function* spite_temp_7 = change_; int32_t spite_temp_8 = ((int32_t (*)(void*, int32_t))spite_temp_7->spite_typed_call)(spite_temp_7->spite_owner, value_); spite_temp_8; });
    return spite_temp_6;
}
