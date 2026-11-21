/*
 * dir.c - Directory sector handling
 */
#include <stdio.h>
#include <string.h>
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