/*
 * fs.h - Shared definitions for the file system
 */
#ifndef FS_H
#define FS_H

#define NUM_SECTORS 100
#define DIR_ENTRIES 25
#define DATA_BYTES  504
#define MAX_NAME    8

/* Required sizes */
#define REQ_INT_SIZE   4
#define REQ_ENTRY_SIZE 20
#define REQ_BLOCK_SIZE 512

/* A sector holds either a directory structure or a file-data structure. */
union block {
    struct {
        int back;       /* previous sector in the same directory */
        int frwd;       /* next sector in the same directory     */
        int free;       /* unused entries in this sector         */
        struct {
            char  type;     /* 'U' file, 'D' directory, 'F' unused */
            char  name[9];  /* null-terminated, max 8 characters   */
            int   link;     /* first data / directory sector       */
            short size;     /* file size in bytes (0 for dirs)     */
        } dir[25];
    } directory;
    struct {
        int  backf;         /* previous sector of the file, or 0 */
        int  frwdf;         /* next sector of the file, or 0     */
        char userdata[504]; /* file contents                     */
    } data;
};

/* The simulated disk (defined in dir.c) */
extern union block sector[100];

/* ---- dir.c ---- */
int  checkLayout(void);                         /* 1 if sizes are correct, else 0 */
void initDir(int s, int back, int frwd);        /* initialize a directory sector  */
void initRoot(void);                            /* initialize sector[0]           */

#endif /* FS_H */