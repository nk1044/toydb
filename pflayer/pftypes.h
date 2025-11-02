/* pftypes.h: declarations for Paged File Interface */

#ifndef PFTYPES_H_
#define PFTYPES_H_

#include "pf.h"

/**************************** File Page Decls *********************/
/* Each file contains a header, which is an integer pointing to the first free page,
   or -1 if no more free pages in the file. */
typedef struct PFhdr_str {
    int firstfree;  /* first free page in the linked list */
    int numpages;   /* total number of pages in the file */
} PFhdr_str;

#define PF_HDR_SIZE sizeof(PFhdr_str)

/* actual page struct to be written to the file */
#define PF_PAGE_LIST_END -1
#define PF_PAGE_USED -2

typedef struct PFfpage {
    int nextfree;                 /* page number of next free page, or PF_PAGE_LIST_END / PF_PAGE_USED */
    char pagebuf[PF_PAGE_SIZE];   /* actual page data */
} PFfpage;

/*************************** Opened File Table **********************/
#define PF_FTAB_SIZE 20

typedef struct PFftab_ele {
    char *fname;        /* file name, NULL if entry not used */
    int unixfd;         /* unix file descriptor */
    PFhdr_str hdr;      /* file header */
    short hdrchanged;   /* TRUE if header has changed */
} PFftab_ele;

/************************** Buffer Page Decls *********************/
#define PF_MAX_BUFS 20

typedef struct PFbpage {
    struct PFbpage *nextpage;
    struct PFbpage *prevpage;
    short dirty:1;      /* TRUE if page is dirty */
    short fixed:1;      /* TRUE if page is fixed in buffer */
    int page;           /* page number */
    int fd;             /* file descriptor */
    PFfpage fpage;      /* file data */
} PFbpage;

/******************** Hash Table Decls ****************************/
#define PF_HASH_TBL_SIZE 20

typedef struct PFhash_entry {
    struct PFhash_entry *nextentry;
    struct PFhash_entry *preventry;
    int fd;
    int page;
    PFbpage *bpage;    /* pointer to buffer holding this page */
} PFhash_entry;

#define PFhash(fd,page) (((fd)+(page)) % PF_HASH_TBL_SIZE)

/******************* Interface functions from Hash Table ****************/
extern void PFhashInit(void);
extern PFbpage *PFhashFind(int fd, int page);
extern int PFhashInsert(int fd, int page, PFbpage *bpage);
extern int PFhashDelete(int fd, int page);
extern void PFhashPrint(void);

/****************** Interface functions from Buffer Manager *************/
extern int PFbufGet(int fd, int pagenum, PFfpage **fpage,
                    int (*readfcn)(int, int, PFfpage *),
                    int (*writefcn)(int, int, PFfpage *));

extern int PFbufUnfix(int fd, int pagenum, int dirty);
extern int PFbufAlloc(int fd, int pagenum, PFfpage **fpage, int (*writefcn)(int, int, PFfpage *));
extern int PFbufReleaseFile(int fd, int (*writefcn)(int, int, PFfpage *));
extern int PFbufUsed(int fd, int pagenum);
extern void PFbufPrint(void);

#endif /* PFTYPES_H_ */
