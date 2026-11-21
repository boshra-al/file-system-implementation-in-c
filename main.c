/*
 * main.c - Program entry point
 */
#include <stdio.h>
#include "fs.h"

static int failures = 0;

static void check(const char *what, int got, int expected)
{
    if (got == expected) {
        printf("  PASS  %s\n", what);
    } else {
        printf("  FAIL  %s (got %d, expected %d)\n", what, got, expected);
        failures++;
    }
}

int main(void)
{
    if (!checkLayout()) {
        fprintf(stderr, "Incorrect disk layout; not continuing.\n");
        return 1;
    }

    initRoot();
    buildList();

    /* ---- Root directory checks ---- */
    printf("Root directory\n");
    check("root back == -1", sector[0].directory.back, -1);
    check("root frwd == -1", sector[0].directory.frwd, -1);
    check("root free == 25", sector[0].directory.free, 25);

    /* ---- Free-sector list checks ---- */
    printf("Free-sector list\n");
    check("initial free count == 99", freeCount(), 99);

    check("Remove #1 == 1", Remove(), 1);
    check("Remove #2 == 2", Remove(), 2);
    check("Remove #3 == 3", Remove(), 3);
    check("free count == 96", freeCount(), 96);

    check("Insert(2) accepted", Insert(2), 1);
    check("Insert(2) again rejected (duplicate)", Insert(2), 0);
    check("Insert(50) rejected (already free)", Insert(50), 0);
    check("Insert(0) rejected (root)", Insert(0), 0);
    check("Insert(100) rejected (out of range)", Insert(100), 0);
    check("Insert(-1) rejected", Insert(-1), 0);
    check("free count == 97", freeCount(), 97);
    check("Remove returns lowest free (2)", Remove(), 2);

    /* Drain the list completely and make sure no number repeats. */
    int seen[NUM_SECTORS] = {0};
    int dup = 0, drained = 0, s;
    while ((s = Remove()) != -1) {
        if (seen[s]) dup++;
        seen[s] = 1;
        drained++;
    }
    check("drained 96 sectors (4..99, plus none repeated)", drained, 96);
    check("no duplicates while draining", dup, 0);
    check("free count == 0", freeCount(), 0);
    check("Remove on empty list == -1", Remove(), -1);

    /* Release everything (1..99) and confirm the list is whole again. */
    int inserted = 0;
    for (int i = 1; i < NUM_SECTORS; i++)
        inserted += Insert(i);
    check("re-inserted 99 sectors", inserted, 99);
    check("free count == 99", freeCount(), 99);

    destroyList();
    check("free count after destroyList == 0", freeCount(), 0);

    printf("\n%s (%d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;