#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "hf.h"
#include "pf.h"

/* ================================
 * On-disk layout (per PF page)
 * ================================
 *
 * Page 0: Heap Meta Page
 *   struct HF_Meta {
 *     int firstFreePage;   // head of free-space list (-1 if none)
 *     int firstDataPage;   // first data page (usually 1) or -1 if none
 *     int reserved[6];     // padding for future use
 *   }
 *
 * Data Page (page >= 1):
 *   [HF_PageHeader] (from offset 0)
 *   ... record bytes (grow upward) ...
 *   ... free space ...
 *   ... slot directory entries (grow downward from end of page)
 *
 * Slot entry: (offset, length), length < 0 => tombstone (deleted)
 */

/* -----------------------------
 * Tunables / constants
 * ---------------------------*/
#define HF_META_PAGENUM          0
#define HF_NO_PAGE              (-1)
#define HF_DELETED_LEN          (-1)

/* Keep structs tightly packed on disk */
#pragma pack(push, 1)

typedef struct {
    int firstFreePage;     /* head of free-space list of data pages */
    int firstDataPage;     /* first data page for scans */
    int reserved[6];       /* pad for future expansion */
} HF_Meta;

typedef struct {
    int   nextFreePage;    /* link in free-space list; -1 => not listed */
    short slotCount;       /* total slots (some may be deleted) */
    short freeStart;       /* start offset of free space region (grows up) */
    short freeEnd;         /* end offset (exclusive) of free space region (grows down) */
} HF_PageHeader;

typedef struct {
    short offset;          /* byte offset of record in page body */
    short length;          /* length of record; -1 => deleted */
} HF_Slot;

#pragma pack(pop)

/* -----------------------------
 * Inline helpers
 * ---------------------------*/
static inline HF_Meta*        HF_META(char* page)       { return (HF_Meta*)page; }
static inline HF_PageHeader*  HF_PHEAD(char* page)      { return (HF_PageHeader*)page; }
static inline HF_Slot*        HF_SLOT(char* page, int i)
{
    /* slots grow downward from the end of page:
       slot i is at: PF_PAGE_SIZE - (i+1)*sizeof(HF_Slot) */
    return (HF_Slot*)(page + PF_PAGE_SIZE - (int)sizeof(HF_Slot) * (i+1));
}

static inline int HF_PageFreeBytes(char* page)
{
    HF_PageHeader* h = HF_PHEAD(page);
    return (int)h->freeEnd - (int)h->freeStart;
}

static inline int HF_SlotDirBytes(short slotCount)
{
    return (int)slotCount * (int)sizeof(HF_Slot);
}

static inline int HF_EffectiveFreeForInsert(char* page)
{
    HF_PageHeader* h = HF_PHEAD(page);
    /* free bytes minus space needed to grow slot directory by 1 */
    return HF_PageFreeBytes(page) - (int)sizeof(HF_Slot);
}

/* Mark / unmark page in the free-space list */
static int HF_LoadMeta(int fd, char** outPage)
{
    return PF_GetThisPage(fd, HF_META_PAGENUM, outPage);
}

static int HF_UnfixMeta(int fd, int dirty)
{
    return PF_UnfixPage(fd, HF_META_PAGENUM, dirty);
}

/* Add data page 'p' to free list head if not already listed */
static int HF_FreeListAdd(int fd, int p)
{
    int err; 
    char* pg;

    if ((err = PF_GetThisPage(fd, p, &pg)) != PFE_OK)
        return err;

    HF_PageHeader* h = HF_PHEAD(pg);

    // Already listed
    if (h->nextFreePage != HF_NO_PAGE) {
        return PF_UnfixPage(fd, p, 0);
    }

    // Load meta briefly
    char* metaPg;
    if ((err = HF_LoadMeta(fd, &metaPg)) != PFE_OK) {
        (void)PF_UnfixPage(fd, p, 0);
        return err;
    }

    int head = HF_META(metaPg)->firstFreePage;
    (void)HF_UnfixMeta(fd, 0);

    // Link into free list
    h->nextFreePage = head;
    if ((err = PF_UnfixPage(fd, p, 1)) != PFE_OK)
        return err;

    // Reopen meta just to write new head
    if ((err = HF_LoadMeta(fd, &metaPg)) != PFE_OK)
        return err;
    HF_META(metaPg)->firstFreePage = p;
    return HF_UnfixMeta(fd, 1);
}


