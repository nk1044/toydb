/* testpf.c */
#include <stdio.h>
#include <stdlib.h>   // for exit()
#include "pf.h"
#include "pftypes.h"

#define FILE1 "file1"
#define FILE2 "file2"

/* function prototypes */
void writefile(const char *fname);
void readfile(const char *fname);
void printfile(int fd);

int main()
{
    int error;
    int i;
    int pagenum, *buf;
    int fd1, fd2;

    /* create a few files */
    if ((error = PF_CreateFile(FILE1)) != PFE_OK) {
        PF_PrintError("file1");
        exit(1);
    }
    printf("file1 created\n");

    if ((error = PF_CreateFile(FILE2)) != PFE_OK) {
        PF_PrintError("file2");
        exit(1);
    }
    printf("file2 created\n");

    /* write to files */
    writefile(FILE1);
    readfile(FILE1);

    writefile(FILE2);
    readfile(FILE2);

    /* open both files */
    if ((fd1 = PF_OpenFile(FILE1)) < 0) {
        PF_PrintError("open file1");
        exit(1);
    }
    printf("opened file1\n");

    if ((fd2 = PF_OpenFile(FILE2)) < 0) {
        PF_PrintError("open file2");
        exit(1);
    }
    printf("opened file2\n");

    /* dispose some pages */
    for (i = 0; i < PF_MAX_BUFS; i++) {
        if (i & 1) {
            if ((error = PF_DisposePage(fd1, i)) != PFE_OK) {
                PF_PrintError("dispose");
                exit(1);
            }
            printf("disposed %d of file1\n", i);
        } else {
            if ((error = PF_DisposePage(fd2, i)) != PFE_OK) {
                PF_PrintError("dispose");
                exit(1);
            }
            printf("disposed %d of file2\n", i);
        }
    }

    if ((error = PF_CloseFile(fd1)) != PFE_OK) {
        PF_PrintError("close fd1");
        exit(1);
    }
    printf("closed file1\n");

    if ((error = PF_CloseFile(fd2)) != PFE_OK) {
        PF_PrintError("close fd2");
        exit(1);
    }
    printf("closed file2\n");

    /* print the files */
    readfile(FILE1);
    readfile(FILE2);

    /* destroy the files */
    if ((error = PF_DestroyFile(FILE1)) != PFE_OK) {
        PF_PrintError("destroy file1");
        exit(1);
    }
    if ((error = PF_DestroyFile(FILE2)) != PFE_OK) {
        PF_PrintError("destroy file2");
        exit(1);
    }

    /* create them again */
    if ((error = PF_CreateFile(FILE1)) != PFE_OK) {
        PF_PrintError("create file1");
        exit(1);
    }
    printf("file1 created\n");

    if ((error = PF_CreateFile(FILE2)) != PFE_OK) {
        PF_PrintError("create file2");
        exit(1);
    }
    printf("file2 created\n");

    /* write to files again */
    writefile(FILE1);
    writefile(FILE2);

    /* open both files */
    if ((fd1 = PF_OpenFile(FILE1)) < 0) {
        PF_PrintError("open file1");
        exit(1);
    }
    printf("opened file1\n");

    if ((fd2 = PF_OpenFile(FILE2)) < 0) {
        PF_PrintError("open file2");
        exit(1);
    }
    printf("opened file2\n");

    /* allocate additional pages */
    for (i = PF_MAX_BUFS; i < PF_MAX_BUFS * 2; i++) {
        if ((error = PF_AllocPage(fd2, &pagenum,  (char **)&buf)) != PFE_OK) {
            PF_PrintError("alloc page fd2");
            exit(1);
        }
        *buf = i;
        if ((error = PF_UnfixPage(fd2, pagenum, 1)) != PFE_OK) {
            PF_PrintError("unfix page fd2");
            exit(1);
        }
        printf("alloc %d file2, page %d\n", i, pagenum);

        if ((error = PF_AllocPage(fd1, &pagenum,  (char **)&buf)) != PFE_OK) {
            PF_PrintError("alloc page fd1");
            exit(1);
        }
        *buf = i;
        if ((error = PF_UnfixPage(fd1, pagenum, 1)) != PFE_OK) {
            PF_PrintError("unfix page fd1");
            exit(1);
        }
        printf("alloc %d file1, page %d\n", i, pagenum);
    }

    /* dispose some pages */
    for (i = PF_MAX_BUFS; i < PF_MAX_BUFS * 2; i++) {
        if (i & 1) {
            if ((error = PF_DisposePage(fd1, i)) != PFE_OK) {
                PF_PrintError("dispose fd1");
                exit(1);
            }
            printf("dispose fd1 page %d\n", i);
        } else {
            if ((error = PF_DisposePage(fd2, i)) != PFE_OK) {
                PF_PrintError("dispose fd2");
                exit(1);
            }
            printf("dispose fd2 page %d\n", i);
        }
    }

    /* print files */
    printfile(fd2);
    printfile(fd1);

    /* close files */
    PF_CloseFile(fd1);
    PF_CloseFile(fd2);

    /* print buffer and hash table */
    printf("buffer:\n");
    PFbufPrint();
    printf("hash table:\n");
    PFhashPrint();

    return 0;
}

/*---------------------------------------*/
/* writefile function */
void writefile(const char *fname)
{
    int i, fd, pagenum, *buf;
    int error;

    if ((fd = PF_OpenFile(fname)) < 0) {
        PF_PrintError("open file");
        exit(1);
    }
    printf("opened %s\n", fname);

    for (i = 0; i < PF_MAX_BUFS; i++) {
        if ((error = PF_AllocPage(fd, &pagenum, (char **)&buf)) != PFE_OK) {
            PF_PrintError("alloc page");
            exit(1);
        }
        *buf = i;
        printf("allocated page %d\n", pagenum);
    }

    if ((error = PF_AllocPage(fd, &pagenum, (char **)&buf)) == PFE_OK) {
        printf("too many buffers, still OK!\n");
        exit(1);
    }

    for (i = 0; i < PF_MAX_BUFS; i++) {
        if ((error = PF_UnfixPage(fd, i, 1)) != PFE_OK) {
            PF_PrintError("unfix page");
            exit(1);
        }
    }

    if ((error = PF_CloseFile(fd)) != PFE_OK) {
        PF_PrintError("close file");
        exit(1);
    }
}

/*---------------------------------------*/
/* readfile function */
void readfile(const char *fname)
{
    int error, fd;
    int *buf;

    if ((fd = PF_OpenFile(fname)) < 0) {
        PF_PrintError("open file");
        exit(1);
    }

    printfile(fd);

    if ((error = PF_CloseFile(fd)) != PFE_OK) {
        PF_PrintError("close file");
        exit(1);
    }
}

/*---------------------------------------*/
/* printfile function */
void printfile(int fd)
{
    int error;
    int *buf;
    int pagenum = -1;

    printf("reading file\n");
    while ((error = PF_GetNextPage(fd, &pagenum, (char **)&buf)) == PFE_OK) {
        printf("got page %d, %d\n", pagenum, *buf);
        if ((error = PF_UnfixPage(fd, pagenum, 0)) != PFE_OK) {
            PF_PrintError("unfix page");
            exit(1);
        }
    }

    if (error != PFE_EOF) {
        PF_PrintError("not eof");
        exit(1);
    }
    printf("eof reached\n");
}
