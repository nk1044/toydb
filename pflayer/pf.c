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
#define L_SET 0
#endif

int PFerrno = PFE_OK; /* last error message */

/* table of opened files */
static PFftab_ele PFftab[PF_FTAB_SIZE];

/* true if file descriptor fd is invalid */
#define PFinvalidFd(fd) ((fd) < 0 || (fd) >= PF_FTAB_SIZE || PFftab[fd].fname == NULL)

/* true if page number "pagenum" of file "fd" is invalid */
#define PFinvalidPagenum(fd,pagenum) ((pagenum) < 0 || (pagenum) >= PFftab[fd].hdr.numpages)

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
    int error;

    if ((error = lseek(PFftab[fd].unixfd, pagenum * sizeof(PFfpage) + PF_HDR_SIZE, L_SET)) == -1) {
        PFerrno = PFE_UNIX;
        return PFerrno;
    }

    if ((error = read(PFftab[fd].unixfd, (char *)buf, sizeof(PFfpage))) != sizeof(PFfpage)) {
        PFerrno = (error < 0) ? PFE_UNIX : PFE_INCOMPLETEREAD;
        return PFerrno;
    }

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
    int error;

    if ((error = lseek(PFftab[fd].unixfd, pagenum * sizeof(PFfpage) + PF_HDR_SIZE, L_SET)) == -1) {
        PFerrno = PFE_UNIX;
        return PFerrno;
    }

    if ((error = write(PFftab[fd].unixfd, (char *)buf, sizeof(PFfpage))) != sizeof(PFfpage)) {
        PFerrno = (error < 0) ? PFE_UNIX : PFE_INCOMPLETEWRITE;
        return PFerrno;
    }

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

/****************************************************************************
SPECIFICATIONS:
	Create a paged file called "fname". The file must not exist before.

RETURN VALUE:
	PFE_OK if OK
	PF error code otherwise
*****************************************************************************/
int PF_CreateFile(const char *fname)
{
    int fd; // file descriptor
    PFhdr_str hdr; // file header (in-memory) stores first free page and numpages

    // check if file already exists
    // If open() with O_EXCL fails, the file already exists (with error code PFE_UNIX)
    // else create the file
    if ((fd = open(fname, O_CREAT | O_EXCL | O_WRONLY, 0664)) < 0) {
        PFerrno = PFE_UNIX;
        return PFerrno;
    }

    hdr.firstfree = PF_PAGE_LIST_END; /* no free page yet */
    hdr.numpages = 0;
	int error;
    // write the header to the file
    // if write fails, close the file and delete it
    if ((error=write(fd,(char *)&hdr,sizeof(hdr))) != sizeof(hdr)){
        PFerrno = (error) ? PFE_UNIX : PFE_HDRWRITE;
        close(fd);
        unlink(fname);
        return PFerrno;
    }

    if (close(fd) == -1) {
        PFerrno = PFE_UNIX;
        return PFerrno;
    }

    return PFE_OK;
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
	Open the paged file named "fname".

RETURN VALUE:
	File descriptor >= 0 if success, PF error code otherwise
*****************************************************************************/
int PF_OpenFile(const char *fname)
{
    int fd;

    if ((fd = PFftabFindFree()) < 0) { // no free entry in PFftab then error
        PFerrno = PFE_FTABFULL;
        return PFerrno;
    }

    if ((PFftab[fd].unixfd = open(fname, O_RDWR)) < 0) { // open file fails then error
        PFerrno = PFE_UNIX;
        return PFerrno;
    }

    // read the file header
    int count;
    if ((count = read(PFftab[fd].unixfd, (char *)&PFftab[fd].hdr, PF_HDR_SIZE)) != PF_HDR_SIZE) {
        PFerrno = (count < 0) ? PFE_UNIX : PFE_HDRREAD;
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

//my functions

// helper funcs
int PFreadhdr(int fd, PFhdr_str *hdr){
    int error;
    if ((error = lseek(PFftab[fd].unixfd, 0, L_SET)) == -1) {
        PFerrno = PFE_UNIX;
        return PFerrno;
    }

    if ((error = read(PFftab[fd].unixfd, (char *)hdr, PF_HDR_SIZE)) != PF_HDR_SIZE) {
        PFerrno = (error < 0) ? PFE_UNIX : PFE_INCOMPLETEREAD;
        return PFerrno;
    }

    return PFE_OK;
}
int PFwritehdr(int fd, PFhdr_str *hdr){
    int error;
    if ((error = lseek(PFftab[fd].unixfd, 0, L_SET)) == -1) {
        PFerrno = PFE_UNIX;
        return PFerrno;
    }

    if ((error = write(PFftab[fd].unixfd, (char *)hdr, PF_HDR_SIZE)) != PF_HDR_SIZE) {
        PFerrno = (error < 0) ? PFE_UNIX : PFE_INCOMPLETEREAD;
        return PFerrno;
    }
    return PFE_OK;
}
//end

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
        "Incomplete write of header from file",
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

