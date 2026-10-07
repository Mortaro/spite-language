/* What the compiler writes from naive/ for this case, as an --optimized build does: only the definitions
 * functions.txt names, copied out by scripts/cases/extract.sh, indented, and with the compiler's own numbered
 * names numbered again from 1. check.sh compares it with what the compiler writes now. */

#define BinaryWriter__Reading_schema(self) ((void)(self), (int64_t)(4647632506827894408LL)) /* Reading{sensor:Integer;level:Float;label:String} */

#define BinaryReader__Reading_schema(self) ((void)(self), (int64_t)(4647632506827894408LL)) /* Reading{sensor:Integer;level:Float;label:String} */

bool Naive_is_current(Naive* self, int64_t header_) {
    bool spite_temp_1 = (header_ == BinaryReader__Reading_schema(self->reader_));
    return spite_temp_1;
}

List_Long* Naive_received_headers(Naive* self, int32_t count_) {
    List_Long* headers_ = List_Long___make();
    int32_t index_ = 0;
    while (((index_ < count_))) {
        int64_t header_ = BinaryWriter__Reading_schema(self->writer_);
        if ((((index_ % 10) == 0))) {
            header_ = SpiteInteger_to_long(index_);
        }
        List_Long_append(headers_, header_);
        index_ = (index_ + 1);
    }
    List_Long* spite_temp_2 = List_Long___retain(headers_);
    List_Long___release(headers_);
    return spite_temp_2;
}
