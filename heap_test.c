#include "heap.h"
#include "ok.h"

#include <stdlib.h>
#include <stdio.h>

static int compare (void* a, void* b)
{
    return *(int*) b - *(int*) a;
}

int main()
{
    int i;
    int entries[] = { 2, 4, 7, 9, 12, 3, 1 };
    int ordered[] = { 1, 2, 3, 4, 7, 9, 12 };
    void* buffer[7];
    pq_queue_t q;
    pq_init(&q, &buffer, 7, -1, &compare);

    plan(25);

    ok(pq_child_of(0, 1) == 1 && pq_child_of(0, 2) == 2,
        "index 0 right child : %d and left child : %d", pq_child_of(0, 1), pq_child_of(0, 2));
    ok(pq_child_of(1, 1) == 3 && pq_child_of(1, 2) == 4,
        "index 1 right child : %d and left child : %d", pq_child_of(1, 1), pq_child_of(1, 2));
    ok(pq_child_of(2, 1) == 5 && pq_child_of(2, 2) == 6,
        "index 2 right child : %d and left child : %d", pq_child_of(2, 1), pq_child_of(2, 2));
    ok(pq_child_of(3, 1) == 7 && pq_child_of(3, 2) == 8,
        "index 3 right child : %d and left child : %d", pq_child_of(3, 1), pq_child_of(3, 2));
    ok(pq_child_of(4, 1) == 9 && pq_child_of(4, 2) == 10,
        "index 4 right child : %d and left child : %d", pq_child_of(4, 1), pq_child_of(4, 2));
    ok(pq_child_of(5, 1) == 11 && pq_child_of(5, 2) == 12,
        "index 5 right child : %d and left child : %d", pq_child_of(5, 1), pq_child_of(5, 2));
    ok(pq_child_of(6, 1) == 13 && pq_child_of(6, 2) == 14,
        "index 6 right child : %d and left child : %d", pq_child_of(6, 1), pq_child_of(6, 2));

    ok(pq_parent_of(10) == 4, "index 10 parent of : %d", pq_parent_of(10));
    ok(pq_parent_of(9) == 4, "index 9 parent of : %d", pq_parent_of(9));
    ok(pq_parent_of(8) == 3, "index 8 parent of : %d", pq_parent_of(8));
    ok(pq_parent_of(7) == 3, "index 7 parent of : %d", pq_parent_of(7));
    ok(pq_parent_of(6) == 2, "index 6 parent of : %d", pq_parent_of(6));
    ok(pq_parent_of(5) == 2, "index 5 parent of : %d", pq_parent_of(5));
    ok(pq_parent_of(4) == 1, "index 4 parent of : %d", pq_parent_of(4));
    ok(pq_parent_of(3) == 1, "index 3 parent of : %d", pq_parent_of(3));
    ok(pq_parent_of(2) == 0, "index 2 parent of : %d", pq_parent_of(2));
    ok(pq_parent_of(1) == 0, "index 1 parent of : %d", pq_parent_of(1));

    for (i = 0; i < 7; i++) {
        printf("inserting: %d, %d\n", i, entries[i]);
        pq_push(&q, &entries[i]);
    }

    for (i = 0; i < 7; i++) {
        printf("%d: %d\n", i, *(int*) q.elements[i]);
    }

    for (i = 0; i < sizeof(ordered) / sizeof(int); i++) {
        int shifted = *(int*) pq_shift(&q);
        ok(shifted == ordered[i], "expected: %d, actual: %d", shifted, ordered[i]);
    }
    ok(pq_empty(&q), "queue empty");

    return EXIT_SUCCESS;
}
