#ifndef PF_H_
#define PF_H_

#include <stdio.h>

/* bring in all PF-related structs */
#include "pftypes.h"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/************** Error Codes *********************************/
#define PFE_OK              0
#define PFE_NOMEM          -1
#define PFE_NOBUF          -2
#define PFE_PAGEFIXED      -3
#define PFE_PAGENOTINBUF   -4
#define PFE_UNIX           -5
#define PFE_INCOMPLETEREAD -6
#define PFE_INCOMPLETEWRITE -7
#define PFE_HDRREAD        -8
#define PFE_HDRWRITE       -9
#define PFE_INVALIDPAGE    -10
#define PFE_FILEOPEN       -11
#define PFE_FTABFULL       -12
#define PFE_FD             -13
#define PFE_EOF            -14
#define PFE_PAGEFREE       -15
#define PFE_PAGEUNFIXED    -16
#define PFE_PAGEINBUF      -17
#define PFE_HASHNOTFOUND   -18
#define PFE_HASHPAGEEXIST  -19

/* Page size */
#define PF_PAGE_SIZE 4096

/* Global error variable */
extern int PFerrno;

extern int USE_MRU;
/**************** Function Declarations ****************/

/* Initialization and utilities */
void PF_Init(void); // initialize pf file table and pf hash table
void PF_PrintError( char *s); // print the last pf error with a given string

/* File operations */
int PF_CreateFile(const char *fname); // create a paged file called "fname" with file header initialized to zero
int PF_DestroyFile(const char *fname); // destroy the paged file named "fname" if it is not open
int PF_OpenFile(const char *fname); // open the paged file named "fname" and return its file descriptor
int PF_CloseFile(int fd); // close the paged file with file descriptor "fd" and write back the header if it has been changed

/* Page operations */
int PF_AllocPage(int fd, int *pagenum, char **buf);
int PF_GetNextPage(int fd, int *pagenum, char **pagebuf);
int PF_UnfixPage(int fd, int pagenum, int dirty);
int PF_DisposePage(int fd, int pagenum);
int PFreadfcn(int fd, int pagenum, PFfpage *buf);
int PFwritefcn(int fd, int pagenum, PFfpage *buf);
int PFreadhdr(int fd, PFhdr_str *hdr);
int PFwritehdr(int fd, PFhdr_str *hdr);
int PF_GetThisPage(int fd, int pagenum, char **pagebuf);
int PF_GetFirstPage(int fd, int *pagenum, char **pagebuf);

/* Buffer and hash debug prints */
void PFbufPrint(void);
void PFhashPrint(void);
int PFbufReleaseFile(int fd, int (*writefcn)(int, int, PFfpage *));
int PFbufAlloc(int fd, int pagenum, PFfpage **fpage, int (*writefcn)(int, int, PFfpage *)) ;
int PFbufGet(int fd, int pagenum, PFfpage **fpage, int (*readfcn)(int, int, PFfpage *), int (*writefcn)(int, int, PFfpage *));
int PFbufUnfix(int fd, int pagenum, int dirty);

#endif /* PF_H_ */
