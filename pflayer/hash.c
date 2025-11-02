/* hash.c: Functions to facilitate finding the buffer page given
   a file descriptor and a page number */

#include <stdio.h>
#include <stdlib.h>
#include "pf.h"
#include "pftypes.h"

/* hash table */
static PFhash_entry *PFhashtbl[PF_HASH_TBL_SIZE];

/****************************************************************************
SPECIFICATIONS:
	Init the hash table entries. Must be called before any of the other
	hash functions are used.

AUTHOR: clc

RETURN VALUE: none

GLOBAL VARIABLES MODIFIED:
	PFhashtbl
*****************************************************************************/
void PFhashInit(void)
{
    for (int i = 0; i < PF_HASH_TBL_SIZE; i++)
        PFhashtbl[i] = NULL;
}

/****************************************************************************
SPECIFICATIONS:
	Given the file descriptor "fd", and page number "page",
	find the buffer address of this particular page.

AUTHOR: clc

RETURN VALUE:
	NULL	if not found.
	Buffer address, if found.

*****************************************************************************/
PFbpage *PFhashFind(int fd, int page)
{
    int bucket = PFhash(fd, page); /* bucket to look for the page */
    PFhash_entry *entry;

    /* go through the linked list of this bucket */
    for (entry = PFhashtbl[bucket]; entry != NULL; entry = entry->nextentry) {
        if (entry->fd == fd && entry->page == page)
            return entry->bpage; /* found it */
    }

    return NULL; /* not found */
}

/*****************************************************************************
SPECIFICATIONS:
	Insert the file descriptor "fd", page number "page", and the
	buffer address "bpage" into the hash table. 

AUTHOR: clc

RETURN VALUE:
	PFE_OK	if OK
	PFE_NOMEM	if no memory
	PFE_HASHPAGEEXIST if the page already exists.
	
GLOBAL VARIABLES MODIFIED:
	PFhashtbl
*****************************************************************************/
int PFhashInsert(int fd, int page, PFbpage *bpage)
{
    if (PFhashFind(fd, page) != NULL) {
        PFerrno = PFE_HASHPAGEEXIST;
        return PFerrno;
    }

    int bucket = PFhash(fd, page);

    PFhash_entry *entry = (PFhash_entry *)malloc(sizeof(PFhash_entry));
    if (entry == NULL) {
        PFerrno = PFE_NOMEM;
        return PFerrno;
    }

    entry->fd = fd;
    entry->page = page;
    entry->bpage = bpage;
    entry->nextentry = PFhashtbl[bucket];
    entry->preventry = NULL;

    if (PFhashtbl[bucket] != NULL)
        PFhashtbl[bucket]->preventry = entry;

    PFhashtbl[bucket] = entry;

    return PFE_OK;
}

/****************************************************************************
SPECIFICATIONS:
	Delete the entry whose file descriptor is "fd", and whose page number
	is "page" from the hash table.

AUTHOR: clc

RETURN VALUE:
	PFE_OK	if OK
	PFE_HASHNOTFOUND if can't find the entry

GLOBAL VARIABLES MODIFIED:
	PFhashtbl
*****************************************************************************/
int PFhashDelete(int fd, int page)
{
    int bucket = PFhash(fd, page);
    PFhash_entry *entry;

    for (entry = PFhashtbl[bucket]; entry != NULL; entry = entry->nextentry) {
        if (entry->fd == fd && entry->page == page)
            break;
    }

    if (entry == NULL) {
        PFerrno = PFE_HASHNOTFOUND;
        return PFerrno;
    }

    if (entry == PFhashtbl[bucket])
        PFhashtbl[bucket] = entry->nextentry;
    if (entry->preventry != NULL)
        entry->preventry->nextentry = entry->nextentry;
    if (entry->nextentry != NULL)
        entry->nextentry->preventry = entry->preventry;

    free(entry);
    return PFE_OK;
}

/****************************************************************************
SPECIFICATIONS:
	Print the hash table entries.

AUTHOR: clc

RETURN VALUE: None
*****************************************************************************/
void PFhashPrint(void)
{
    for (int i = 0; i < PF_HASH_TBL_SIZE; i++) {
        printf("bucket %d\n", i);
        if (PFhashtbl[i] == NULL)
            printf("\tempty\n");
        else {
            for (PFhash_entry *entry = PFhashtbl[i]; entry != NULL; entry = entry->nextentry)
                printf("\tfd: %d, page: %d, bpage: %p\n",
                       entry->fd, entry->page, (void *)entry->bpage);
        }
    }
}
