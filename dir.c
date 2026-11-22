/*
 * dir.c - Directory sector handling
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "fs.h"

union block sector[100];

/*
 * Verify that the compiler's layout matches the required disk layout.
 * Sizes are checked with sizeof rather than assumed from the field sizes.
 */
int checkLayout(void)
{
    size_t intSize   = sizeof(int);
    size_t entrySize = sizeof(sector[0].directory.dir[0]);
    size_t blockSize = sizeof(union block);
    int ok = 1;

    if (intSize != REQ_INT_SIZE) {
        fprintf(stderr, "Layout error: sizeof(int) is %zu, required %d\n",
                intSize, REQ_INT_SIZE);
        ok = 0;
    }
    if (entrySize != REQ_ENTRY_SIZE) {
        fprintf(stderr, "Layout error: directory entry is %zu bytes, required %d\n",
                entrySize, REQ_ENTRY_SIZE);
        ok = 0;
    }
    if (blockSize != REQ_BLOCK_SIZE) {
        fprintf(stderr, "Layout error: union block is %zu bytes, required %d\n",
                blockSize, REQ_BLOCK_SIZE);
        ok = 0;
    }
    return ok;
}

/*
 * Initialize sector s as an empty directory sector: all 25 entries are
 * marked F with empty names, link 0 and size 0, and free is set to 25.
 */
void initDir(int s, int back, int frwd)
{
    sector[s].directory.back = back;
    sector[s].directory.frwd = frwd;
    sector[s].directory.free = DIR_ENTRIES;

    for (int i = 0; i < DIR_ENTRIES; i++) {
        sector[s].directory.dir[i].type = 'F';
        memset(sector[s].directory.dir[i].name, 0,
               sizeof sector[s].directory.dir[i].name);
        sector[s].directory.dir[i].link = 0;
        sector[s].directory.dir[i].size = 0;
    }
}

/* The root directory lives in sector 0; both links are -1 per the spec. */
void initRoot(void)
{
    initDir(0, -1, -1);
}

/* ------------------------------------------------------------------ */
/* Directory navigation and entry management                           */
/* ------------------------------------------------------------------ */

int curDir = 0;     /* first sector of the current directory (0 = root) */

/*
 * Search the directory chain that starts at dirStart for an occupied entry
 * called name.  On success *sec and *idx identify the entry.
 */
int findEntry(int dirStart, const char *name, int *sec, int *idx)
{
    int s = dirStart;
    for (;;) {
        for (int i = 0; i < DIR_ENTRIES; i++) {
            if (sector[s].directory.dir[i].type != 'F' &&
                strcmp(sector[s].directory.dir[i].name, name) == 0) {
                *sec = s;
                *idx = i;
                return 1;
            }
        }
        int next = sector[s].directory.frwd;
        if (next <= 0) break;           /* 0 (or -1 for the root) ends the chain */
        s = next;
    }
    return 0;
}

/* Find an unused entry in the chain without changing anything. */
int findFreeSlot(int dirStart, int *sec, int *idx)
{
    int s = dirStart;
    for (;;) {
        if (sector[s].directory.free > 0) {
            for (int i = 0; i < DIR_ENTRIES; i++) {
                if (sector[s].directory.dir[i].type == 'F') {
                    *sec = s;
                    *idx = i;
                    return 1;
                }
            }
        }
        int next = sector[s].directory.frwd;
        if (next <= 0) break;
        s = next;
    }
    return 0;
}

static int lastDirSector(int dirStart)
{
    int s = dirStart;
    while (sector[s].directory.frwd > 0)
        s = sector[s].directory.frwd;
    return s;
}

/*
 * Return a free entry in the directory.  If every sector of the directory
 * is full, allocate another sector and chain it on with back/frwd; this
 * extends the same directory and does not create a subdirectory.
 * Returns 0 if a new sector is needed but the disk is full.
 */
int getSlot(int dirStart, int *sec, int *idx)
{
    if (findFreeSlot(dirStart, sec, idx)) return 1;

    int last = lastDirSector(dirStart);
    int ns = Remove();
    if (ns < 0) return 0;

    initDir(ns, last, 0);
    sector[last].directory.frwd = ns;
    *sec = ns;
    *idx = 0;
    return 1;
}

static int validName(const char *name)
{
    size_t len = strlen(name);
    if (len == 0 || len > MAX_NAME) {
        printf("Error: names must be 1 to %d characters\n", MAX_NAME);
        return 0;
    }
    return 1;
}