/* Remove page 'p' from free list if listed */
static int HF_FreeListRemove(int fd, int p)
{
    int err; 
    char* metaPg;

    if ((err = HF_LoadMeta(fd, &metaPg)) != PFE_OK)
        return err;

    HF_Meta* m = HF_META(metaPg);
    int head = m->firstFreePage;
    if ((err = HF_UnfixMeta(fd, 0)) != PFE_OK)
        return err;

    // Case 1: Removing head of list
    if (head == p) {
        char* pg;
        if ((err = PF_GetThisPage(fd, p, &pg)) != PFE_OK)
            return err;

        HF_PageHeader* h = HF_PHEAD(pg);
        int newHead = h->nextFreePage;
        h->nextFreePage = HF_NO_PAGE;

        if ((err = PF_UnfixPage(fd, p, 1)) != PFE_OK)
            return err;

        if ((err = HF_LoadMeta(fd, &metaPg)) != PFE_OK)
            return err;

        HF_META(metaPg)->firstFreePage = newHead;
        return HF_UnfixMeta(fd, 1);
    }

    // Case 2: Walk the list without meta pinned
    int prev = head;
    while (prev != HF_NO_PAGE) {
        char* prevPg;
        if ((err = PF_GetThisPage(fd, prev, &prevPg)) != PFE_OK)
            return err;

        HF_PageHeader* ph = HF_PHEAD(prevPg);
        int next = ph->nextFreePage;

        if (next == p) {
            // Found target; unlink
            char* curPg;
            if ((err = PF_GetThisPage(fd, p, &curPg)) != PFE_OK) {
                (void)PF_UnfixPage(fd, prev, 0);
                return err;
            }

            HF_PageHeader* ch = HF_PHEAD(curPg);
            ph->nextFreePage = ch->nextFreePage;
            ch->nextFreePage = HF_NO_PAGE;

            int e1 = PF_UnfixPage(fd, p, 1);
            int e2 = PF_UnfixPage(fd, prev, 1);
            return (e1 != PFE_OK) ? e1 : e2;
        }

        (void)PF_UnfixPage(fd, prev, 0);
        prev = next;
    }

    return PFE_OK;
}

/* Update page position relative to free-list based on free bytes */
static int HF_FreeListUpdate(int fd, int p)
{
    int err; char* pg;
    if ((err = PF_GetThisPage(fd, p, &pg)) != PFE_OK) return err;

    HF_PageHeader* h = HF_PHEAD(pg);
    int eff = HF_EffectiveFreeForInsert(pg);
    int listed = (h->nextFreePage != HF_NO_PAGE);

    (void)PF_UnfixPage(fd, p, 0);

    if (eff > 0 && !listed)      return HF_FreeListAdd(fd, p);
    else if (eff <= 0 && listed) return HF_FreeListRemove(fd, p);
    else                         return PFE_OK;
}


/* Initialize a fresh data page header */
static void HF_InitDataPage(char* page)
{
    HF_PageHeader* h = HF_PHEAD(page);
    h->nextFreePage = HF_NO_PAGE;
    h->slotCount    = 0;
    h->freeStart    = (short)sizeof(HF_PageHeader);
    h->freeEnd      = (short)PF_PAGE_SIZE;
}

/* Initialize meta page */
static void HF_InitMetaPage(char* page, int firstDataPage)
{
    HF_Meta* m = HF_META(page);
    m->firstFreePage = firstDataPage;
    m->firstDataPage = firstDataPage;
    memset(m->reserved, 0, sizeof(m->reserved));
}

/* Allocate first data page if needed, return page number */
static int HF_EnsureFirstDataPage(int fd, int* outPageNum)
{
    int err; char* metaPg; 
    if ((err = HF_LoadMeta(fd, &metaPg)) != PFE_OK) return err;
    HF_Meta* m = HF_META(metaPg);

    if (m->firstDataPage != HF_NO_PAGE) {
        *outPageNum = m->firstDataPage;
        return HF_UnfixMeta(fd, 0);
    }

    /* allocate */
    int p; char* pg;
    if ((err = PF_AllocPage(fd, &p, &pg)) != PFE_OK) { 
        (void)HF_UnfixMeta(fd,0); 
        return err; 
    }
    HF_InitDataPage(pg);
    if ((err = PF_UnfixPage(fd, p, 1)) != PFE_OK) { 
        (void)HF_UnfixMeta(fd,0); 
        return err; 
    }

    m->firstDataPage = p;
    m->firstFreePage = p;

    *outPageNum = p;
    return HF_UnfixMeta(fd, 1);
}

