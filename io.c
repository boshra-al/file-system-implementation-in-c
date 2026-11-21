#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include "fs.h"

/* The size field in a directory entry is a short. */
#define MAX_FILE_SIZE SHRT_MAX

static struct {
    int  isOpen;
    char mode;      /* 'I' input, 'O' output, 'U' update */
    int  dirSec;    /* directory sector holding the file's entry */
    int  dirIdx;    /* index of the entry in that sector          */
    int  pos;       /* file pointer: byte offset from the start   */
    char name[9];
} of;

/* Startup: no file is open. */
void initOpenFile(void)
{
    of.isOpen = 0;
    of.mode   = 0;
    of.dirSec = -1;
    of.dirIdx = -1;
    of.pos    = 0;
    of.name[0] = '\0';
}

static int fileSize(void)
{
    return sector[of.dirSec].directory.dir[of.dirIdx].size;
}

/*
 Return the sector holding the j-th DATA_BYTES chunk of the open file
 (j = 0 is the first).  If grow is set, missing sectors are taken from
 the free list and chained on.  
 Returns -1 if the sector does not exist(or the disk is full when growing).
 */
static int dataSector(int j, int grow)
{
    int *link = &sector[of.dirSec].directory.dir[of.dirIdx].link;
    int prev = 0;
    int s = *link;

    for (int i = 0; ; i++) {
        if (s == 0) {   /* chain ends before chunk k */
            if (!grow) return -1;
            int ns = Remove();
            if (ns < 0) return -1;
            sector[ns].data.backf = prev;
            sector[ns].data.frwdf = 0;
            memset(sector[ns].data.userdata, 0, DATA_BYTES);
            if (prev == 0) *link = ns;
            else           sector[prev].data.frwdf = ns;
            s = ns;
        }
        if (i == j) return s;
        prev = s;
        s = sector[s].data.frwdf;
    }
}

/* Number of data sectors the open file already has. */
static int sectorCount(void)
{
    int c = 0;
    for (int s = sector[of.dirSec].directory.dir[of.dirIdx].link; s != 0;
         s = sector[s].data.frwdf)
        c++;
    return c;
}

static int needOpen(void)
{
    if (!of.isOpen) {
        printf("Error: no file is open\n");
        return 0;
    }
    return 1;
}

int fileIsOpen(const char *name)
{
    return of.isOpen && strcmp(of.name, name) == 0;
}

/* OPEN I/O/U name : open a user file in the current directory. */
void cmdOpen(const char *modeStr, const char *name)
{
    char mode = (char)toupper((unsigned char)modeStr[0]);
    if (strlen(modeStr) != 1 || (mode != 'I' && mode != 'O' && mode != 'U')) {
        printf("Usage: OPEN I|O|U name\n");
        return;
    }
    if (of.isOpen) {
        printf("Error: '%s' is already open; CLOSE it first\n", of.name);
        return;
    }

    int s, x;
    if (!findEntry(curDir, name, &s, &x)) {
        printf("Error: '%s' not found\n", name);
        return;
    }
    if (sector[s].directory.dir[x].type != 'U') {
        printf("Error: '%s' is a directory, not a file\n", name);
        return;
    }

    of.isOpen = 1;
    of.mode   = mode;
    of.dirSec = s;
    of.dirIdx = x;
    strcpy(of.name, sector[s].directory.dir[x].name);
    /* Input and update start at the beginning; output appends at the end. */
    of.pos = (mode == 'O') ? fileSize() : 0;

    printf("Opened '%s' for %s\n", of.name,
           mode == 'I' ? "input" : mode == 'O' ? "output" : "update");
}

/* CLOSE : close the open file. */
void cmdClose(void)
{
    if (!needOpen()) return;
    printf("Closed '%s'\n", of.name);
    of.isOpen = 0;
}

