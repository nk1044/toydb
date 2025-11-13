/* pf.c: Paged File Interface Routines + support routines */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include "pf.h"
#include "pftypes.h"

/* To keep system V and PC users happy */
#ifndef L_SET
#define L_SET SEEK_SET
#endif

int PFerrno = PFE_OK; /* last error message */

/* table of opened files */
static PFftab_ele PFftab[PF_FTAB_SIZE];

/* true if file descriptor fd is invalid */
#define PFinvalidFd(fd) ((fd) < 0 || (fd) >= PF_FTAB_SIZE || PFftab[fd].fname == NULL)

/* true if page number "pagenum" of file "fd" is invalid */
#define PFinvalidPagenum(fd,pagenum) ((pagenum) < 0 || (pagenum) >= PFftab[fd].hdr.numpages)

/***************************for stat********************** */
unsigned long PF_physical_reads = 0;
unsigned long PF_physical_writes = 0;
unsigned long PF_page_alloc = 0;
unsigned long PF_page_evicted = 0;
unsigned long PF_logical_reads = 0;   
unsigned long PF_logical_writes = 0;
/********************************************************* */

/****************** Internal Support Functions *****************************/

/****************************************************************************
SPECIFICATIONS:
	Allocate memory and save string pointed by str.
	Return a pointer to the saved string, or NULL if no memory.
*****************************************************************************/
static char *savestr(const char *str)
{
    char *s = malloc(strlen(str) + 1);
    if (s != NULL)
        strcpy(s, str);
    return s;
}

/****************************************************************************
SPECIFICATIONS:
	Find the index in PFftab[] whose "fname" field matches "fname".

RETURN VALUE:
	Index if found, -1 if not found
*****************************************************************************/
static int PFtabFindFname(const char *fname)
{
    for (int i = 0; i < PF_FTAB_SIZE; i++) {
        if (PFftab[i].fname != NULL && strcmp(PFftab[i].fname, fname) == 0)
            return i;
    }
    return -1;
}

/****************************************************************************
SPECIFICATIONS:
	Find a free entry in the open file table PFftab and return its index.

RETURN VALUE:
	>=0 : index of free entry
	-1  : no free entry
*****************************************************************************/
static int PFftabFindFree(void)
{
    for (int i = 0; i < PF_FTAB_SIZE; i++) {
        if (PFftab[i].fname == NULL)
            return i;
    }
    return -1;
}

/****************************************************************************
SPECIFICATIONS:
	Read the page numbered "pagenum" from the file indexed by "fd"
	into the page buffer "buf".

RETURN VALUE:
	PFE_OK if ok
	PF error code otherwise
*****************************************************************************/
int PFreadfcn(int fd, int pagenum, PFfpage *buf)
{
    ssize_t nread;
    off_t offset;

    /* Seek to the page's byte offset: header + pagenum * PF_PAGE_SIZE */
    offset = (off_t)PF_HDR_SIZE + (off_t)pagenum * (off_t)PF_PAGE_SIZE;
    if (lseek(PFftab[fd].unixfd, offset, L_SET) == -1) {
        PFerrno = PFE_UNIX;
        perror("lseek (read)");
        return PFerrno;
    }

    nread = read(PFftab[fd].unixfd, (char *)buf, PF_PAGE_SIZE);
    if (nread != (ssize_t)PF_PAGE_SIZE) {
        PFerrno = (nread < 0) ? PFE_UNIX : PFE_INCOMPLETEREAD;
        if (nread >= 0)
            fprintf(stderr, "PFreadfcn: Incomplete read of page %d (got %zd bytes, expected %d)\n",
                    pagenum, nread, PF_PAGE_SIZE);
        else
            perror("read");
        return PFerrno;
    }

    PF_physical_reads++;
    return PFE_OK;
}

