/* naive/ written in C the way it reads: a desk holding an array of pointers to pages, take() copying one into the
 * open page and put_back() storing it into its slot again. C keeps no counts, so the write-back is one store. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Page {
    int32_t words;
} Page;

typedef struct Desk {
    Page** pages;
    int32_t count;
    int32_t capacity;
    Page* open;
    int32_t at;
} Desk;

static void stock(Desk* self, int32_t count) {
    for (int32_t index = 0; index < count; index = index + 1) {
        if (self->count == self->capacity) {
            self->capacity = self->capacity == 0 ? 4 : self->capacity * 2;
            self->pages = realloc(self->pages, sizeof(Page*) * self->capacity);
        }
        Page* made = malloc(sizeof(Page));
        made->words = index % 7;
        self->pages[self->count] = made;
        self->count = self->count + 1;
    }
}

static void take(Desk* self, int32_t index) {
    self->at = index;
    if (self->at < 0 || self->at >= self->count) abort();
    self->open = self->pages[self->at];
}

static void put_back(Desk* self) {
    if (self->at < 0 || self->at >= self->count) abort();
    self->pages[self->at] = self->open;
}

int main(void) {
    Desk* desk = calloc(1, sizeof(Desk));
    stock(desk, 100000);
    int64_t start = now_nanoseconds();
    for (int32_t round = 0; round < 100; round = round + 1) {
        for (int32_t index = 0; index < 100000; index = index + 1) {
            take(desk, index);
            desk->open->words = desk->open->words + 1;
            put_back(desk);
        }
    }
    int32_t total = 0;
    for (int32_t index = 0; index < desk->count; index = index + 1) total = total + desk->pages[index]->words;
    int64_t microseconds = microseconds_since(start);
    printf("words %d\n", total);
    print_microseconds(microseconds);
    for (int32_t index = 0; index < desk->count; index = index + 1) free(desk->pages[index]);
    free(desk->pages);
    free(desk);
    return 0;
}
