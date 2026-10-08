/* The layout sweep of design/proposals/data_oriented_layout.md: one record of eight 32-bit fields, stored eight ways,
 * walked by seven loops that read one field, half of them, all of them, filter on one, update one, and read one or
 * all in a random order, at sizes from the first level of cache to well past the last. It prints, per loop, a table
 * of nanoseconds per record for each layout and size, and then what converting between two layouts costs. Each
 * number is the least of several rounds (three unless the second argument says), every round making the layouts
 * again, so a round slowed by other work on the machine does not count.
 * sweep.sh builds it three ways (scalar, the C compiler's vectoriser for SSE2, and for AVX2) and runs each.
 * Every layout must give the same answer for every read-only loop, or the sweep stops naming the pair. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../benchmarks/clock.h"

#ifndef BUILD_NAME
#define BUILD_NAME "unnamed"
#endif

typedef struct { int32_t f[8]; } Record;                        /* array of structures, inline */
typedef struct { int32_t header[2]; int32_t f[8]; } Object;     /* an object with Spite's 8-byte header, by pointer */
typedef struct { int32_t f[4]; } Half;                          /* hot and cold split, four and four */
typedef struct { int32_t f[8][8]; } Block8;                     /* AoSoA, 8 lanes: one AVX2 register per field */
typedef struct { int32_t f[8][16]; } Block16;                   /* AoSoA, 16 lanes: one cache line per field */

static Record* aos;
static int32_t* soa[8];
static Object* pool;          /* objects side by side, as Spite's class pools put them */
static Object** pooled;       /* the list: pointers into the pool, in memory order */
static Object** scattered;    /* the same pool, pointers in a shuffled order (a list after sorting or churn) */
static Object** malloced;     /* one malloc per object, made in list order, as naive C does */
static Half* hot;
static Half* cold;
static Block8* block8;
static Block16* block16;
static int32_t* order;        /* a random permutation, for the random-order loops */

/* ---- the loops: each reads a record through FIELD(k), which each layout defines ---- */
#define LOOP_ONE(F)    sum += F(0);
#define LOOP_HALF(F)   sum += (int64_t)(F(0) * F(1) + F(2) - F(3));
#define LOOP_ALL(F)    sum += (int64_t)(F(0) + 2 * F(1) + 3 * F(2) + 4 * F(3) + 5 * F(4) + 6 * F(5) + 7 * F(6) + 8 * F(7));
#define LOOP_FILTER(F) if (F(0) < 512) sum += F(1);
#define LOOP_UPDATE(F) F(0) = (F(0) + F(1)) & 1023;

#define AOS_F(k) aos[i].f[k]
#define SOA_F(k) soa_##k[i]
#define POOL_F(k) pooled[i]->f[k]
#define SCATTER_F(k) scattered[i]->f[k]
#define MALLOC_F(k) malloced[i]->f[k]
#define HYBRID_F(k) (*((k) < 4 ? &hot[i].f[(k) & 3] : &cold[i].f[(k) & 3]))
#define BLOCK8_F(k) block8[b].f[k][lane]
#define BLOCK16_F(k) block16[b].f[k][lane]

/* SoA reads each column through a local pointer, so the C compiler sees eight arrays, not one global table */
#define SOA_LOCALS int32_t* restrict soa_0 = soa[0]; int32_t* restrict soa_1 = soa[1]; int32_t* restrict soa_2 = soa[2]; \
    int32_t* restrict soa_3 = soa[3]; int32_t* restrict soa_4 = soa[4]; int32_t* restrict soa_5 = soa[5]; \
    int32_t* restrict soa_6 = soa[6]; int32_t* restrict soa_7 = soa[7]; \
    (void)soa_0; (void)soa_1; (void)soa_2; (void)soa_3; (void)soa_4; (void)soa_5; (void)soa_6; (void)soa_7;

#define FLAT(name, loop, field, locals) \
    static int64_t name(int32_t count) { locals int64_t sum = 0; \
        for (int32_t i = 0; i < count; i++) { loop(field) } return sum; }
