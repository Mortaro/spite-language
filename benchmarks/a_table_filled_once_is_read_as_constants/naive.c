/* naive/ written in C the way it reads: a growable array of step codes, filled once when the checkout is made, and
 * a price that walks it and tests each step against every kind of step there is, as the Spite does. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

enum Step { STEP_DISCOUNT, STEP_TAX, STEP_ROUNDING, STEP_COUPON };

typedef struct Pipeline {
    int32_t* steps;
    int32_t count;
    int32_t capacity;
} Pipeline;

static Pipeline* pipeline_make(void) {
    Pipeline* pipeline = malloc(sizeof(Pipeline));
    pipeline->steps = NULL;
    pipeline->count = 0;
    pipeline->capacity = 0;
    return pipeline;
}

static void pipeline_add(Pipeline* pipeline, int32_t step);

static Pipeline* checkout_make(void) {
    Pipeline* checkout = pipeline_make();
    pipeline_add(checkout, STEP_DISCOUNT);
    pipeline_add(checkout, STEP_TAX);
    pipeline_add(checkout, STEP_DISCOUNT);
    return checkout;
}

static void pipeline_add(Pipeline* pipeline, int32_t step) {
    if (pipeline->count == pipeline->capacity) {
        pipeline->capacity = pipeline->capacity == 0 ? 4 : pipeline->capacity * 2;
        pipeline->steps = realloc(pipeline->steps, sizeof(int32_t) * pipeline->capacity);
    }
    pipeline->steps[pipeline->count] = step;
    pipeline->count = pipeline->count + 1;
}

static int32_t pipeline_price(Pipeline* pipeline, int32_t amount) {
    int32_t total = amount;
    for (int32_t index = 0; index < pipeline->count; index = index + 1) {
        if (pipeline->steps[index] == STEP_DISCOUNT) total = total - total / 10;
        if (pipeline->steps[index] == STEP_TAX) total = total + total / 5;
        if (pipeline->steps[index] == STEP_ROUNDING) total = total / 100 * 100;
        if (pipeline->steps[index] == STEP_COUPON) total = total - 50;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    Pipeline* checkout = checkout_make();
    int64_t total = 0;
    for (int32_t index = 0; index < 10000000; index = index + 1) {
        int32_t amount = 1000 + index % 1000;
        total = total + pipeline_price(checkout, amount);
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
