/* The same work tuned by hand: every order holds its customer and its four lines inline (each name fits 16 bytes),
 * so the orders are one array of plain structs, and a deep copy of all of them is one allocation and one memcpy
 * of the whole array. The copy is made, read and freed every round, as the program asks. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../clock.h"

#define ORDER_COUNT 5000
#define LINE_COUNT 4

typedef struct Line {
    int32_t quantity;
    int32_t price;
} Line;

typedef struct Order {
    int32_t id;
    int32_t level;
    char name[16];
    Line lines[LINE_COUNT];
} Order;

static int32_t order_value(const Order* order) {
    int32_t amount = 0;
    for (int32_t index = 0; index < LINE_COUNT; index++) amount += order->lines[index].quantity * order->lines[index].price;
    return amount + order->level + order->id % 3;
}

int main(void) {
    int64_t start = now_nanoseconds();
    Order* orders = malloc(sizeof(Order) * ORDER_COUNT);
    for (int32_t index = 0; index < ORDER_COUNT; index++) {
        Order* order = &orders[index];
        order->id = index;
        order->level = index % 5;
        snprintf(order->name, sizeof(order->name), "customer %d", index % 300);
        for (int32_t line = 0; line < LINE_COUNT; line++) {
            order->lines[line].quantity = line + 1;
            order->lines[line].price = index % 97 + line;
        }
    }
    int64_t total = 0;
    for (int32_t round = 0; round < 40; round++) {
        Order* copies = malloc(sizeof(Order) * ORDER_COUNT);
        memcpy(copies, orders, sizeof(Order) * ORDER_COUNT);
        __asm__ volatile("" : : "r"(copies) : "memory");   /* the copy is made, as the program asks, not only read */
        int32_t value = 0;
        for (int32_t index = 0; index < ORDER_COUNT; index++) value += order_value(&copies[index]);
        total += value;
        free(copies);
    }
    int64_t microseconds = microseconds_since(start);
    printf("total %lld\n", (long long)total);
    print_microseconds(microseconds);
    free(orders);
    return 0;
}