/****************************************************************************
SPECIFICATIONS:
	Write the page numbered "pagenum" from buffer "buf" into the file indexed by "fd".

RETURN VALUE:
	PFE_OK if ok
	PF error code otherwise
*****************************************************************************/
int PFwritefcn(int fd, int pagenum, PFfpage *buf)
{
    ssize_t nwritten;
    off_t offset;

    /* Seek to the page's byte offset: header + pagenum * PF_PAGE_SIZE */
    offset = (off_t)PF_HDR_SIZE + (off_t)pagenum * (off_t)PF_PAGE_SIZE;
    if (lseek(PFftab[fd].unixfd, offset, L_SET) == -1) {
        PFerrno = PFE_UNIX;
        perror("lseek (write)");
        return PFerrno;
    }

    nwritten = write(PFftab[fd].unixfd, (char *)buf, PF_PAGE_SIZE);
    if (nwritten != (ssize_t)PF_PAGE_SIZE) {
        PFerrno = (nwritten < 0) ? PFE_UNIX : PFE_INCOMPLETEWRITE;
        if (nwritten >= 0)
            fprintf(stderr, "PFwritefcn: Incomplete write of page %d (wrote %zd bytes, expected %d)\n",
                    pagenum, nwritten, PF_PAGE_SIZE);
        else
            perror("write");
        return PFerrno;
    }

    /* Ensure data is flushed to disk on the real UNIX fd */
    if (fsync(PFftab[fd].unixfd) == -1) {
        /* fsync failure is a Unix error */
        PFerrno = PFE_UNIX;
        perror("fsync");
        return PFerrno;
    }

    PF_physical_writes++;
    return PFE_OK;
}

/************************* Interface Routines ****************************/

/****************************************************************************
SPECIFICATIONS:
	Initialize the PF interface. Must be the first function called.

GLOBAL VARIABLES MODIFIED:
	PFftab
*****************************************************************************/
void PF_Init(void)
{
    PFhashInit(); /* init the hash table */

    for (int i = 0; i < PF_FTAB_SIZE; i++) {
        PFftab[i].fname = NULL;
    }
}
/* Create the paged file */
int PF_CreateFile(const char *fname)
{
    int fd; // unix file descriptor
    PFhdr_str hdr; // file header (in-memory) stores first free page and numpages
    ssize_t written;

    /* check if file already exists and create it atomically */
    /* use O_RDWR so file is created read/write (avoid platform quirks) */
    fd = open(fname, O_CREAT | O_EXCL | O_RDWR, 0664);
    if (fd < 0) {
        PFerrno = PFE_UNIX;
        /* give a helpful perror so the caller can see the OS errno message */
        perror("PF_CreateFile: open");
        return PFerrno;
    }

    hdr.firstfree = PF_PAGE_LIST_END; /* no free page yet */
    hdr.numpages = 0;

    /* write the header to the file using PF_HDR_SIZE so reading uses same size */
    written = write(fd, (char *)&hdr, PF_HDR_SIZE);
    if (written != (ssize_t)PF_HDR_SIZE) {
        PFerrno = (written < 0) ? PFE_UNIX : PFE_HDRWRITE;
        perror("PF_CreateFile: write header");
        close(fd);
        /* remove incomplete file */
        unlink(fname);
        return PFerrno;
    }

    /* make sure it's on disk */
    if (fsync(fd) == -1) {
        PFerrno = PFE_UNIX;
        perror("PF_CreateFile: fsync");
        close(fd);
        unlink(fname);
        return PFerrno;
    }

    if (close(fd) == -1) {
        PFerrno = PFE_UNIX;
        perror("PF_CreateFile: close");
        unlink(fname);
        return PFerrno;
    }

    return PFE_OK;
}

