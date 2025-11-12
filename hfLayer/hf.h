#ifndef HF_H
#define HF_H

#include "pf.h"

/* ---------------------------------------
 * Heap File (HF) public API
 * -------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

/* Record Identifier (RID) */
typedef struct {
    int   page;   /* PF page number */
    short slot;   /* slot index (0-based) */
} HF_RID;

/* Opaque scan handle */
typedef struct {
    int  fd;          /* HF file descriptor (PF fd) */
    int  curPage;     /* current page for scan */
    int  curSlot;     /* current slot index */
    int  endOfScan;   /* boolean */
} HF_Scan;

/* ---- File operations ---- */
int HF_CreateFile(const char* fname);
int HF_DestroyFile(const char* fname);
int HF_OpenFile(const char* fname);       /* returns PF fd */
int HF_CloseFile(int fd);

/* ---- Record CRUD ---- */
int HF_InsertRecord(int fd, const void* rec, int len, HF_RID* outRid);
int HF_GetRecord(int fd, HF_RID rid, void* outBuf, int* inoutLen);
/* If newLen > oldLen and no space in-place: relocates record, returns new RID */
int HF_UpdateRecord(int fd, HF_RID* rid, const void* rec, int newLen);
int HF_DeleteRecord(int fd, HF_RID rid);

/* ---- Scan ---- */
int HF_ScanOpen(int fd, HF_Scan* scan);
int HF_ScanNext(HF_Scan* scan, HF_RID* rid, void* outBuf, int* inoutLen);
int HF_ScanClose(HF_Scan* scan);

#ifdef __cplusplus
}
#endif

#endif /* HF_H */
