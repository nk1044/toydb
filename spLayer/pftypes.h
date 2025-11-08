#ifndef PFTYPES_H_
#define PFTYPES_H_

#define PF_PAGE_SIZE 4096
/**************************** File Page Decls *****************************/
typedef struct PFhdr_str {
    int firstfree;  /* first free page in the linked list */
    int numpages;   /* total number of pages in the file */
} PFhdr_str;

#define PF_HDR_SIZE sizeof(PFhdr_str)

/* Page markers */
#define PF_PAGE_LIST_END -1
#define PF_PAGE_USED     -2

typedef struct PFfpage {
    int nextfree;
    char pagebuf[PF_PAGE_SIZE];
} PFfpage;

/*************************** Opened File Table ****************************/
#define PF_FTAB_SIZE 20

typedef struct PFftab_ele {
    char *fname;
    int unixfd;
    PFhdr_str hdr;
    short hdrchanged;
} PFftab_ele;

/************************** Buffer Page Decls *****************************/
#define PF_MAX_BUFS 20

typedef struct PFbpage {
    struct PFbpage *nextpage;
    struct PFbpage *prevpage;
    unsigned short dirty:1;
    unsigned short fixed:1;
    int page;
    int fd;
    PFfpage fpage;
} PFbpage;

/******************** Hash Table Decls ****************************/
#define PF_HASH_TBL_SIZE 20

typedef struct PFhash_entry {
    struct PFhash_entry *nextentry;
    struct PFhash_entry *preventry;
    int fd;
    int page;
    PFbpage *bpage;
} PFhash_entry;

#define PFhash(fd, page) (((fd) + (page)) % PF_HASH_TBL_SIZE)

/******************* Interface functions from Hash Table ****************/
extern void PFhashInit(void);
extern PFbpage *PFhashFind(int fd, int page);
extern int PFhashInsert(int fd, int page, PFbpage *bpage);
extern int PFhashDelete(int fd, int page);
extern void PFhashPrint(void);

#endif /* PFTYPES_H_ */
