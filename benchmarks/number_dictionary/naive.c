/* naive/ written in C the way a C programmer writes it:
 * an open-addressing hash table keyed by int32, grown by doubling. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* The same clock the Spite program reads: the time of the work goes to the error output and the answer to the
 * standard output, so run.sh compares the answers and times the work without the process's start. */
#ifdef _WIN32
#include <windows.h>
static int64_t now_nanoseconds(void) {
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    return (int64_t)((double)counter.QuadPart * 1000000000.0 / (double)frequency.QuadPart);
}
#else
#include <time.h>
static int64_t now_nanoseconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)now.tv_sec * 1000000000 + now.tv_nsec;
}
#endif

typedef struct Slot {
    int32_t key;
    int32_t value;
    int32_t used;
} Slot;

typedef struct Table {
    Slot* slots;
    int32_t capacity;
    int32_t shift;
    int32_t count;
} Table;

/* Fibonacci hashing: the top bits of the key times 2^64 over the golden ratio. */
static uint32_t home(const Table* table, int32_t key) {
    uint64_t hash = (uint64_t)(int64_t)key * 11400714819323198485ULL;
    return (uint32_t)(hash >> table->shift);
}

static void put(Table* table, int32_t key, int32_t value);

static void grow(Table* table) {
    Slot* old = table->slots;
    int32_t old_capacity = table->capacity;
    table->capacity = old_capacity == 0 ? 16 : old_capacity * 2;
    table->shift = old_capacity == 0 ? 60 : table->shift - 1;
    table->slots = calloc((size_t)table->capacity, sizeof(Slot));
    table->count = 0;
    for (int32_t index = 0; index < old_capacity; index = index + 1) {
        if (old[index].used) {
            put(table, old[index].key, old[index].value);
        }
    }
    free(old);
}

static void put(Table* table, int32_t key, int32_t value) {
    if ((table->count + 1) * 4 > table->capacity * 3) {
        grow(table);
    }
    uint32_t slot = home(table, key);
    while (table->slots[slot].used && table->slots[slot].key != key) {
        slot = (slot + 1) & (uint32_t)(table->capacity - 1);
    }
    if (!table->slots[slot].used) {
        table->count = table->count + 1;
    }
    table->slots[slot].key = key;
    table->slots[slot].value = value;
    table->slots[slot].used = 1;
}

static const int32_t* get(const Table* table, int32_t key) {
    uint32_t slot = home(table, key);
    while (table->slots[slot].used) {
        if (table->slots[slot].key == key) {
            return &table->slots[slot].value;
        }
        slot = (slot + 1) & (uint32_t)(table->capacity - 1);
    }
    return 0;
}

int main(void) {
    int64_t start = now_nanoseconds();
    Table scores = {0, 0, 0, 0};
    for (int32_t index = 0; index < 500000; index = index + 1) {
        put(&scores, index * 7, index);
    }
    int64_t total = 0;
    int32_t found = 0;
    for (int32_t round = 0; round < 10; round = round + 1) {
        for (int32_t index = 0; index < 500000; index = index + 1) {
            const int32_t* score = get(&scores, index * 3);
            if (score) {
                total = total + *score;
                found = found + 1;
            }
        }
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("entries %d found %d total %lld\n", scores.count, found, (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(scores.slots);
    return 0;
}
