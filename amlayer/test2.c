/* test2.c */
#include <stdio.h>
#include "am.h"
#include "testam.h"

#define STRING_SIZE 250
#define MAXRECS 100
#define FNAME_LENGTH 80

int main(void)
{
    int fd;
    char fname[FNAME_LENGTH];
    char buf[STRING_SIZE];
    int recnum;
    int sd;
    int numrec;

    printf("initializing\n");
    PF_Init();

    printf("creating index\n");
    xAM_CreateIndex(RELNAME, 0, CHAR_TYPE, STRING_SIZE);

    printf("opening index\n");
    sprintf(fname, "%s.0", RELNAME);
    fd = xPF_OpenFile(fname);

    printf("inserting into index\n");
    for (recnum = 0; recnum < MAXRECS; recnum++) {
        sprintf(buf, "%d", recnum);
        padstring(buf, STRING_SIZE);
        xAM_InsertEntry(fd, CHAR_TYPE, STRING_SIZE,
                        buf, IntToRecId(recnum));
    }

    printf("opening index scan on char\n");
    sd = xAM_OpenIndexScan(fd, CHAR_TYPE, STRING_SIZE, EQ_OP, NULL);

    printf("retrieving recid's from scan descriptor %d\n", sd);
    numrec = 0;

    while ((recnum = RecIdToInt(xAM_FindNextEntry(sd))) >= 0) {
        printf("%d\n", recnum);
        numrec++;
    }
    printf("retrieved %d records\n", numrec);

    printf("closing down\n");
    xAM_CloseIndexScan(sd);
    xPF_CloseFile(fd);
    xAM_DestroyIndex(RELNAME, 0);

    printf("test2 done!\n");
    return 0;
}
