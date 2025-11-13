#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hf.h"
#include "../pflayer/pf.h"

typedef struct {
    int slotCount;
    int freeStart;    // where free space for data begins
    int freeEnd;      // where free space for slots ends
} HF_PageHdr;

#define HDR_SIZE sizeof(HF_PageHdr)

typedef struct {
    int offset;
    int length;
} HF_Slot;

typedef struct {
    int unixfd;
    int totalPages;
} HF_File;

static HF_File HFtable[HF_MAX_FILE];

/* --------------------------------------------------------- */
/* Initialize page */
static void HF_InitPage(char *page)
{
    HF_PageHdr *h = (HF_PageHdr *)page;
    h->slotCount = 0;
    h->freeStart = HDR_SIZE;
    h->freeEnd = PF_PAGE_SIZE;
}

/* --------------------------------------------------------- */
/* Create file */
int HF_CreateFile(const char *fname)
{
    return PF_CreateFile(fname);
}

/* --------------------------------------------------------- */
/* Open file */
int HF_OpenFile(const char *fname)
{
    int pfFd = PF_OpenFile(fname);
    if (pfFd < 0) {
        return pfFd;
    }

    int hffd;
    for (hffd = 0; hffd < HF_MAX_FILE; hffd++) {
        if (HFtable[hffd].unixfd == 0)
            break;
    }
    if (hffd == HF_MAX_FILE)
        return -1;

    HFtable[hffd].unixfd = pfFd;

    // Count pages
    int pageCount = 0;
    for (;;) {
        char *pg;
        int err = PF_GetThisPage(pfFd, pageCount, &pg);
        if (err < 0)
            break;
        PF_UnfixPage(pfFd, pageCount, 0);
        pageCount++;
    }
    HFtable[hffd].totalPages = pageCount;

    return hffd;
}

/* --------------------------------------------------------- */
/* Close file */
int HF_CloseFile(int hffd)
{
    if (hffd < 0 || hffd >= HF_MAX_FILE)
        return -1;

    int unixfd = HFtable[hffd].unixfd;
    HFtable[hffd].unixfd = 0;
    HFtable[hffd].totalPages = 0;

    return PF_CloseFile(unixfd);
}

/* --------------------------------------------------------- */
/* Insert record */
int HF_InsertRecord(int hffd, const void *record, int len, HF_RID *rid)
{
    HF_File *hf = &HFtable[hffd];
    int pfFd = hf->unixfd;

    int pageNum = hf->totalPages - 1;
    char *page;

    if (pageNum < 0) {
        // Allocate first page
        int err = PF_AllocPage(pfFd, &pageNum, &page);
        if (err < 0) return err;
        HF_InitPage(page);
        PF_UnfixPage(pfFd, pageNum, 1);
        hf->totalPages = 1;
    }

try_insert:
    PF_GetThisPage(pfFd, pageNum, &page);
    HF_PageHdr *h = (HF_PageHdr *)page;

    /* Compute available free space */
    int freeSpace = h->freeEnd - h->freeStart;
    int needSpace = len + sizeof(HF_Slot);

    if (freeSpace < needSpace) {
        PF_UnfixPage(pfFd, pageNum, 0);

        // Allocate a NEW page
        int newPage;
        char *newPg;
        int err = PF_AllocPage(pfFd, &newPage, &newPg);
        if (err < 0) return err;

        HF_InitPage(newPg);
        PF_UnfixPage(pfFd, newPage, 1);
        hf->totalPages++;

        pageNum = newPage;
        goto try_insert;
    }

    /* Insert data */
    int recOffset = h->freeStart;
    memcpy(page + recOffset, record, len);
    h->freeStart += len;

    /* Insert slot */
    h->freeEnd -= sizeof(HF_Slot);
    HF_Slot *slot = (HF_Slot *)(page + h->freeEnd);
    slot->offset = recOffset;
    slot->length = len;

    rid->pageNum = pageNum;
    rid->slotNum = h->slotCount;
    rid->recordLen = len;

    h->slotCount++;

    PF_UnfixPage(pfFd, pageNum, 1);

    return HF_OK;
}

/* --------------------------------------------------------- */
/* Scan open */
int HF_ScanOpen(int hffd, HF_Scan *scan)
{
    HF_File *hf = &HFtable[hffd];
    scan->fd = hffd;
    scan->curPage = 0;
    scan->curSlot = 0;
    scan->totalPages = hf->totalPages;
    scan->isOpen = 1;
    return HF_OK;
}

/* --------------------------------------------------------- */
/* Scan next */
int HF_ScanNext(HF_Scan *scan, HF_RID *rid, void *recBuf, int *recLen)
{
    if (!scan->isOpen)
        return HF_SCAN_CLOSED;

    HF_File *hf = &HFtable[scan->fd];
    int pfFd = hf->unixfd;

    for (;;) {
        if (scan->curPage >= scan->totalPages)
            return HF_SCAN_CLOSED;

        char *page;
        int err = PF_GetThisPage(pfFd, scan->curPage, &page);
        if (err < 0)
            return HF_SCAN_CLOSED;

        HF_PageHdr *h = (HF_PageHdr *)page;

        if (scan->curSlot < h->slotCount) {
            HF_Slot *slot = (HF_Slot *)(page + h->freeEnd + (h->slotCount - 1 - scan->curSlot) * sizeof(HF_Slot));

            *recLen = slot->length;
            memcpy(recBuf, page + slot->offset, *recLen);

            rid->pageNum = scan->curPage;
            rid->slotNum = scan->curSlot;
            rid->recordLen = *recLen;

            scan->curSlot++;

            PF_UnfixPage(pfFd, scan->curPage, 0);
            return HF_OK;
        }

        scan->curPage++;
        scan->curSlot = 0;
        PF_UnfixPage(pfFd, scan->curPage - 1, 0);
    }
}

/* --------------------------------------------------------- */
/* Scan close */
int HF_ScanClose(HF_Scan *scan)
{
    scan->isOpen = 0;
    return HF_OK;
}
