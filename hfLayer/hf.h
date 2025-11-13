#ifndef HF_H
#define HF_H

#include "../pflayer/pf.h"

#define HF_SCAN_CLOSED 1
#define HF_OK 0

#define HF_MAX_FILE 20

typedef struct {
    int pageNum;
    int slotNum;
    int recordLen;
} HF_RID;

typedef struct {
    int fd;          // HF file descriptor
    int curPage;     // current page number
    int curSlot;     // current slot number
    int totalPages;  // total pages in HF file
    int isOpen;      // indicates scan is open
} HF_Scan;

/* HF API */
int HF_CreateFile(const char *fname);
int HF_OpenFile(const char *fname);
int HF_CloseFile(int hffd);

int HF_InsertRecord(int hffd, const void *record, int len, HF_RID *rid);

int HF_ScanOpen(int hffd, HF_Scan *scan);
int HF_ScanNext(HF_Scan *scan, HF_RID *rid, void *recBuf, int *recLen);
int HF_ScanClose(HF_Scan *scan);

#endif
