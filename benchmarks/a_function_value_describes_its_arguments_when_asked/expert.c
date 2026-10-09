/* The same work tuned by hand: the function value is a function pointer and its owner, two words on the stack, and
 * its description is a constant the program reads only when asked. The C compiler sees which function the pointer
 * holds and calls it directly. */
#include <stdint.h>
#include <stdio.h>
#include "../clock.h"

typedef struct Scorer {
    int32_t bonus;
} Scorer;

typedef struct Argument {
    const char* name;
    const char* class_name;
} Argument;

typedef struct FunctionValue {
    int32_t (*call)(void*, int32_t);
    void* owner;
} FunctionValue;

static const Argument score_arguments[] = {{"value", "Integer"}};

static int32_t scorer_score(void* owner, int32_t value) {
    Scorer* scorer = owner;
    return value * 2 + scorer->bonus;
}

int main(void) {
    int64_t start = now_nanoseconds();
    Scorer scorer = {3};
    int64_t applied = 0;
    for (int32_t index = 0; index < 2000000; index++) {
        FunctionValue change = {scorer_score, &scorer};
        applied += change.call(change.owner, index % 100);
    }
    int64_t microseconds = microseconds_since(start);
    int32_t argument_count = (int32_t)(sizeof(score_arguments) / sizeof(score_arguments[0]));
    printf("applied %lld arguments %d\n", (long long)applied, argument_count);
    print_microseconds(microseconds);
    return 0;
}