/* Find (or allocate) a page with enough free space for len bytes + 1 slot */
static int HF_FindTargetPage(int fd, int len, int* outPage)
{
    int err; 
    char* metaPg;

    // 1. Load meta page briefly just to read head of free list
    if ((err = HF_LoadMeta(fd, &metaPg)) != PFE_OK)
        return err;

    HF_Meta* m = HF_META(metaPg);
    int head = m->firstFreePage;

    // Unfix meta page immediately
    if ((err = HF_UnfixMeta(fd, 0)) != PFE_OK)
        return err;

    // 2. Walk the free list without holding meta page
    int p = head;
    while (p != HF_NO_PAGE) {
        char* pg;
        if ((err = PF_GetThisPage(fd, p, &pg)) != PFE_OK)
            return err;

        if (HF_EffectiveFreeForInsert(pg) >= len) {
            (void)PF_UnfixPage(fd, p, 0);
            *outPage = p;
            return PFE_OK;
        }

        HF_PageHeader* h = HF_PHEAD(pg);
        int next = h->nextFreePage;
        (void)PF_UnfixPage(fd, p, 0);
        p = next;
    }

    // 3. If no page found, allocate a new one
    int newp; 
    char* newpg;
    if ((err = PF_AllocPage(fd, &newp, &newpg)) != PFE_OK)
        return err;

    HF_InitDataPage(newpg);

    // Set nextFreePage to old head
    HF_PageHeader* nh = HF_PHEAD(newpg);
    nh->nextFreePage = head;

    if ((err = PF_UnfixPage(fd, newp, 1)) != PFE_OK)
        return err;

    // 4. Re-fix meta to update head pointer
    if ((err = HF_LoadMeta(fd, &metaPg)) != PFE_OK)
        return err;
    m = HF_META(metaPg);
    m->firstFreePage = newp;
    if ((err = HF_UnfixMeta(fd, 1)) != PFE_OK)
        return err;

    *outPage = newp;
    return PFE_OK;
}

int HF_CreateFile(const char* fname) {
    int err;
    if ((err = PF_CreateFile(fname)) != PFE_OK) return err;

    int fd = PF_OpenFile(fname);
    if (fd < 0) return fd;

    /* allocate meta page (page 0) */
    int p0; char* metaPg;
    if ((err = PF_AllocPage(fd, &p0, &metaPg)) != PFE_OK) {
        PF_CloseFile(fd);
        return err;
    }

    // initialize meta page
    HF_InitMetaPage(metaPg, HF_NO_PAGE);
    if ((err = PF_UnfixPage(fd, p0, 1)) != PFE_OK) {
        PF_CloseFile(fd);
        return err;
    }

    // allocate and initialize first data page
    int p1; char* dataPg;
    if ((err = PF_AllocPage(fd, &p1, &dataPg)) != PFE_OK) {
        PF_CloseFile(fd);
        return err;
    }
    HF_InitDataPage(dataPg);
    if ((err = PF_UnfixPage(fd, p1, 1)) != PFE_OK) {
        PF_CloseFile(fd);
        return err;
    }

    // update meta to reference page 1
    char* metaPg2;
    if ((err = HF_LoadMeta(fd, &metaPg2)) != PFE_OK) {
        PF_CloseFile(fd);
        return err;
    }
    HF_META(metaPg2)->firstDataPage = p1;
    HF_META(metaPg2)->firstFreePage = p1;
    if ((err = HF_UnfixMeta(fd, 1)) != PFE_OK) {
        PF_CloseFile(fd);
        return err;
    }

    PF_CloseFile(fd);
    return PFE_OK;
}


int HF_DestroyFile(const char* fname)
{
    return PF_DestroyFile(fname);
}

int HF_OpenFile(const char* fname)
{
    return PF_OpenFile(fname);
}

