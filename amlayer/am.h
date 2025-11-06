#ifndef AM_H
#define AM_H

#include <stddef.h>

/* ===== Types ===== */

typedef struct am_leafheader {
    char  pageType;
    int   nextLeafPage;
    short recIdPtr;
    short keyPtr;
    short freeListPtr;
    short numinfreeList;
    short attrLength;
    short numKeys;
    short maxKeys;
} AM_LEAFHEADER; /* Header for a leaf page */

typedef struct am_intheader {
    char  pageType;
    short numKeys;
    short maxKeys;
    short attrLength;
} AM_INTHEADER; /* Header for an internal node */

/* ===== Globals ===== */
extern int AM_RootPageNum;  /* The page number of the root */
extern int AM_LeftPageNum;  /* The page Number of the leftmost leaf */
extern int AM_Errno;        /* last error in AM layer */

/* ===== Legacy-size helpers ===== */
#define AM_si   ((int)sizeof(int))
#define AM_ss   ((int)sizeof(short))
#define AM_sl   ((int)sizeof(AM_LEAFHEADER))
#define AM_sint ((int)sizeof(AM_INTHEADER))
#define AM_sc   ((int)sizeof(char))
#define AM_sf   ((int)sizeof(float))

/* ===== Constants ===== */
#define AM_NOT_FOUND 0
#define AM_FOUND 1
#define AM_NULL 0
#define AM_MAX_FNAME_LENGTH 80
#define AM_NULL_PAGE -1

#define FREE 0
#define FIRST 1
#define BUSY 2
#define LAST 3
#define OVER 4

#define ALL 0
#define EQUAL 1
#define LESS_THAN 2
#define GREATER_THAN 3
#define LESS_THAN_EQUAL 4
#define GREATER_THAN_EQUAL 5
#define NOT_EQUAL 6

#define MAXSCANS 20
#define AM_MAXATTRLENGTH 256

/* ===== Errors ===== */
#define AME_OK 0
#define AME_INVALIDATTRLENGTH -1
#define AME_NOTFOUND -2
#define AME_PF -3
#define AME_INTERROR -4
#define AME_INVALID_SCANDESC -5
#define AME_INVALID_OP_TO_SCAN -6
#define AME_EOF -7
#define AME_SCAN_TAB_FULL -8
#define AME_INVALIDATTRTYPE -9
#define AME_FD -10
#define AME_INVALIDVALUE -11

/* PF error forwarding macro expects `errVal` in scope */
#define AM_Check do { if (errVal != PFE_OK) { AM_Errno = AME_PF; return AME_PF; } } while (0)

/* ===== Forward declarations for project-wide helpers ===== */
void AM_bcopy(const void *src, void *dst, int nbytes);
#ifdef bcopy
#undef bcopy
#endif
#define bcopy AM_bcopy

#define AM_Check \
  do { if (errVal != PFE_OK) { AM_Errno = AME_PF; return AME_PF; } } while (0)



/* ===== am.c ===== */
int  AM_SplitLeaf(int fileDesc, char *pageBuf, int *pageNum, int attrLength,
                  int recId, char *value, int status, int index, char *key);
int  AM_AddtoParent(int fileDesc, int pageNum, char *value, int attrLength);
void AM_AddtoIntPage(char *pageBuf, char *value, int pageNum,
                     AM_INTHEADER *header, int offset);
void AM_FillRootPage(char *pageBuf, int pageNum1, int pageNum2, char *value,
                     short attrLength, short maxKeys);
void AM_SplitIntNode(char *pageBuf, char *pbuf1, char *pbuf2,
                     AM_INTHEADER *header, char *value, int pageNum, int offset);

/* ===== amfns.c ===== */
int AM_CreateIndex(char *fileName, int indexNo, char attrType, int attrLength);
int AM_DestroyIndex(char *fileName, int indexNo);
int AM_DeleteEntry(int fileDesc, char attrType, int attrLength, char *value, int recId);
int AM_InsertEntry(int fileDesc, char attrType, int attrLength, char *value, int recId);

/* ===== aminsert.c ===== */
int  AM_InsertintoLeaf(char *pageBuf, int attrLength, char *value, int recId, int index, int status);
void AM_InsertToLeafFound(char *pageBuf, int recId, int index, AM_LEAFHEADER *header);
void AM_InsertToLeafNotFound(char *pageBuf, char *value, int recId, int index, AM_LEAFHEADER *header);
void AM_Compact(int low, int high, char *pageBuf, char *tempPage, AM_LEAFHEADER *header);

/* ===== amprint.c ===== */
void AM_PrintIntNode(char *pageBuf, char attrType);
void AM_PrintLeafNode(char *pageBuf, char attrType);
int AM_DumpLeafPages(int fileDesc, int min, char attrType, int attrLength);
void AM_PrintLeafKeys(char *pageBuf, char attrType);
void AM_PrintAttr(char *bufPtr, char attrType, int attrLength);
void AM_PrintTree(int fileDesc, int pageNum, char attrType);

/* ===== amscan.c ===== */
int  AM_OpenIndexScan(int fileDesc, char attrType, int attrLength, int op, char *value);
int  AM_FindNextEntry(int scanDesc);
int  AM_CloseIndexScan(int scanDesc);        /* defined in amscan.c */
int  GetLeftPageNum(int fileDesc);           /* helper referenced by scan code */

/* ===== amsearch.c ===== */
int  AM_Search(int fileDesc, char attrType, int attrLength, char *value,
               int *pageNum, char **pageBuf, int *indexPtr);
int  AM_BinSearch(char *pageBuf, char attrType, int attrLength, char *value,
                  int *indexPtr, AM_INTHEADER *header);
int  AM_SearchLeaf(char *pageBuf, char attrType, int attrLength, char *value,
                   int *indexPtr, AM_LEAFHEADER *header);
int  AM_Compare(char *bufPtr, char attrType, int attrLength, char *valPtr);

/* ===== amstack.c ===== */
void AM_PushStack(int pageNum, int offset);
void AM_PopStack(void);
void AM_topofStack(int *pageNum, int *offset);
void AM_EmptyStack(void);

#endif /* AM_H */
