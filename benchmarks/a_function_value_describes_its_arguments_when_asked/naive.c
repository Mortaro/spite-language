/* naive/ written in C the way it reads: `scorer.score` is a function value, an object that holds the function, its
 * owner and its own description (its name, what it returns, and the list of its arguments, each with its name and
 * class), all made when the value is made, as the Spite says a function value is; apply calls through it, and it is
 * freed after the call. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Scorer {
    int32_t bonus;
} Scorer;

typedef struct Argument {
    const char* name;
    const char* class_name;
} Argument;

typedef struct ArgumentList {
    Argument** items;
    int32_t count;
    int32_t capacity;
} ArgumentList;

typedef struct FunctionValue {
    const char* name;
    const char* returns;
    ArgumentList* arguments;
    int32_t (*call)(void*, int32_t);
    void* owner;
} FunctionValue;

static void arguments_append(ArgumentList* list, Argument* item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(Argument*) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static int32_t scorer_score(void* owner, int32_t value) {
    Scorer* scorer = owner;
    return value * 2 + scorer->bonus;
}

static FunctionValue* scorer_score_value(Scorer* scorer) {
    FunctionValue* function = malloc(sizeof(FunctionValue));
    function->name = "score";
    function->returns = "Integer";
    function->arguments = calloc(1, sizeof(ArgumentList));
    Argument* argument = malloc(sizeof(Argument));
    argument->name = "value";
    argument->class_name = "Integer";
    arguments_append(function->arguments, argument);
    function->call = scorer_score;
    function->owner = scorer;
    return function;
}

static void function_value_free(FunctionValue* function) {
    for (int32_t index = 0; index < function->arguments->count; index = index + 1) free(function->arguments->items[index]);
    free(function->arguments->items);
    free(function->arguments);
    free(function);
}

static int32_t apply(FunctionValue* change, int32_t value) {
    return change->call(change->owner, value);
}

static int64_t apply_all(Scorer* scorer, int32_t count) {
    int64_t applied = 0;
    for (int32_t index = 0; index < count; index = index + 1) {
        FunctionValue* change = scorer_score_value(scorer);
        int32_t scored = apply(change, index % 100);
        function_value_free(change);
        applied = applied + scored;
    }
    return applied;
}

int main(void) {
    int64_t start = now_nanoseconds();
    Scorer* scorer = malloc(sizeof(Scorer));
    scorer->bonus = 3;
    int64_t applied = apply_all(scorer, 2000000);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    FunctionValue* scoring = scorer_score_value(scorer);
    int32_t argument_count = scoring->arguments->count;
    printf("applied %lld arguments %d\n", (long long)applied, argument_count);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    function_value_free(scoring);
    free(scorer);
    return 0;
}
