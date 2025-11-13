/* buf.c: buffer management routines. The interface routines are:
PFbufGet(), PFbufUnfix(), PFbufAlloc(), PFbufReleaseFile(), PFbufUsed() and
PFbufPrint() */

#include <stdio.h>
#include <stdlib.h>
#include "pf.h"
#include "pftypes.h"

static int PFnumbpage = 0;          /* # of buffer pages in memory */
static PFbpage *PFfirstbpage = NULL; /* ptr to first buffer page, or NULL */
static PFbpage *PFlastbpage = NULL;  /* ptr to last buffer page, or NULL */
static PFbpage *PFfreebpage = NULL;  /* list of free buffer pages */

/* Insert the buffer page pointed by "bpage" into the free list. */
static void PFbufInsertFree(PFbpage *bpage) {
    bpage->nextpage = PFfreebpage;
    PFfreebpage = bpage;
}

/* Link the buffer page pointed by "bpage" as the head of the used buffer list. */
static void PFbufLinkHead(PFbpage *bpage) {
    bpage->nextpage = PFfirstbpage;
    bpage->prevpage = NULL;
    if (PFfirstbpage != NULL)
        PFfirstbpage->prevpage = bpage;
    PFfirstbpage = bpage;
    if (PFlastbpage == NULL)
        PFlastbpage = bpage;
}

/* Unlink the page pointed by bpage from the buffer list */
void PFbufUnlink(PFbpage *bpage) {
    if (PFfirstbpage == bpage)
        PFfirstbpage = bpage->nextpage;

    if (PFlastbpage == bpage)
        PFlastbpage = bpage->prevpage;

    if (bpage->nextpage != NULL)
        bpage->nextpage->prevpage = bpage->prevpage;

    if (bpage->prevpage != NULL)
        bpage->prevpage->nextpage = bpage->nextpage;

    bpage->prevpage = bpage->nextpage = NULL;
}

int USE_MRU = 0; 

/* Internal buffer allocation routine */
/* Internal buffer allocation routine */
static int PFbufInternalAlloc(PFbpage **bpage, int (*writefcn)(int, int, PFfpage *)) {
    PFbpage *tbpage = NULL;
    int error;

    /* Case 1: Reuse a free page from the free list */
    if (PFfreebpage != NULL) {
        *bpage = PFfreebpage;
        PFfreebpage = (*bpage)->nextpage;
    }

    /* Case 2: Allocate a new page if buffer not yet full */
    else if (PFnumbpage < PF_MAX_BUFS) {
        *bpage = (PFbpage *)malloc(sizeof(PFbpage));
        if (*bpage == NULL) {
            PFerrno = PFE_NOMEM;
            *bpage = NULL;
            return PFerrno;
        }

        PFnumbpage++;

        /* Initialize fields for new page */
        (*bpage)->nextpage = (*bpage)->prevpage = NULL;
        (*bpage)->fd = -1;
        (*bpage)->page = -1;
        (*bpage)->dirty = FALSE;
        (*bpage)->fixed = FALSE;
    }

    /* Case 3: Need to evict a page using LRU or MRU */
    else {
        PF_page_evicted++;
        if (USE_MRU) {
            /* MRU policy: evict from head */
            for (tbpage = PFfirstbpage; tbpage != NULL; tbpage = tbpage->nextpage) {
                if (!tbpage->fixed)
                    break;
            }
        } else {
            /* LRU policy: evict from tail */
            for (tbpage = PFlastbpage; tbpage != NULL; tbpage = tbpage->prevpage) {
                if (!tbpage->fixed)
                    break;
            }
        }

        /* No available victim (all pages pinned) */
        if (tbpage == NULL) {
            PFerrno = PFE_NOBUF;
            *bpage = NULL;
            return PFerrno;
        }

        /* If the victim page is dirty, write it to disk */
        if (tbpage->dirty) {
            error = (*writefcn)(tbpage->fd, tbpage->page, &tbpage->fpage);
            if (error != PFE_OK) {
                /* Keep consistent state and return */
                return error;
            }
            tbpage->dirty = FALSE;
        }

        /* Remove victim from hash and used list */
        if ((error = PFhashDelete(tbpage->fd, tbpage->page)) != PFE_OK)
            return error;

        PFbufUnlink(tbpage);

        /* Reset all metadata before reuse */
        tbpage->fd = -1;
        tbpage->page = -1;
        tbpage->fixed = FALSE;
        tbpage->dirty = FALSE;
        tbpage->nextpage = tbpage->prevpage = NULL;
        *bpage = tbpage;
    }

    /* Always insert new/reused page at head (most recently used) */
    PFbufLinkHead(*bpage);
    PF_page_alloc++;
    return PFE_OK;
}



