// D222 feasibility (design/proposals/one_list.md): the layouts one automatic List could pick, written by hand in C
// so each costs only what the layout costs. Not Spite and not run by check.sh; build and run it by hand:
//   clang -O2 benchmarks/one_list/layouts.c -o layouts.exe && ./layouts.exe
//   clang -O2 -ffast-math ...   to see what reassociating a float sum would buy
// Every variant computes the same answers, printed so the work cannot be optimised away.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
static double now_microseconds(void) {
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart * 1e6 / (double)frequency.QuadPart;
}
#else
static double now_microseconds(void) {
    struct timespec spec;
    clock_gettime(CLOCK_MONOTONIC, &spec);
    return spec.tv_sec * 1e6 + spec.tv_nsec / 1e3;
}
#endif

#define ENTITIES 200000
#define TICKS 100
#define ROUNDS 5
#define CHUNK_BITS 10
#define CHUNK (1 << CHUNK_BITS)

typedef struct { float left, top; } Position;
typedef struct { float across, down; int moving; } Velocity;
// a heap object as a List holds it today: a header with a count, then the attributes
typedef struct { int64_t count; int32_t class_id; float left, top; } PositionObject;
typedef struct { int64_t count; int32_t class_id; float across, down; int moving; } VelocityObject;

static Position flat_positions[ENTITIES];
static Velocity flat_velocities[ENTITIES];
static Position *chunked_positions[ENTITIES / CHUNK + 1];
static Velocity *chunked_velocities[ENTITIES / CHUNK + 1];
static PositionObject *position_objects[ENTITIES];
static VelocityObject *velocity_objects[ENTITIES];
static uint32_t position_generations[ENTITIES], velocity_generations[ENTITIES];
static uint32_t kept_position_generations[ENTITIES], kept_velocity_generations[ENTITIES];
// a sparse set per column, as SlopEngine keeps them: the entity of each row, and the row of each entity
static int32_t entity_of_position_row[ENTITIES];
static int32_t velocity_row_of_entity[ENTITIES];
static uint8_t position_alive[ENTITIES];
volatile int64_t sink;

static void shuffle(int32_t *values, int count, uint32_t seed) {
    for (int index = count - 1; index > 0; index--) {
        seed = seed * 1664525u + 1013904223u;
        int other = (int)(seed % (uint32_t)(index + 1));
        int32_t kept = values[index];
        values[index] = values[other];
        values[other] = kept;
    }
}

static void fill(void) {
    static int32_t order[ENTITIES];
    for (int index = 0; index < ENTITIES; index++) order[index] = index;
    shuffle(order, ENTITIES, 7);
    for (int row = 0; row < ENTITIES; row++) entity_of_position_row[row] = row;
    for (int row = 0; row < ENTITIES; row++) position_alive[row] = (uint8_t)((row * 2654435761u >> 7) % 4 != 0);
    for (int row = 0; row < ENTITIES; row++) velocity_row_of_entity[order[row]] = row;
    for (int chunk = 0; chunk <= ENTITIES / CHUNK; chunk++) {
        chunked_positions[chunk] = calloc(CHUNK, sizeof(Position));
        chunked_velocities[chunk] = calloc(CHUNK, sizeof(Velocity));
    }
    for (int index = 0; index < ENTITIES; index++) {
        Velocity velocity = { (float)(index % 100), (float)(index % 7), index % 3 != 0 };
        flat_velocities[index] = velocity;
        chunked_velocities[index >> CHUNK_BITS][index & (CHUNK - 1)] = velocity;
        position_objects[index] = calloc(1, sizeof(PositionObject));
        position_objects[index]->count = 1;
        velocity_objects[index] = calloc(1, sizeof(VelocityObject));
        velocity_objects[index]->count = 1;
        velocity_objects[index]->across = velocity.across;
        velocity_objects[index]->down = velocity.down;
        velocity_objects[index]->moving = velocity.moving;
    }
    // the heap objects are made in a different order from the list, as a program that makes them elsewhere would
    static int32_t spread[ENTITIES];
    for (int index = 0; index < ENTITIES; index++) spread[index] = index;
    shuffle(spread, ENTITIES, 11);
    for (int index = 0; index < ENTITIES; index++) {
        PositionObject *kept = position_objects[index];
        position_objects[index] = position_objects[spread[index]];
        position_objects[spread[index]] = kept;
    }
}