/* CREATE U name  |  CREATE D name */
void cmdCreate(const char *typeStr, const char *name)
{
    char type = (char)toupper((unsigned char)typeStr[0]);
    if (strlen(typeStr) != 1 || (type != 'U' && type != 'D')) {
        printf("Usage: CREATE U|D name\n");
        return;
    }
    if (!validName(name)) return;

    int s, x;
    if (findEntry(curDir, name, &s, &x)) {
        printf("Error: '%s' already exists\n", name);
        return;
    }

    /* Count the sectors needed first so a failure changes nothing. */
    int need = (type == 'D') ? 1 : 0;
    if (!findFreeSlot(curDir, &s, &x)) need++;      /* directory must grow */
    if (freeCount() < need) {
        printf("Error: disk full\n");
        return;
    }

    getSlot(curDir, &s, &x);

    int link = 0;                       /* an empty file has no data sector */
    if (type == 'D') {
        link = Remove();
        initDir(link, -1, 0);
    }

    sector[s].directory.dir[x].type = type;
    strcpy(sector[s].directory.dir[x].name, name);
    sector[s].directory.dir[x].link = link;
    sector[s].directory.dir[x].size = 0;
    sector[s].directory.free--;

    printf("Created %s '%s'\n", type == 'D' ? "directory" : "file", name);
}

/* 1 if no sector in the directory chain starting at dirStart has an entry. */
static int dirIsEmpty(int dirStart)
{
    for (int s = dirStart; s > 0; s = sector[s].directory.frwd)
        if (sector[s].directory.free != DIR_ENTRIES) return 0;
    return 1;
}

/* Return every sector of a file's data chain to the free list. */
static void releaseDataChain(int s)
{
    while (s != 0) {
        int next = sector[s].data.frwdf;
        Insert(s);
        s = next;
    }
}

/* Return every sector of a directory chain to the free list. */
static void releaseDirChain(int s)
{
    while (s > 0) {
        int next = sector[s].directory.frwd;
        Insert(s);
        s = next;
    }
}

/*
 * An extension sector (not the first sector of its directory) that has
 * become completely empty is unlinked from the chain and freed.
 */
static void shrinkDir(int s)
{
    if (s == curDir || sector[s].directory.free != DIR_ENTRIES) return;

    int back = sector[s].directory.back;
    int frwd = sector[s].directory.frwd;
    sector[back].directory.frwd = frwd;
    if (frwd > 0) sector[frwd].directory.back = back;
    Insert(s);
}

/*
 * DELETE name: remove a regular file or an empty subdirectory from the
 * current directory.  All of its sectors go back on the free list.
 */
void cmdDelete(const char *name)
{
    if (!validName(name)) return;

    int s, x;
    if (!findEntry(curDir, name, &s, &x)) {
        printf("Error: '%s' not found\n", name);
        return;
    }

    char type = sector[s].directory.dir[x].type;
    int  link = sector[s].directory.dir[x].link;

    if (type == 'U') {
        if (fileIsOpen(s, x)) {
            printf("Error: '%s' is open; CLOSE it first\n", name);
            return;
        }
        releaseDataChain(link);
    } else {
        /* The root is never an entry of any directory, so link > 0 here. */
        if (link <= 0) {
            printf("Error: the root directory cannot be deleted\n");
            return;
        }
        if (!dirIsEmpty(link)) {
            printf("Error: directory '%s' is not empty\n", name);
            return;
        }
        releaseDirChain(link);
    }

    sector[s].directory.dir[x].type = 'F';
    memset(sector[s].directory.dir[x].name, 0,
           sizeof sector[s].directory.dir[x].name);
    sector[s].directory.dir[x].link = 0;
    sector[s].directory.dir[x].size = 0;
    sector[s].directory.free++;

    shrinkDir(s);

    printf("Deleted %s '%s'\n", type == 'D' ? "directory" : "file", name);
}

/* DIR: list every occupied entry in all sectors of the current directory. */
void cmdDir(void)
{
    int count = 0;
    int s = curDir;
    for (;;) {
        for (int i = 0; i < DIR_ENTRIES; i++) {
            if (sector[s].directory.dir[i].type == 'F') continue;

            if (sector[s].directory.dir[i].type == 'D')
                printf("  D  %-8s\n", sector[s].directory.dir[i].name);
            else
                printf("  U  %-8s %d bytes\n", sector[s].directory.dir[i].name,
                       sector[s].directory.dir[i].size);
            count++;
        }
        int next = sector[s].directory.frwd;
        if (next <= 0) break;
        s = next;
    }
    if (count == 0) printf("  (empty)\n");
}

/* CD name: move into a subdirectory of the current directory. */
void cmdCd(const char *name)
{
    if (!validName(name)) return;

    int s, x;
    if (!findEntry(curDir, name, &s, &x)) {
        printf("Error: '%s' not found\n", name);
        return;
    }
    if (sector[s].directory.dir[x].type != 'D') {
        printf("Error: '%s' is not a directory\n", name);
        return;
    }
    curDir = sector[s].directory.dir[x].link;
    printf("Current directory: %s\n", name);
}

/* ROOT: return to the root directory (sector 0). */
void cmdRoot(void)
{
    curDir = 0;
    printf("Current directory: root\n");
}