/* Open the paged file named "fname". Returns PF fd (index into PFftab) or PF error */
int PF_OpenFile(const char *fname)
{
    int fd;

    /* if file already open, return error */
    if (PFtabFindFname(fname) != -1) {
        PFerrno = PFE_FILEOPEN;
        return PFerrno;
    }

    if ((fd = PFftabFindFree()) < 0) { // no free entry in PFftab then error
        PFerrno = PFE_FTABFULL;
        return PFerrno;
    }

    if ((PFftab[fd].unixfd = open(fname, O_RDWR)) < 0) { // open file fails then error
        PFerrno = PFE_UNIX;
        perror("PF_OpenFile: open");
        return PFerrno;
    }

    /* read the file header */
    ssize_t count;
    if ((count = read(PFftab[fd].unixfd, (char *)&PFftab[fd].hdr, PF_HDR_SIZE)) != (ssize_t)PF_HDR_SIZE) {
        PFerrno = (count < 0) ? PFE_UNIX : PFE_HDRREAD;
        if (count < 0) perror("PF_OpenFile: read header");
        close(PFftab[fd].unixfd);
        return PFerrno;
    }

    PFftab[fd].hdrchanged = 0; /* header not changed */

    if ((PFftab[fd].fname = savestr(fname)) == NULL) {
        close(PFftab[fd].unixfd);
        PFerrno = PFE_NOMEM;
        return PFerrno;
    }

    return fd;
}



/****************************************************************************
SPECIFICATIONS:
	Destroy the paged file named "fname". The file must exist and
	not be open.

RETURN VALUE:
	PFE_OK if success
	PF error codes otherwise
*****************************************************************************/
int PF_DestroyFile(const char *fname)
{
    if (PFtabFindFname(fname) != -1) { // file is open then cannot be destroyed
        PFerrno = PFE_FILEOPEN;
        return PFerrno;
    }

    if (unlink(fname) != 0) {
        PFerrno = PFE_UNIX;
        return PFerrno;
    }

    return PFE_OK;
}

/****************************************************************************
SPECIFICATIONS:
	Close the file indexed by "fd". Pages must be unfixed first.

RETURN VALUE:
	PFE_OK if OK
	PF error code otherwise
*****************************************************************************/
int PF_CloseFile(int fd)
{
    int error;

    if (PFinvalidFd(fd)) {
        PFerrno = PFE_FD;
        return PFerrno;
    }

    if ((error = PFbufReleaseFile(fd, PFwritefcn)) != PFE_OK)
        return error;

    if (PFftab[fd].hdrchanged) {
        if (lseek(PFftab[fd].unixfd, 0, L_SET) == -1) {
            PFerrno = PFE_UNIX;
            return PFerrno;
        }

        if (write(PFftab[fd].unixfd, (char *)&PFftab[fd].hdr, PF_HDR_SIZE) != PF_HDR_SIZE) {
            PFerrno = PFE_HDRWRITE;
            return PFerrno;
        }

        PFftab[fd].hdrchanged = 0;
    }

    if (close(PFftab[fd].unixfd) == -1) {
        PFerrno = PFE_UNIX;
        return PFerrno;
    }

    free(PFftab[fd].fname);
    PFftab[fd].fname = NULL;

    return PFE_OK;
}

/* helper funcs */

/* read header */
int PFreadhdr(int fd, PFhdr_str *hdr){
    ssize_t nread;
    if (lseek(PFftab[fd].unixfd, 0, L_SET) == -1) {
        PFerrno = PFE_UNIX;
        perror("lseek (read hdr)");
        return PFerrno;
    }

    nread = read(PFftab[fd].unixfd, (char *)hdr, PF_HDR_SIZE);
    if (nread != (ssize_t)PF_HDR_SIZE) {
        PFerrno = (nread < 0) ? PFE_UNIX : PFE_INCOMPLETEREAD;
        if (nread >= 0)
            fprintf(stderr, "PFreadhdr: incomplete header read (got %zd bytes, expected %d)\n", nread, PF_HDR_SIZE);
        return PFerrno;
    }

    return PFE_OK;
}