// ---- the stress shape: Move over sparse columns, position driven, velocity found through the entity ----
static void move_flat(void) {
    for (int row = 0; row < ENTITIES; row++) {
        int32_t entity = entity_of_position_row[row];
        Velocity *velocity = &flat_velocities[velocity_row_of_entity[entity]];
        Position *position = &flat_positions[row];
        position->left += velocity->across;
        position->top += velocity->down;
    }
}

static void move_chunked(void) {
    for (int row = 0; row < ENTITIES; row++) {
        int32_t entity = entity_of_position_row[row];
        int32_t other = velocity_row_of_entity[entity];
        Velocity *velocity = &chunked_velocities[other >> CHUNK_BITS][other & (CHUNK - 1)];
        Position *position = &chunked_positions[row >> CHUNK_BITS][row & (CHUNK - 1)];
        position->left += velocity->across;
        position->top += velocity->down;
    }
}

static void move_generation_checked(void) {
    for (int row = 0; row < ENTITIES; row++) {
        int32_t entity = entity_of_position_row[row];
        int32_t other = velocity_row_of_entity[entity];
        if (velocity_generations[other] != kept_velocity_generations[entity]) continue;
        if (position_generations[row] != kept_position_generations[entity]) continue;
        Velocity *velocity = &flat_velocities[other];
        Position *position = &flat_positions[row];
        position->left += velocity->across;
        position->top += velocity->down;
    }
}

// free slots instead of swapping: a quarter of the slots are holes, skipped by a flag, against the same live items packed
static void move_tombstones(void) {
    for (int row = 0; row < ENTITIES; row++) {
        if (!position_alive[row]) continue;
        int32_t entity = entity_of_position_row[row];
        Velocity *velocity = &flat_velocities[velocity_row_of_entity[entity]];
        Position *position = &flat_positions[row];
        position->left += velocity->across;
        position->top += velocity->down;
    }
}

static void move_packed_three_quarters(void) {
    for (int row = 0; row < ENTITIES / 4 * 3; row++) {
        int32_t entity = entity_of_position_row[row];
        Velocity *velocity = &flat_velocities[velocity_row_of_entity[entity]];
        Position *position = &flat_positions[row];
        position->left += velocity->across;
        position->top += velocity->down;
    }
}

static void move_references_uncounted(void) {
    for (int row = 0; row < ENTITIES; row++) {
        int32_t entity = entity_of_position_row[row];
        VelocityObject *velocity = velocity_objects[velocity_row_of_entity[entity]];
        PositionObject *position = position_objects[row];
        position->left += velocity->across;
        position->top += velocity->down;
    }
}

static void __attribute__((noinline)) release_object(int64_t *count) {
    if (--*count == 0) abort();
}

static void move_references_counted(void) {
    for (int row = 0; row < ENTITIES; row++) {
        int32_t entity = entity_of_position_row[row];
        VelocityObject *velocity = velocity_objects[velocity_row_of_entity[entity]];
        PositionObject *position = position_objects[row];
        velocity->count++;
        position->count++;
        position->left += velocity->across;
        position->top += velocity->down;
        release_object(&position->count);
        release_object(&velocity->count);
    }
}

// ---- a fused chain: each_integrate() then filter_moving().sum_across() over one column ----
static double chain_flat(void) {
    double total = 0;
    for (int index = 0; index < ENTITIES; index++) flat_velocities[index].across += flat_velocities[index].down * 0.5f;
    for (int index = 0; index < ENTITIES; index++)
        if (flat_velocities[index].moving) total += flat_velocities[index].across;
    return total;
}

static double chain_chunked(void) {
    double total = 0;
    for (int chunk = 0; chunk <= ENTITIES / CHUNK; chunk++) {
        int end = chunk == ENTITIES / CHUNK ? ENTITIES % CHUNK : CHUNK;
        Velocity *items = chunked_velocities[chunk];
        for (int index = 0; index < end; index++) items[index].across += items[index].down * 0.5f;
    }
    for (int chunk = 0; chunk <= ENTITIES / CHUNK; chunk++) {
        int end = chunk == ENTITIES / CHUNK ? ENTITIES % CHUNK : CHUNK;
        Velocity *items = chunked_velocities[chunk];
        for (int index = 0; index < end; index++)
            if (items[index].moving) total += items[index].across;
    }
    return total;
}