int HF_CloseFile(int fd)
{
    return PF_CloseFile(fd);
}

/* Insert record into a page chosen via free-list; returns RID */
int HF_InsertRecord(int fd, const void* rec, int len, HF_RID* outRid)
{
    int err, p;
    
    // Ensure we have a data page
    if ((err = HF_EnsureFirstDataPage(fd, &p)) != PFE_OK)
        return err;
    
    if ((err = HF_FindTargetPage(fd, len, &p)) != PFE_OK) 
        return err;

    char* page;
    if ((err = PF_GetThisPage(fd, p, &page)) != PFE_OK) 
        return err;

    HF_PageHeader* h = HF_PHEAD(page);

    /* compute where to place record and slot */
    int eff = HF_EffectiveFreeForInsert(page);
    if (eff < len) {
        (void)PF_UnfixPage(fd, p, 0);
        return PFE_NOBUF;
    }

    /* Allocate slot index */
    int newSlotIndex = h->slotCount;
    
    /* Place record at freeStart (growing upward) */
    int recOffset = (int)h->freeStart;
    
    /* Safety check */
    int slotDirStart = PF_PAGE_SIZE - (newSlotIndex + 1) * sizeof(HF_Slot);
    if (recOffset + len > slotDirStart) {
        (void)PF_UnfixPage(fd, p, 0);
        return PFE_NOBUF;
    }

    /* Copy record bytes */
    memcpy(page + recOffset, rec, (size_t)len);

    /* Write the slot */
    HF_Slot* slot = HF_SLOT(page, newSlotIndex);
    slot->offset  = (short)recOffset;
    slot->length  = (short)len;

    /* Update header */
    h->slotCount += 1;
    h->freeStart = (short)(recOffset + len);

    /* Capture RID BEFORE unfixing */
    if (outRid) {
        outRid->page = p;
        outRid->slot = (short)newSlotIndex;
    }

    if ((err = PF_UnfixPage(fd, p, 1)) != PFE_OK) 
        return err;

    /* Update free list membership */
    if ((err = HF_FreeListUpdate(fd, p)) != PFE_OK) 
        return err;

    return PFE_OK;
}


/* Fetch record by RID; caller supplies buffer and capacity in *inoutLen */
int HF_GetRecord(int fd, HF_RID rid, void* outBuf, int* inoutLen)
{
    int err; char* page;
    if ((err = PF_GetThisPage(fd, rid.page, &page)) != PFE_OK) 
        return err;

    HF_PageHeader* h = HF_PHEAD(page);
    if (rid.slot < 0 || rid.slot >= h->slotCount) { 
        (void)PF_UnfixPage(fd, rid.page, 0); 
        return PFE_INVALIDPAGE; 
    }

    HF_Slot* s = HF_SLOT(page, rid.slot);
    if (s->length == HF_DELETED_LEN) { 
        (void)PF_UnfixPage(fd, rid.page, 0); 
        return PFE_PAGEFREE; 
    }

    if (*inoutLen < s->length) { 
        (void)PF_UnfixPage(fd, rid.page, 0); 
        return PFE_NOBUF; 
    }

    memcpy(outBuf, page + s->offset, (size_t)s->length);
    *inoutLen = s->length;

    return PF_UnfixPage(fd, rid.page, 0);
}

/* Delete => mark slot tombstone; we do not compact immediately */
int HF_DeleteRecord(int fd, HF_RID rid)
{
    int err; char* page;
    if ((err = PF_GetThisPage(fd, rid.page, &page)) != PFE_OK) 
        return err;

    HF_PageHeader* h = HF_PHEAD(page);
    if (rid.slot < 0 || rid.slot >= h->slotCount) { 
        (void)PF_UnfixPage(fd, rid.page, 0); 
        return PFE_INVALIDPAGE; 
    }

    HF_Slot* s = HF_SLOT(page, rid.slot);
    if (s->length == HF_DELETED_LEN) { 
        (void)PF_UnfixPage(fd, rid.page, 0); 
        return PFE_OK; 
    }

    s->length = HF_DELETED_LEN;

    if ((err = PF_UnfixPage(fd, rid.page, 1)) != PFE_OK) 
        return err;

    return HF_FreeListUpdate(fd, rid.page);
}