/* write header */
int PFwritehdr(int fd, PFhdr_str *hdr){
    ssize_t nwritten;
    if (lseek(PFftab[fd].unixfd, 0, L_SET) == -1) {
        PFerrno = PFE_UNIX;
        perror("lseek (write hdr)");
        return PFerrno;
    }

    nwritten = write(PFftab[fd].unixfd, (char *)hdr, PF_HDR_SIZE);
    if (nwritten != (ssize_t)PF_HDR_SIZE) {
        PFerrno = (nwritten < 0) ? PFE_UNIX : PFE_HDRWRITE;
        if (nwritten >= 0)
            fprintf(stderr, "PFwritehdr: incomplete header write (wrote %zd bytes, expected %d)\n", nwritten, PF_HDR_SIZE);
        return PFerrno;
    }
    /* ensure header durability */
    if (fsync(PFftab[fd].unixfd) == -1) {
        PFerrno = PFE_UNIX;
        perror("fsync (hdr)");
        return PFerrno;
    }
    return PFE_OK;
}

int PF_AllocPage(int fd, int *pagenum, char **pagebuf){
    PFfpage *fpage;	/* pointer to file page */
int error;

	if (PFinvalidFd(fd)){
		PFerrno= PFE_FD;
		return(PFerrno);
	}

	if (PFftab[fd].hdr.firstfree != PF_PAGE_LIST_END){
		/* get a page from the free list */
		*pagenum = PFftab[fd].hdr.firstfree;
		if ((error=PFbufGet(fd,*pagenum,&fpage,PFreadfcn,
					PFwritefcn))!= PFE_OK)
			/* can't get the page */
			return(error);
		PFftab[fd].hdr.firstfree = fpage->nextfree;
		PFftab[fd].hdrchanged = TRUE;
	}
	else {
		/* Free list empty, allocate one more page from the file */
		*pagenum = PFftab[fd].hdr.numpages;
		if ((error=PFbufAlloc(fd,*pagenum,&fpage,PFwritefcn))!= PFE_OK)
			/* can't allocate a page */
			return(error);
	
		/* increment # of pages for this file */
		PFftab[fd].hdr.numpages++;
		PFftab[fd].hdrchanged = TRUE;

		/* mark this page dirty */
		if ((error=PFbufUsed(fd,*pagenum))!= PFE_OK){
			printf("internal error: PFalloc()\n");
			exit(1);
		}

	}

	/* zero out the page. Seems to be a nice thing to do,
	at least for debugging. */
	/*
	bzero(fpage->pagebuf,PF_PAGE_SIZE);
	*/

	/* Mark the new page used */
	fpage->nextfree = PF_PAGE_USED;

	/* set return value */
	*pagebuf = fpage->pagebuf;
	
	return(PFE_OK);
}


int PF_GetNextPage(int fd, int *pagenum, char **pagebuf){
    int temppage;	/* page number to scan for next valid page */
int error;	/* error code */
PFfpage *fpage;	/* pointer to file page */

	if (PFinvalidFd(fd)){
		PFerrno = PFE_FD;
		return(PFerrno);
	}


	if (*pagenum < -1 || *pagenum >= PFftab[fd].hdr.numpages){
		PFerrno = PFE_INVALIDPAGE;
		return(PFerrno);
	}

	/* scan the file until a valid used page is found */
	for (temppage= *pagenum+1;temppage<PFftab[fd].hdr.numpages;temppage++){
		if ( (error=PFbufGet(fd,temppage,&fpage,PFreadfcn,
					PFwritefcn))!= PFE_OK)
			return(error);
		else if (fpage->nextfree == PF_PAGE_USED){
			/* found a used page */
			*pagenum = temppage;
			*pagebuf = (char *)fpage->pagebuf;
			return(PFE_OK);
		}

		/* page is free, unfix it */
		if ((error=PFbufUnfix(fd,temppage,FALSE))!= PFE_OK)
			return(error);
	}

	/* No valid used page found */
	PFerrno = PFE_EOF;
	return(PFerrno);
}


