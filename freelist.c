/*
 * freelist.c - Free-sector management
 */
#include <stdio.h>
#include <stdlib.h>
#include "fs.h"

static struct nodeType *freeList = NULL;

static struct nodeType *newNode(int info, struct nodeType *next)
{
    struct nodeType *n = malloc(sizeof *n);
    if (!n) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    n->info = info;
    n->link = next;
    return n;
}

/* Create the initial list holding sector numbers 1 through 99, ascending. */
void buildList(void)
{
    destroyList();
    for (int i = NUM_SECTORS - 1; i >= 1; i--)
        freeList = newNode(i, freeList);
}

/* Remove one sector number from the list and return it; -1 if none left. */
int Remove(void)
{
    if (!freeList) return -1;

    struct nodeType *n = freeList;
    int s = n->info;
    freeList = n->link;
    free(n);
    return s;
}

/*
 * Return a released sector to the list.  The list is kept in ascending
 * order, so the lowest free sector is always allocated first.  Rejects
 * sector 0, out-of-range numbers, and sectors already in the list so a
 * number can never appear twice.  Returns 1 on success, 0 on rejection.
 */
int Insert(int s)
{
    if (s < 1 || s >= NUM_SECTORS) return 0;

    struct nodeType **pp = &freeList;
    while (*pp && (*pp)->info < s)
        pp = &(*pp)->link;

    if (*pp && (*pp)->info == s) return 0;      /* already free */

    *pp = newNode(s, *pp);
    return 1;
}

int freeCount(void)
{
    int c = 0;
    for (struct nodeType *n = freeList; n; n = n->link) c++;
    return c;
}

/* Release every node (called at EXIT). */
void destroyList(void)
{
    while (freeList) {
        struct nodeType *n = freeList;
        freeList = n->link;
        free(n);
    }
}