/*
 SEEK pos : set the file pointer to byte position pos (input or update).
 Positions start at 0; anything outisde 0 is rejected.
 SEEK never changes the file size.
 */
void cmdSeek(const char *posStr)
{
    if (!needOpen()) return;
    if (of.mode == 'O') {
        printf("Error: SEEK is not allowed in output mode\n");
        return;
    }

    char *end;
    long p = strtol(posStr, &end, 10);
    if (*end || end == posStr) {
        printf("Usage: SEEK position\n");
        return;
    }
    if (p < 0 || p > fileSize()) {
        printf("Error: position must be 0 to %d\n", fileSize());
        return;
    }
    of.pos = (int)p;
    printf("File pointer at byte %d (sector %d of the file, offset %d)\n",
           of.pos, of.pos / DATA_BYTES + 1, of.pos % DATA_BYTES);
}

/* READ n : print up to n bytes from the file pointer (input or update). */
void cmdRead(const char *nStr)
{
    if (!needOpen()) return;
    if (of.mode == 'O') {
        printf("Error: READ is not allowed in output mode\n");
        return;
    }

    char *end;
    long n = strtol(nStr, &end, 10);
    if (*end || n <= 0) {
        printf("Usage: READ n   (n > 0)\n");
        return;
    }

    int size = fileSize();
    int avail = size - of.pos;
    int count = n < avail ? (int)n : avail;

    putchar('"');
    for (int done = 0; done < count; ) {
        int k   = of.pos / DATA_BYTES;
        int off = of.pos % DATA_BYTES;
        int chunk = DATA_BYTES - off;
        if (chunk > count - done) chunk = count - done;

        int s = dataSector(k, 0);
        fwrite(sector[s].data.userdata + off, 1, (size_t)chunk, stdout);
        of.pos += chunk;
        done   += chunk;
    }
    putchar('"');
    putchar('\n');
    if (count < n) printf("End of file reached after %d byte(s)\n", count);
}

/*
 WRITE n 'data' : write n bytes at the file pointer (output or update).
 If data is shorter than n it is padded with blanks; if longer, only the
 first n bytes are written.  New sectors are allocated as the file grows.
 If the disk fills up, as much as fits is written.
 */
void cmdWrite(const char *nStr, const char *data)
{
    if (!needOpen()) return;
    if (of.mode == 'I') {
        printf("Error: WRITE is not allowed in input mode\n");
        return;
    }

    char *end;
    long n = strtol(nStr, &end, 10);
    if (*end || end == nStr || n <= 0) {
        printf("Usage: WRITE n data   (n > 0)\n");
        return;
    }
    if ((long)strlen(data) < n) {
        printf("Error: %ld byte(s) requested but only %zu given\n",
               n, strlen(data));
        return;
    }
    if (of.pos + n > MAX_FILE_SIZE) {
        printf("Error: file would exceed %d bytes\n", MAX_FILE_SIZE);
        return;
    }

    /* Count the new sectors first so a full disk changes nothing. */
    int needSectors = (int)((of.pos + n + DATA_BYTES - 1) / DATA_BYTES);
    int extra = needSectors - sectorCount();
    if (extra > freeCount()) {
        printf("Error: disk full (need %d more sector(s), %d free)\n",
               extra, freeCount());
        return;
    }

    for (long done = 0; done < n; ) {
        int k   = of.pos / DATA_BYTES;
        int off = of.pos % DATA_BYTES;
        int chunk = DATA_BYTES - off;
        if (chunk > n - done) chunk = (int)(n - done);

        int s = dataSector(k, 1);
        memcpy(sector[s].data.userdata + off, data + done, (size_t)chunk);
        of.pos += chunk;
        done   += chunk;
    }

    short *size = &sector[of.dirSec].directory.dir[of.dirIdx].size;
    if (of.pos > *size) *size = (short)of.pos;
    printf("Wrote %ld byte(s); '%s' is now %d bytes\n", n, of.name, *size);
}