int PF_UnfixPage(int fd, int pagenum, int dirty){
    if (PFinvalidFd(fd)){
		PFerrno = PFE_FD;
		return(PFerrno);
	}

	if (PFinvalidPagenum(fd,pagenum)){
		PFerrno = PFE_INVALIDPAGE;
		return(PFerrno);
	}

	return(PFbufUnfix(fd,pagenum,dirty));
}

int PF_DisposePage(int fd, int pagenum) {
    PFfpage *fpage;	/* pointer to file page */
int error;

	if (PFinvalidFd(fd)){
		PFerrno = PFE_FD;
		return(PFerrno);
	}

	if (PFinvalidPagenum(fd,pagenum)){
		PFerrno = PFE_INVALIDPAGE;
		return(PFerrno);
	}

	if ((error=PFbufGet(fd,pagenum,&fpage,PFreadfcn,PFwritefcn))!= PFE_OK)
		/* can't get this page */
		return(error);
	
	if (fpage->nextfree != PF_PAGE_USED){
		/* this page already freed */
		if (PFbufUnfix(fd,pagenum,FALSE)!= PFE_OK){
			printf("internal error: PFdispose()\n");
			exit(1);
		}
		PFerrno = PFE_PAGEFREE;
		return(PFerrno);
	}

	/* put this page into the free list */
	fpage->nextfree = PFftab[fd].hdr.firstfree;
	PFftab[fd].hdr.firstfree = pagenum;
	PFftab[fd].hdrchanged = TRUE;

	/* unfix this page */
	return(PFbufUnfix(fd,pagenum,TRUE));
}

int PF_GetThisPage(int fd, int pagenum, char **pagebuf)
{
    int error;
    PFfpage *fpage;
    
        if (PFinvalidFd(fd)){
            PFerrno = PFE_FD;
            return(PFerrno);
        }
    
        if (PFinvalidPagenum(fd,pagenum)){
            PFerrno = PFE_INVALIDPAGE;
            return(PFerrno);
        }
    
        if ( (error=PFbufGet(fd,pagenum,&fpage,PFreadfcn,PFwritefcn))!= PFE_OK){
            if (error== PFE_PAGEFIXED)
                *pagebuf = fpage->pagebuf;
            return(error);
        }
    
        if (fpage->nextfree == PF_PAGE_USED){
            /* page is used*/
            *pagebuf = (char *)fpage->pagebuf;
            return(PFE_OK);
        }
        else {
            /* invalid page */
            if (PFbufUnfix(fd,pagenum,FALSE)!= PFE_OK){
                printf("internal error:PFgetThis()\n");
                exit(1);
            }
            PFerrno = PFE_INVALIDPAGE;
            return(PFerrno);
        }
}

int PF_GetFirstPage(int fd, int *pagenum, char **pagebuf)
{
    *pagenum = -1;
	return(PF_GetNextPage(fd,pagenum,pagebuf));
}


/****************************************************************************
SPECIFICATIONS:
	Print last PF error with a given string

RETURN VALUE: none
*****************************************************************************/
void PF_PrintError( char *s)
{
    static char *PFerrormsg[] = {
        "No error",
        "No memory",
        "No buffer space",
        "Page already fixed in buffer",
        "Page to be unfixed is not in the buffer",
        "Unix error",
        "Incomplete read of page from file",
        "Incomplete write of page to file",
        "Incomplete read of header from file",
        "Incomplete write of header to file",
        "Invalid page number",
        "File already open",
        "File table full",
        "Invalid file descriptor",
        "End of file",
        "Page already free",
        "Page already unfixed",
        "New page to be allocated already in buffer",
        "Hash table entry not found",
        "Page already in hash table"
    };

    fprintf(stderr, "%s: %s", s, PFerrormsg[-PFerrno]);
    if (PFerrno == PFE_UNIX)
        perror(" ");
    else
        fprintf(stderr, "\n");
}
