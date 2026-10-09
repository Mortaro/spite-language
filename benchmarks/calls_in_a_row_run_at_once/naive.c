/* naive/ written in C the way it reads: two objects, and their two counts called one after the other on the one
 * thread the program has. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Evens {
    int64_t total;
} Evens;

typedef struct Odds {
    int64_t total;
} Odds;

static void evens_count(Evens* evens) {
    for (int32_t index = 0; index < 40000000; index = index + 1) evens->total = evens->total + index % 7 * 2;
}

static void odds_count(Odds* odds) {
    for (int32_t index = 0; index < 40000000; index = index + 1) odds->total = odds->total + index % 5 * 2 + 1;
}

static void count_both(Evens* evens, Odds* odds) {
    evens_count(evens);
    odds_count(odds);
}

int main(void) {
    Evens* evens = malloc(sizeof(Evens));
    evens->total = 0;
    Odds* odds = malloc(sizeof(Odds));
    odds->total = 0;
    int64_t start = now_nanoseconds();
    count_both(evens, odds);
    int64_t microseconds = microseconds_since(start);
    printf("%lld %lld\n", (long long)evens->total, (long long)odds->total);
    print_microseconds(microseconds);
    free(evens);
    free(odds);
    return 0;
}
