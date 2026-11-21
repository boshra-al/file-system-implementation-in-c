/*
 * main.c - Program entry point
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "fs.h"

#define LINE_LEN 256
#define MAX_TOKENS 4

static void printHelp(void)
{
    printf("Commands:\n"
           "  CREATE U|D name   create a file (U) or directory (D)\n"
           "  DIR               list the current directory\n"
           "  CD name           change to a subdirectory\n"
           "  ROOT              return to the root directory\n"
           "  HELP              show this list\n"
           "  EXIT              quit\n");
}

int main(void)
{
    if (!checkLayout()) {
        fprintf(stderr, "Incorrect disk layout; not continuing.\n");
        return 1;
    }

    initRoot();             /* sector[0] becomes the empty root directory */
    buildList();            /* free sectors 1..99                         */
    curDir = 0;             /* start in the root                          */

    printf("Simple File System ready (type HELP for commands)\n");

    char line[LINE_LEN];
    for (;;) {
        printf("fs> ");
        fflush(stdout);

        if (!fgets(line, sizeof line, stdin)) break;        /* end of input */

        /* Discard the rest of an over-long line. */
        if (!strchr(line, '\n')) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF) { }
        }

        char *tok[MAX_TOKENS];
        int nt = 0;
        for (char *t = strtok(line, " \t\r\n"); t && nt < MAX_TOKENS;
             t = strtok(NULL, " \t\r\n"))
            tok[nt++] = t;
        if (nt == 0) continue;

        for (char *c = tok[0]; *c; c++)
            *c = (char)toupper((unsigned char)*c);
        const char *cmd = tok[0];

        if (strcmp(cmd, "CREATE") == 0) {
            if (nt < 3) printf("Usage: CREATE U|D name\n");
            else        cmdCreate(tok[1], tok[2]);
        } else if (strcmp(cmd, "DIR") == 0) {
            cmdDir();
        } else if (strcmp(cmd, "CD") == 0) {
            if (nt < 2) printf("Usage: CD name\n");
            else        cmdCd(tok[1]);
        } else if (strcmp(cmd, "ROOT") == 0) {
            cmdRoot();
        } else if (strcmp(cmd, "HELP") == 0) {
            printHelp();
        } else if (strcmp(cmd, "EXIT") == 0) {
            break;
        } else {
            printf("Unknown command '%s' (type HELP)\n", cmd);
        }
    }

    destroyList();          /* release the free-list nodes */
    return 0;
}