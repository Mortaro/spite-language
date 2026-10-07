/* The same work tuned by hand: one array of nodes kept across the rounds, each round's chain linked through it
 * from the start of the array, and nothing asked of the C library per node. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

#define CHAIN_LENGTH 1000

typedef struct Node {
    int32_t value;
    struct Node* next;
} Node;

int main(void) {
    int64_t start = now_nanoseconds();
    Node* nodes = malloc(sizeof(Node) * CHAIN_LENGTH);
    int64_t total = 0;
    for (int32_t round = 0; round < 2000; round++) {
        nodes[0].value = round;
        nodes[0].next = NULL;
        Node* head = &nodes[0];
        for (int32_t index = 1; index < CHAIN_LENGTH; index++) {
            nodes[index].value = index + round;
            nodes[index].next = head;
            head = &nodes[index];
        }
        /* keeps the links written, as the program walks them */
        __asm__ volatile("" : "+r"(head) : : "memory");
        int32_t sum = 0;
        for (Node* current = head; current != NULL; current = current->next) sum += current->value;
        total += sum;
    }
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    free(nodes);
    return 0;
}