/* Update in-place if fits; else delete+insert (record may move; RID changes) */
int HF_UpdateRecord(int fd, HF_RID* rid, const void* rec, int newLen)
{
    int err; char* page;
    if ((err = PF_GetThisPage(fd, rid->page, &page)) != PFE_OK) 
        return err;

    HF_PageHeader* h = HF_PHEAD(page);
    if (rid->slot < 0 || rid->slot >= h->slotCount) { 
        (void)PF_UnfixPage(fd, rid->page, 0); 
        return PFE_INVALIDPAGE; 
    }

    HF_Slot* s = HF_SLOT(page, rid->slot);
    if (s->length == HF_DELETED_LEN) { 
        (void)PF_UnfixPage(fd, rid->page, 0); 
        return PFE_PAGEFREE; 
    }

    if (newLen <= s->length) {
        /* in-place overwrite OK */
        memcpy(page + s->offset, rec, (size_t)newLen);
        s->length = (short)newLen;
        if ((err = PF_UnfixPage(fd, rid->page, 1)) != PFE_OK) 
            return err;
        return HF_FreeListUpdate(fd, rid->page);
    }

    /* cannot grow in place => relocate */
    s->length = HF_DELETED_LEN;
    if ((err = PF_UnfixPage(fd, rid->page, 1)) != PFE_OK) 
        return err;
    if ((err = HF_FreeListUpdate(fd, rid->page)) != PFE_OK) 
        return err;

    HF_RID newRid;
    if ((err = HF_InsertRecord(fd, rec, newLen, &newRid)) != PFE_OK) 
        return err;

    *rid = newRid;
    return PFE_OK;
}

/* ---------------- Scan --------------- */

static int HF_FindFirstDataPage(int fd, int* outPage)
{
    int err; char* metaPg;
    if ((err = HF_LoadMeta(fd, &metaPg)) != PFE_OK) 
        return err;
    HF_Meta* m = HF_META(metaPg);
    *outPage = m->firstDataPage;
    return HF_UnfixMeta(fd, 0);
}

int HF_ScanOpen(int fd, HF_Scan* scan)
{
    memset(scan, 0, sizeof(*scan));
    scan->fd = fd;
    scan->endOfScan = 0;
    int first;
    int err = HF_FindFirstDataPage(fd, &first);
    if (err != PFE_OK) return err;
    scan->curPage = first;
    scan->curSlot = -1;
    return PFE_OK;
}

/* Sequentially returns all alive records with their RID */
int HF_ScanNext(HF_Scan* scan, HF_RID* rid, void* outBuf, int* inoutLen)
{
    if (scan->endOfScan || scan->curPage == HF_NO_PAGE) 
        return PFE_EOF;

    int err; char* page;

    while (1) {
        if ((err = PF_GetThisPage(scan->fd, scan->curPage, &page)) != PFE_OK) 
            return err;
        HF_PageHeader* h = HF_PHEAD(page);

        for (int i = scan->curSlot + 1; i < h->slotCount; ++i) {
            HF_Slot* s = HF_SLOT(page, i);
            if (s->length != HF_DELETED_LEN) {
                if (*inoutLen < s->length) { 
                    (void)PF_UnfixPage(scan->fd, scan->curPage, 0); 
                    return PFE_NOBUF; 
                }
                memcpy(outBuf, page + s->offset, (size_t)s->length);
                *inoutLen = s->length;
                scan->curSlot = i;

                rid->page = scan->curPage;
                rid->slot = (short)i;

                return PF_UnfixPage(scan->fd, scan->curPage, 0);
            }
        }

        int p = scan->curPage;
        (void)PF_UnfixPage(scan->fd, scan->curPage, 0);

        int next = p;
        char* nb;
        err = PF_GetNextPage(scan->fd, &next, &nb);
        if (err == PFE_EOF) {
            scan->endOfScan = 1;
            return PFE_EOF;
        } else if (err != PFE_OK) {
            return err;
        } else {
            scan->curPage = next;
            scan->curSlot = -1;
            (void)PF_UnfixPage(scan->fd, next, 0);
        }
    }
}

int HF_ScanClose(HF_Scan* scan)
{
    scan->endOfScan = 1;
    return PFE_OK;
}