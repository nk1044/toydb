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

/* Internal buffer allocation routine */
static int PFbufInternalAlloc(PFbpage **bpage, int (*writefcn)(int, int, PFfpage *)) {
    PFbpage *tbpage;
    int error;

    if (PFfreebpage != NULL) {
        *bpage = PFfreebpage;
        PFfreebpage = (*bpage)->nextpage;
    } else if (PFnumbpage < PF_MAX_BUFS) {
        if ((*bpage = (PFbpage *)malloc(sizeof(PFbpage))) == NULL) {
            *bpage = NULL;
            PFerrno = PFE_NOMEM;
            return PFerrno;
        }
        PFnumbpage++;
    } else {
        *bpage = NULL;
        for (tbpage = PFlastbpage; tbpage != NULL; tbpage = tbpage->prevpage) {
            if (!tbpage->fixed)
                break;
        }
        if (tbpage == NULL) {
            PFerrno = PFE_NOBUF;
            return PFerrno;
        }
        if (tbpage->dirty && (error = (*writefcn)(tbpage->fd, tbpage->page, &tbpage->fpage)) != PFE_OK)
            return error;
        tbpage->dirty = FALSE;

        if ((error = PFhashDelete(tbpage->fd, tbpage->page)) != PFE_OK)
            return error;

        PFbufUnlink(tbpage);
        *bpage = tbpage;
    }

    PFbufLinkHead(*bpage);
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