static double chain_chunked_indexed(void) {
    double total = 0;
    for (int index = 0; index < ENTITIES; index++) {
        Velocity *item = &chunked_velocities[index >> CHUNK_BITS][index & (CHUNK - 1)];
        item->across += item->down * 0.5f;
    }
    for (int index = 0; index < ENTITIES; index++) {
        Velocity *item = &chunked_velocities[index >> CHUNK_BITS][index & (CHUNK - 1)];
        if (item->moving) total += item->across;
    }
    return total;
}

static double chain_references(void) {
    double total = 0;
    for (int index = 0; index < ENTITIES; index++)
        velocity_objects[index]->across += velocity_objects[index]->down * 0.5f;
    for (int index = 0; index < ENTITIES; index++)
        if (velocity_objects[index]->moving) total += velocity_objects[index]->across;
    return total;
}

// ---- plain values: what vectorising buys ----
#define NUMBERS 1000000
static float floats[NUMBERS], other_floats[NUMBERS];
static int32_t integers[NUMBERS];

static void __attribute__((noinline)) scale_floats(float *values, const float *from, int count, float by, float plus) {
    for (int index = 0; index < count; index++) values[index] = from[index] * by + plus;
}

static void __attribute__((noinline)) scale_floats_scalar(float *values, const float *from, int count, float by, float plus) {
#pragma clang loop vectorize(disable) interleave(disable)
    for (int index = 0; index < count; index++) values[index] = from[index] * by + plus;
}

static float __attribute__((noinline)) sum_floats(const float *values, int count) {
    float total = 0;
    for (int index = 0; index < count; index++) total += values[index];
    return total;
}

static int64_t __attribute__((noinline)) sum_integers(const int32_t *values, int count) {
    int64_t total = 0;
    for (int index = 0; index < count; index++) total += values[index];
    return total;
}

static int64_t __attribute__((noinline)) sum_integers_scalar(const int32_t *values, int count) {
    int64_t total = 0;
#pragma clang loop vectorize(disable) interleave(disable)
    for (int index = 0; index < count; index++) total += values[index];
    return total;
}

// a List<Integer> read through List's []: T?, so a range test on every read, as the Spite loop has it
typedef struct { int64_t count; int32_t *items; int32_t item_count; } IntegerList;
static int32_t __attribute__((noinline)) list_find_at(IntegerList *list, int32_t index, int *found) {
    if (index >= 0 && index < list->item_count) { *found = 1; return list->items[index]; }
    *found = 0;
    return 0;
}

static int64_t __attribute__((noinline)) sum_list_checked(IntegerList *list) {
    int64_t total = 0;
    for (int index = 0; index < list->item_count; index++) {
        if (index >= 0 && index < list->item_count) total += list->items[index];
        else abort();
    }
    return total;
}

static int64_t __attribute__((noinline)) sum_list_called(IntegerList *list) {
    int64_t total = 0;
    for (int index = 0; index < list->item_count; index++) {
        int found;
        int32_t value = list_find_at(list, index, &found);
        if (!found) abort();
        total += value;
    }
    return total;
}

#define TIME(best, body) do { double start_ = now_microseconds(); body; double took_ = now_microseconds() - start_; \
    if (best < 0 || took_ < best) best = took_; } while (0)

