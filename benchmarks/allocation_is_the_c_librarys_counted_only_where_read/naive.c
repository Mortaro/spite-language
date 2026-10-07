/* naive/ written in C the way it reads: a struct per class, malloc per node, and each chain freed node by node
 * once its sum is taken, as the Spite program lets it go. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../clock.h"

typedef struct Node {
    int32_t value;
    struct Node* next;
} Node;

static Node* node_make(int32_t value, Node* next) {
    Node* node = malloc(sizeof(Node));
    node->value = value;
    node->next = next;
    return node;
}

static void chain_free(Node* head) {
    while (head != NULL) {
        Node* next = head->next;
        free(head);
        head = next;
    }
}

static Node* made_chain(int32_t length, int32_t round) {
    Node* head = node_make(round, NULL);
    for (int32_t index = 1; index < length; index = index + 1) {
        Node* added = node_make(index + round, head);
        head = added;
    }
    return head;
}

static int32_t chain_sum(Node* head) {
    int32_t total = 0;
    for (Node* current = head; current != NULL; current = current->next) total = total + current->value;
    return total;
}

static int64_t all_rounds(int32_t rounds) {
    int64_t total = 0;
    for (int32_t round = 0; round < rounds; round = round + 1) {
        Node* chain = made_chain(1000, round);
        int32_t sum = chain_sum(chain);
        chain_free(chain);
        total = total + sum;
    }
    return total;
}

int main(void) {
    int64_t start = now_nanoseconds();
    int64_t total = all_rounds(2000);
    int64_t microseconds = (now_nanoseconds() - start) / 1000;
    printf("total %lld\n", (long long)total);
    fprintf(stderr, "microseconds %lld\n", (long long)microseconds);
    return 0;
}
