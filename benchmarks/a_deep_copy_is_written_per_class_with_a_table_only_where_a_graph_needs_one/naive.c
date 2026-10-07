/* naive/ written in C the way it reads: a struct and a malloc per object, a growable array of pointers per list,
 * each name a string of its own on the heap, and a deep copy function per struct, written by hand, that copies
 * each field and calls the copy of each object it holds. No struct here can reach itself again, so none of the
 * copies keeps a table of what it has copied. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

typedef struct List {
    void** items;
    int32_t count;
    int32_t capacity;
} List;

typedef struct Customer {
    char* name;
    int32_t level;
} Customer;

typedef struct Line {
    int32_t quantity;
    int32_t price;
} Line;

typedef struct Order {
    int32_t id;
    Customer* customer;
    List* lines;
} Order;

static List* list_make(void) {
    List* list = malloc(sizeof(List));
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

static void list_append(List* list, void* item) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->items = realloc(list->items, sizeof(void*) * list->capacity);
    }
    list->items[list->count] = item;
    list->count = list->count + 1;
}

static char* text_copy(const char* text) {
    char* copied = malloc(strlen(text) + 1);
    strcpy(copied, text);
    return copied;
}

static Customer* customer_make(const char* name, int32_t level) {
    Customer* customer = malloc(sizeof(Customer));
    customer->name = text_copy(name);
    customer->level = level;
    return customer;
}

static Line* line_make(int32_t quantity, int32_t price) {
    Line* line = malloc(sizeof(Line));
    line->quantity = quantity;
    line->price = price;
    return line;
}

static Order* order_make(int32_t id, Customer* customer) {
    Order* order = malloc(sizeof(Order));
    order->id = id;
    order->customer = customer;
    order->lines = list_make();
    return order;
}

static Customer* customer_deep_copy(Customer* customer) {
    return customer_make(customer->name, customer->level);
}

static Line* line_deep_copy(Line* line) {
    return line_make(line->quantity, line->price);
}

static List* lines_deep_copy(List* lines) {
    List* copied = list_make();
    for (int32_t index = 0; index < lines->count; index = index + 1) list_append(copied, line_deep_copy(lines->items[index]));
    return copied;
}

static Order* order_deep_copy(Order* order) {
    Order* copied = malloc(sizeof(Order));
    copied->id = order->id;
    copied->customer = customer_deep_copy(order->customer);
    copied->lines = lines_deep_copy(order->lines);
    return copied;
}

static List* orders_deep_copy(List* orders) {
    List* copied = list_make();
    for (int32_t index = 0; index < orders->count; index = index + 1) list_append(copied, order_deep_copy(orders->items[index]));
    return copied;
}

static void orders_free(List* orders) {
    for (int32_t index = 0; index < orders->count; index = index + 1) {
        Order* order = orders->items[index];
        for (int32_t line = 0; line < order->lines->count; line = line + 1) free(order->lines->items[line]);
        free(order->lines->items);
        free(order->lines);
        free(order->customer->name);
        free(order->customer);
        free(order);
    }
    free(orders->items);
    free(orders);
}

static int32_t line_amount(Line* line) {
    return line->quantity * line->price;
}

static int32_t order_value(Order* order) {
    int32_t amount = 0;
    for (int32_t index = 0; index < order->lines->count; index = index + 1) amount = amount + line_amount(order->lines->items[index]);
    return amount + order->customer->level + order->id % 3;
}

static void add_lines(Order* order, int32_t index) {
    for (int32_t line_index = 0; line_index < 4; line_index = line_index + 1) {
        list_append(order->lines, line_make(line_index + 1, index % 97 + line_index));
    }
}

static List* make_orders(int32_t count) {
    List* orders = list_make();
    for (int32_t index = 0; index < count; index = index + 1) {
        char name[32];
        snprintf(name, sizeof(name), "customer %d", index % 300);
        Customer* customer = customer_make(name, index % 5);
        Order* order = order_make(index, customer);
        add_lines(order, index);
        list_append(orders, order);
    }
    return orders;
}

static int64_t copy_rounds(List* orders) {
    int64_t total = 0;
    for (int32_t round = 0; round < 40; round = round + 1) {
        List* copies = orders_deep_copy(orders);
        int32_t value = 0;
        for (int32_t index = 0; index < copies->count; index = index + 1) value = value + order_value(copies->items[index]);
        total = total + value;
        orders_free(copies);
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    List* orders = make_orders(5000);
    int64_t total = copy_rounds(orders);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    orders_free(orders);
    return 0;
}