int main(void) {
    fill();
    double flat = -1, chunked = -1, checked = -1, uncounted = -1, counted = -1, holes = -1, packed = -1;
    for (int round = 0; round < ROUNDS; round++) {
        TIME(flat, for (int tick = 0; tick < TICKS; tick++) move_flat());
        TIME(chunked, for (int tick = 0; tick < TICKS; tick++) move_chunked());
        TIME(checked, for (int tick = 0; tick < TICKS; tick++) move_generation_checked());
        TIME(uncounted, for (int tick = 0; tick < TICKS; tick++) move_references_uncounted());
        TIME(counted, for (int tick = 0; tick < TICKS; tick++) move_references_counted());
        TIME(holes, for (int tick = 0; tick < TICKS; tick++) move_tombstones());
        TIME(packed, for (int tick = 0; tick < TICKS; tick++) move_packed_three_quarters());
    }
    printf("move over sparse columns, %d entities, microseconds per tick (best of %d rounds):\n", ENTITIES, ROUNDS);
    printf("  flat inline            %8.1f\n", flat / TICKS);
    printf("  chunked inline (1024)  %8.1f\n", chunked / TICKS);
    printf("  flat, generation check %8.1f\n", checked / TICKS);
    printf("  references, uncounted  %8.1f\n", uncounted / TICKS);
    printf("  references, counted    %8.1f\n", counted / TICKS);
    printf("  a quarter holes, skipped %6.1f\n", holes / TICKS);
    printf("  the same live, packed  %8.1f\n", packed / TICKS);
    double chain_flat_time = -1, chain_chunk_time = -1, chain_chunk_indexed_time = -1, chain_reference_time = -1;
    double answer = 0;
    for (int round = 0; round < ROUNDS; round++) {
        TIME(chain_flat_time, for (int tick = 0; tick < TICKS; tick++) answer += chain_flat());
        TIME(chain_chunk_time, for (int tick = 0; tick < TICKS; tick++) answer += chain_chunked());
        TIME(chain_chunk_indexed_time, for (int tick = 0; tick < TICKS; tick++) answer += chain_chunked_indexed());
        TIME(chain_reference_time, for (int tick = 0; tick < TICKS; tick++) answer += chain_references());
    }
    printf("each_integrate + filter_moving().sum_across(), microseconds per tick:\n");
    printf("  flat inline            %8.1f\n", chain_flat_time / TICKS);
    printf("  chunked, walked by chunk %6.1f\n", chain_chunk_time / TICKS);
    printf("  chunked, indexed       %8.1f\n", chain_chunk_indexed_time / TICKS);
    printf("  references             %8.1f\n", chain_reference_time / TICKS);
    for (int index = 0; index < NUMBERS; index++) {
        other_floats[index] = (float)(index % 1000) * 0.001f;
        integers[index] = index % 1000;
    }
    double vector_scale = -1, scalar_scale = -1, float_sum = -1, vector_sum = -1, scalar_sum = -1;
    double list_checked = -1, list_called = -1;
    float float_answer = 0;
    int64_t integer_answer = 0;
    IntegerList list = { 1, integers, NUMBERS };
    for (int round = 0; round < ROUNDS; round++) {
        TIME(vector_scale, for (int pass = 0; pass < 100; pass++) scale_floats(floats, other_floats, NUMBERS, 1.5f, 0.25f));
        TIME(scalar_scale, for (int pass = 0; pass < 100; pass++) scale_floats_scalar(floats, other_floats, NUMBERS, 1.5f, 0.25f));
        TIME(float_sum, for (int pass = 0; pass < 100; pass++) { other_floats[pass] = (float)(pass % 1000) * 0.001f; float_answer += sum_floats(other_floats, NUMBERS); });
        TIME(vector_sum, for (int pass = 0; pass < 100; pass++) { integers[pass] = pass % 1000; integer_answer += sum_integers(integers, NUMBERS); });
        TIME(scalar_sum, for (int pass = 0; pass < 100; pass++) { integers[pass] = pass % 1000; integer_answer += sum_integers_scalar(integers, NUMBERS); });
        TIME(list_checked, for (int pass = 0; pass < 100; pass++) { integers[pass] = pass % 1000; integer_answer += sum_list_checked(&list); });
        TIME(list_called, for (int pass = 0; pass < 100; pass++) { integers[pass] = pass % 1000; integer_answer += sum_list_called(&list); });
    }
    printf("plain values, %d items, microseconds per pass:\n", NUMBERS);
    printf("  x = y * a + b, vectorised      %8.1f\n", vector_scale / 100);
    printf("  x = y * a + b, scalar          %8.1f\n", scalar_scale / 100);
    printf("  float sum (strict order)       %8.1f\n", float_sum / 100);
    printf("  integer sum, vectorised        %8.1f\n", vector_sum / 100);
    printf("  integer sum, scalar            %8.1f\n", scalar_sum / 100);
    printf("  integer sum, range test inline %8.1f\n", list_checked / 100);
    printf("  integer sum, read by a call    %8.1f\n", list_called / 100);
    sink = (int64_t)answer + (int64_t)float_answer + integer_answer + (int64_t)floats[NUMBERS / 2]
        + (int64_t)flat_positions[ENTITIES / 2].left + (int64_t)position_objects[ENTITIES / 2]->left
        + (int64_t)chunked_positions[3][5].left;
    return 0;
}