#define FLAT_RANDOM(name, loop, field, locals) \
    static int64_t name(int32_t count) { locals int64_t sum = 0; \
        for (int32_t j = 0; j < count; j++) { int32_t i = order[j]; loop(field) } return sum; }
#define BLOCKED(name, loop, field, lanes) \
    static int64_t name(int32_t count) { int64_t sum = 0; \
        for (int32_t b = 0; b < count / lanes; b++) for (int32_t lane = 0; lane < lanes; lane++) { loop(field) } return sum; }
#define BLOCKED_RANDOM(name, loop, field, lanes) \
    static int64_t name(int32_t count) { int64_t sum = 0; \
        for (int32_t j = 0; j < count; j++) { int32_t b = order[j] / lanes; int32_t lane = order[j] % lanes; loop(field) } return sum; }

#define LAYOUT_FLAT(layout, field, locals) \
    FLAT(layout##_one, LOOP_ONE, field, locals) FLAT(layout##_half, LOOP_HALF, field, locals) \
    FLAT(layout##_all, LOOP_ALL, field, locals) FLAT(layout##_filter, LOOP_FILTER, field, locals) \
    FLAT(layout##_update, LOOP_UPDATE, field, locals) \
    FLAT_RANDOM(layout##_random_one, LOOP_ONE, field, locals) FLAT_RANDOM(layout##_random_all, LOOP_ALL, field, locals)
#define LAYOUT_BLOCKED(layout, field, lanes) \
    BLOCKED(layout##_one, LOOP_ONE, field, lanes) BLOCKED(layout##_half, LOOP_HALF, field, lanes) \
    BLOCKED(layout##_all, LOOP_ALL, field, lanes) BLOCKED(layout##_filter, LOOP_FILTER, field, lanes) \
    BLOCKED(layout##_update, LOOP_UPDATE, field, lanes) \
    BLOCKED_RANDOM(layout##_random_one, LOOP_ONE, field, lanes) BLOCKED_RANDOM(layout##_random_all, LOOP_ALL, field, lanes)

LAYOUT_FLAT(malloced, MALLOC_F, )
LAYOUT_FLAT(pooled, POOL_F, )
LAYOUT_FLAT(scattered, SCATTER_F, )
LAYOUT_FLAT(aos, AOS_F, )
LAYOUT_FLAT(soa, SOA_F, SOA_LOCALS)
LAYOUT_FLAT(hybrid, HYBRID_F, )
LAYOUT_BLOCKED(block8, BLOCK8_F, 8)
LAYOUT_BLOCKED(block16, BLOCK16_F, 16)

typedef int64_t (*Loop)(int32_t);
#define LAYOUTS 8
#define LOOPS 7
static const char* layout_names[LAYOUTS] = {
    "objects, one malloc each", "objects in a pool (Spite today)", "pool, list order shuffled",
    "array of structures", "structure of arrays", "hot and cold, 4 and 4", "AoSoA, 8 lanes", "AoSoA, 16 lanes"};
static const char* loop_names[LOOPS] = {
    "one field of eight, in order", "four fields of eight, in order", "all eight fields, in order",
    "filter on one field, sum another (half pass)", "update one field from another, in order",
    "one field of eight, random order", "all eight fields, random order"};
#define ROW(layout) { layout##_one, layout##_half, layout##_all, layout##_filter, layout##_update, layout##_random_one, layout##_random_all }
static Loop loops[LAYOUTS][LOOPS] = { ROW(malloced), ROW(pooled), ROW(scattered), ROW(aos), ROW(soa), ROW(hybrid), ROW(block8), ROW(block16) };

static uint64_t random_state = 88172645463325252ull;
static uint32_t next_random(void) {
    random_state ^= random_state << 13; random_state ^= random_state >> 7; random_state ^= random_state << 17;
    return (uint32_t)(random_state >> 11);
}

static void shuffle(int32_t* values, int32_t count) {
    for (int32_t index = count - 1; index > 0; index--) {
        int32_t other = (int32_t)(next_random() % (uint32_t)(index + 1));
        int32_t kept = values[index]; values[index] = values[other]; values[other] = kept;
    }
}

static void* aligned(size_t bytes) {
#ifdef _WIN32
    void* memory = _aligned_malloc(bytes, 64);
#else
    void* memory = aligned_alloc(64, (bytes + 63) / 64 * 64);
#endif
    if (!memory) { fprintf(stderr, "out of memory\n"); exit(1); }
    memset(memory, 0, bytes);
    return memory;
}
static void aligned_free(void* memory) {
#ifdef _WIN32
    _aligned_free(memory);
#else
    free(memory);
#endif
}

static void make(int32_t count) {
    aos = aligned(sizeof(Record) * count);
    for (int k = 0; k < 8; k++) soa[k] = aligned(sizeof(int32_t) * count);
    pool = aligned(sizeof(Object) * count);
    pooled = aligned(sizeof(Object*) * count);
    scattered = aligned(sizeof(Object*) * count);
    malloced = aligned(sizeof(Object*) * count);
    hot = aligned(sizeof(Half) * count);
    cold = aligned(sizeof(Half) * count);
    block8 = aligned(sizeof(Block8) * (count / 8));
    block16 = aligned(sizeof(Block16) * (count / 16));
    order = aligned(sizeof(int32_t) * count);
    for (int32_t i = 0; i < count; i++) {
        for (int k = 0; k < 8; k++) {
            int32_t value = (int32_t)(next_random() % 1024);
            aos[i].f[k] = value;
            soa[k][i] = value;
            pool[i].f[k] = value;
            if (k < 4) hot[i].f[k] = value; else cold[i].f[k - 4] = value;
            block8[i / 8].f[k][i % 8] = value;
            block16[i / 16].f[k][i % 16] = value;
        }
        pool[i].header[0] = 1;
        pooled[i] = &pool[i];
        Object* object = malloc(sizeof(Record));   /* the plain C struct, without a header */
        memcpy(object, &aos[i], sizeof(Record));
        malloced[i] = (Object*)((char*)object - offsetof(Object, f));
        order[i] = i;
    }
    shuffle(order, count);
    /* the shuffled list holds the pool's objects in another order, so its i-th item is pool[order[i]]: the
     * records' values are permuted the same way, which the sums do not see */
    for (int32_t i = 0; i < count; i++) scattered[i] = &pool[order[i]];
    shuffle(order, count);
}

static void unmake(int32_t count) {
    for (int32_t i = 0; i < count; i++) free((char*)malloced[i] + offsetof(Object, f));
    aligned_free(aos); for (int k = 0; k < 8; k++) aligned_free(soa[k]);
    aligned_free(pool); aligned_free(pooled); aligned_free(scattered); aligned_free(malloced);
    aligned_free(hot); aligned_free(cold); aligned_free(block8); aligned_free(block16); aligned_free(order);
}

static volatile int64_t sink;

/* best nanoseconds per record of one loop: passes enough to take about 20 ms per sample, five samples */
static double time_loop(Loop loop, int32_t count) {
    int64_t start = now_nanoseconds();
    sink += loop(count);
    int64_t once = now_nanoseconds() - start;
    int64_t passes = once > 0 ? 20000000 / once : 1000;
    if (passes < 1) passes = 1;
    double best = 1e30;
    for (int sample = 0; sample < 5; sample++) {
        start = now_nanoseconds();
        for (int64_t pass = 0; pass < passes; pass++) sink += loop(count);
        double each = (double)(now_nanoseconds() - start) / (double)passes / (double)count;
        if (each < best) best = each;
    }
    return best;
}

static void aos_to_soa(int32_t count) {
    SOA_LOCALS
    for (int32_t i = 0; i < count; i++) {
        soa_0[i] = aos[i].f[0]; soa_1[i] = aos[i].f[1]; soa_2[i] = aos[i].f[2]; soa_3[i] = aos[i].f[3];
        soa_4[i] = aos[i].f[4]; soa_5[i] = aos[i].f[5]; soa_6[i] = aos[i].f[6]; soa_7[i] = aos[i].f[7];
    }
}
static void soa_to_aos(int32_t count) {
    SOA_LOCALS
    for (int32_t i = 0; i < count; i++) {
        aos[i].f[0] = soa_0[i]; aos[i].f[1] = soa_1[i]; aos[i].f[2] = soa_2[i]; aos[i].f[3] = soa_3[i];
        aos[i].f[4] = soa_4[i]; aos[i].f[5] = soa_5[i]; aos[i].f[6] = soa_6[i]; aos[i].f[7] = soa_7[i];
    }
}
static void pool_to_soa_one(int32_t count) {
    int32_t* restrict column = soa[0];
    for (int32_t i = 0; i < count; i++) column[i] = pooled[i]->f[0];
}
/* ---- what a checked sum costs: Spite halts on an overflow, so every + is checked unless it is proven safe ---- */
__attribute__((noinline, noreturn)) static void overflowed(void) {
    fprintf(stderr, "overflow\n");
    exit(2);
}
/* a Long sum of Integer values checked at every step, as Spite writes it today */
static int64_t sum_checked_long(int32_t count) {
    const int32_t* restrict column = soa[0];
    int64_t sum = 0;
    for (int32_t i = 0; i < count; i++) {
        if (__builtin_add_overflow(sum, (int64_t)column[i], &sum)) overflowed();
    }
    return sum;
}
/* the same sum with the check left out, as it may be: count Integer values (count below 2^31) summed in a Long can
 * never leave the Long's range */
static int64_t sum_proven_long(int32_t count) {
    const int32_t* restrict column = soa[0];
    int64_t sum = 0;
    for (int32_t i = 0; i < count; i++) sum += column[i];
    return sum;
}
/* an Integer sum of Integer values checked at every step (each value less 512, so the sum stays in range) */
static int64_t sum_checked_integer(int32_t count) {
    const int32_t* restrict column = soa[0];
    int32_t sum = 0;
    for (int32_t i = 0; i < count; i++) {
        if (__builtin_add_overflow(sum, column[i] - 512, &sum)) overflowed();
    }
    return sum;
}
/* the Integer sum checked once: summed in 64-bit lanes beside the sum of the absolute values; when that bound fits an
 * Integer no prefix of the sum in written order can have left the range, so the answer is the checked loop's. When
 * it does not fit, the checked loop runs again from the start, halting at the very step the written loop would. */
static int64_t sum_deferred_integer(int32_t count) {
    const int32_t* restrict column = soa[0];
    int64_t sum = 0;
    int64_t bound = 0;
    for (int32_t i = 0; i < count; i++) {
        int64_t value = column[i] - 512;
        sum += value;
        bound += value < 0 ? -value : value;
    }
    if (bound > INT32_MAX) return sum_checked_integer(count);
    return (int32_t)sum;
}
#define CHECKS 4
static Loop checked_loops[CHECKS] = {sum_checked_long, sum_proven_long, sum_checked_integer, sum_deferred_integer};
static const char* checked_names[CHECKS] = {
    "Long sum, every + checked (Spite today)", "Long sum, check proven unneeded", "Integer sum, every + checked",
    "Integer sum, checked once by a bound"};

static double time_step(void (*step)(int32_t), int32_t count) {
    double best = 1e30;
    for (int sample = 0; sample < 5; sample++) {
        int64_t start = now_nanoseconds();
        step(count);
        double each = (double)(now_nanoseconds() - start) / (double)count;
        if (each < best) best = each;
    }
    return best;
}

static void keep_least(double* kept, double measured) {
    if (measured < *kept) *kept = measured;
}

#define SIZES 8
static const int32_t sizes[SIZES] = {512, 4096, 16384, 65536, 262144, 1048576, 2097152, 4194304};
static double results[SIZES][LAYOUTS][LOOPS];
static double conversions[SIZES][3];
static double checked[SIZES][CHECKS];

int main(int argument_count, char** arguments) {
#ifdef _WIN32
    SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)1 << 6);
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#endif
    int sizes_wanted = argument_count > 1 ? atoi(arguments[1]) : SIZES;
    if (sizes_wanted < 1 || sizes_wanted > SIZES) sizes_wanted = SIZES;
    int rounds = argument_count > 2 ? atoi(arguments[2]) : 3;
    if (rounds < 1) rounds = 1;
    for (int size = 0; size < SIZES; size++) {
        for (int layout = 0; layout < LAYOUTS; layout++)
            for (int loop = 0; loop < LOOPS; loop++) results[size][layout][loop] = 1e30;
        for (int conversion = 0; conversion < 3; conversion++) conversions[size][conversion] = 1e30;
        for (int check = 0; check < CHECKS; check++) checked[size][check] = 1e30;
    }
    for (int round = 0; round < rounds; round++)
    for (int size = 0; size < sizes_wanted; size++) {
        int32_t count = sizes[size];
        make(count);
        for (int loop = 0; loop < LOOPS; loop++) {
            if (loop == 4) continue;
            int64_t expected = loops[3][loop](count);
            for (int layout = 0; layout < LAYOUTS; layout++) {
                int64_t answer = loops[layout][loop](count);
                if (answer != expected) {
                    fprintf(stderr, "%s and %s answer %lld and %lld for %s\n", layout_names[layout], layout_names[3],
                        (long long)answer, (long long)expected, loop_names[loop]);
                    return 1;
                }
            }
        }
        for (int loop = 0; loop < LOOPS; loop++) {
            if (loop == 4) continue;
            for (int layout = 0; layout < LAYOUTS; layout++) keep_least(&results[size][layout][loop], time_loop(loops[layout][loop], count));
        }
        for (int layout = 0; layout < LAYOUTS; layout++) keep_least(&results[size][layout][4], time_loop(loops[layout][4], count));
        for (int check = 0; check < CHECKS; check++) keep_least(&checked[size][check], time_loop(checked_loops[check], count));
        keep_least(&conversions[size][0], time_step(aos_to_soa, count));
        keep_least(&conversions[size][1], time_step(soa_to_aos, count));
        keep_least(&conversions[size][2], time_step(pool_to_soa_one, count));
        unmake(count);
        fprintf(stderr, "round %d size %d done\n", round + 1, count);
    }
    printf("## Build: %s\n\n", BUILD_NAME);
    for (int loop = 0; loop < LOOPS; loop++) {
        printf("### %s (ns per record)\n\n| layout |", loop_names[loop]);
        for (int size = 0; size < sizes_wanted; size++) printf(" %d |", sizes[size]);
        printf("\n|---|");
        for (int size = 0; size < sizes_wanted; size++) printf("---|");
        printf("\n");
        for (int layout = 0; layout < LAYOUTS; layout++) {
            printf("| %s |", layout_names[layout]);
            for (int size = 0; size < sizes_wanted; size++) printf(" %.2f |", results[size][layout][loop]);
            printf("\n");
        }
        printf("\n");
    }
    printf("### Checked sums over one column (ns per record)\n\n| sum |");
    for (int size = 0; size < sizes_wanted; size++) printf(" %d |", sizes[size]);
    printf("\n|---|");
    for (int size = 0; size < sizes_wanted; size++) printf("---|");
    printf("\n");
    for (int check = 0; check < CHECKS; check++) {
        printf("| %s |", checked_names[check]);
        for (int size = 0; size < sizes_wanted; size++) printf(" %.2f |", checked[size][check]);
        printf("\n");
    }
    printf("\n");
    printf("### Converting between layouts (ns per record, one pass)\n\n| conversion |");
    for (int size = 0; size < sizes_wanted; size++) printf(" %d |", sizes[size]);
    printf("\n|---|");
    for (int size = 0; size < sizes_wanted; size++) printf("---|");
    printf("\n");
    const char* conversion_names[3] = {"array of structures to structure of arrays", "structure of arrays to array of structures", "pooled objects to one column"};
    for (int conversion = 0; conversion < 3; conversion++) {
        printf("| %s |", conversion_names[conversion]);
        for (int size = 0; size < sizes_wanted; size++) printf(" %.2f |", conversions[size][conversion]);
        printf("\n");
    }
    printf("\n");
    return 0;
}