/* Get a page from the file and fix it in buffer */
int PFbufGet(int fd, int pagenum, PFfpage **fpage, int (*readfcn)(int, int, PFfpage *), int (*writefcn)(int, int, PFfpage *)) {
    PFbpage *bpage;
    int error;

    if ((bpage = PFhashFind(fd, pagenum)) == NULL) {
        if ((error = PFbufInternalAlloc(&bpage, writefcn)) != PFE_OK) {
            *fpage = NULL;
            return error;
        }

        if ((error = (*readfcn)(fd, pagenum, &bpage->fpage)) != PFE_OK) {
            PFbufUnlink(bpage);
            PFbufInsertFree(bpage);
            *fpage = NULL;
            return error;
        }

        if ((error = PFhashInsert(fd, pagenum, bpage)) != PFE_OK) {
            PFbufUnlink(bpage);
            PFbufInsertFree(bpage);
            return error;
        }

        bpage->fd = fd;
        bpage->page = pagenum;
        bpage->dirty = FALSE;
    } else if (bpage->fixed) {
        *fpage = &bpage->fpage;
        PFerrno = PFE_PAGEFIXED;
        return PFerrno;
    }
    PF_logical_reads++;

    bpage->fixed = TRUE;
    *fpage = &bpage->fpage;
    return PFE_OK;
}

/* Unfix a page in buffer */
int PFbufUnfix(int fd, int pagenum, int dirty) {
    PFbpage *bpage;

    if ((bpage = PFhashFind(fd, pagenum)) == NULL) {
        PFerrno = PFE_PAGENOTINBUF;
        return PFerrno;
    }

    if (!bpage->fixed) {
        PFerrno = PFE_PAGEUNFIXED;
        return PFerrno;
    }

    if (dirty)
        bpage->dirty = TRUE;

    bpage->fixed = FALSE;
    PFbufUnlink(bpage);
    PFbufLinkHead(bpage);

    return PFE_OK;
}

/* Allocate a buffer and associate with a page */
int PFbufAlloc(int fd, int pagenum, PFfpage **fpage, int (*writefcn)(int, int, PFfpage *)) {
    PFbpage *bpage;
    int error;

    *fpage = NULL;

    if ((bpage = PFhashFind(fd, pagenum)) != NULL) {
        PFerrno = PFE_PAGEINBUF;
        return PFerrno;
    }

    if ((error = PFbufInternalAlloc(&bpage, writefcn)) != PFE_OK)
        return error;

    if ((error = PFhashInsert(fd, pagenum, bpage)) != PFE_OK) {
        PFbufUnlink(bpage);
        PFbufInsertFree(bpage);
        return error;
    }
    PF_page_alloc++;
    PF_logical_writes++;

    bpage->fd = fd;
    bpage->page = pagenum;
    bpage->fixed = TRUE;
    bpage->dirty = FALSE;

    *fpage = &bpage->fpage;
    return PFE_OK;
}

/* Release all pages of a file */
int PFbufReleaseFile(int fd, int (*writefcn)(int, int, PFfpage *)) {
    PFbpage *bpage = PFfirstbpage, *temppage;
    int error;

    while (bpage != NULL) {
        if (bpage->fd == fd) {
            if (bpage->fixed) {
                PFerrno = PFE_PAGEFIXED;
                return PFerrno;
            }

            if (bpage->dirty && (error = (*writefcn)(fd, bpage->page, &bpage->fpage)) != PFE_OK)
                return error;
            bpage->dirty = FALSE;

            if ((error = PFhashDelete(fd, bpage->page)) != PFE_OK) {
                printf("Internal error: PFbufReleaseFile()\n");
                exit(1);
            }

            temppage = bpage;
            bpage = bpage->nextpage;
            PFbufUnlink(temppage);
            PFbufInsertFree(temppage);
        } else {
            bpage = bpage->nextpage;
        }
    }
    return PFE_OK;
}

/* Mark page as used (dirty) */
int PFbufUsed(int fd, int pagenum) {
    PFbpage *bpage;

    if ((bpage = PFhashFind(fd, pagenum)) == NULL) {
        PFerrno = PFE_PAGENOTINBUF;
        return PFerrno;
    }

    if (!bpage->fixed) {
        PFerrno = PFE_PAGEUNFIXED;
        return PFerrno;
    }

    bpage->dirty = TRUE;
    PFbufUnlink(bpage);
    PFbufLinkHead(bpage);

    return PFE_OK;
}

/* Print current buffer pages */
void PFbufPrint() {
    PFbpage *bpage;
    printf("buffer content:\n");
    if (PFfirstbpage == NULL) {
        printf("empty\n");
    } else {
        printf("fd\tpage\tfixed\tdirty\tfpage\n");
        for (bpage = PFfirstbpage; bpage != NULL; bpage = bpage->nextpage)
            printf("%d\t%d\t%d\t%d\t%p\n",
                   bpage->fd, bpage->page, (int)bpage->fixed,
                   (int)bpage->dirty, (void *)&bpage->fpage);
    }
}
