/*
 * main.c - Program entry point
 */
#include <stdio.h>
#include "fs.h"

int main(void)
{
    if (!checkLayout()) {
        fprintf(stderr, "Incorrect disk layout; not continuing.\n");
        return 1;
    }

    initRoot();

    /* Stage 1 self-check output */
    printf("sizeof(int)             = %zu\n", sizeof(int));
    printf("sizeof(dir entry)       = %zu\n", sizeof(sector[0].directory.dir[0]));
    printf("sizeof(union block)     = %zu\n", sizeof(union block));
    printf("sizeof(sector)          = %zu\n", sizeof(sector));
    printf("root: back=%d frwd=%d free=%d\n",
           sector[0].directory.back,
           sector[0].directory.frwd,
           sector[0].directory.free);

    int unused = 0;
    for (int i = 0; i < DIR_ENTRIES; i++)
        if (sector[0].directory.dir[i].type == 'F') unused++;
    printf("root: %d of %d entries marked F\n", unused, DIR_ENTRIES);

    return 0;
}