/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

int main(int argument_count, char** argument_values) {
    spite_fault_install();
    spite_program_arguments = (SpiteArguments){ .count = argument_count - 1, .items = (const char**)(argument_values + 1) };
    #ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
    #endif
    Launcher* spite_launcher = Launcher___allocate();
    Launcher_Launcher(spite_launcher);
    Launcher___release(spite_launcher);
    spite_singletons_destroy();
    
    
    
    if (spite_foreign_library_1_tracked) DynamicLibrary___destroy(spite_foreign_library_1_cache);
    
    
    
    return 0;
}

void Naive_Naive(Naive* self) {
    SpiteString spite_framed_1_items[3];
    List_String spite_framed_1;
    List_String* names_ = List_String___framed(&spite_framed_1, (int64_t)(intptr_t)spite_framed_1_items, 0);
    spite_framed_1.capacity_ = 3;
    List_String_append(names_, spite_lit_1);
    List_String_append(names_, spite_lit_2);
    List_String_append(names_, spite_lit_3);
    SpiteString joined_ = List_String_join(names_, spite_lit_4);
    int32_t longest_ = Naive_longest_length___held_0(self, names_);
    List_Console_Printable spite_framed_2; Console_Printable spite_framed_2_items[2]; int32_t spite_framed_2_count = 0;
    Console_print(self->console_, ({ spite_framed_2_items[0] = spite_tagged_object(0, spite_box_SpiteString(SpiteString___retain(joined_))); spite_framed_2_items[1] = spite_tagged_SpiteInteger(longest_); spite_framed_2_count = 2; List_Console_Printable___framed(&spite_framed_2, (int64_t)(intptr_t)spite_framed_2_items, 2); }));
    for (int32_t spite_index = 0; spite_index < spite_framed_2_count; spite_index = spite_index + 1) { Console_Printable___release(spite_framed_2_items[spite_index]); }
    SpiteString___release(joined_);
    List_String_clear(names_);
}

int32_t Naive_longest_length___held_0(Naive* self, List_String* names_) {
    int32_t longest_ = 0;
    int32_t index_ = 0;
    while (((index_ < List_String_count(names_)))) {
        SpiteString name_ = ({ SpiteString spite_temp_1 = List_String_get_at(names_, index_); if (__builtin_expect(!((!SPITE_STRING_IS_NULL(spite_temp_1))), 0)) spite_outside_list("names[index]", spite_site_1()); spite_temp_1; });
        int32_t length_ = SpiteString_length(name_);
        longest_ = SpiteInteger_maximum(longest_, length_);
        index_ = (index_ + 1);
        SpiteString___release(name_);
    }
    int32_t spite_temp_2 = longest_;
    return spite_temp_2;
}
