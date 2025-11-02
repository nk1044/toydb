/*
 * hash.c
 * Functions to facilitate finding the buffer page given
 * a file descriptor and a page number
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include "pf.h"
 #include "pftypes.h"
 
 /* Hash table */
 static PFhash_entry *PFhashtbl[PF_HASH_TBL_SIZE];
 
 /****************************************************************************
  * Initialize the hash table entries.
  * Must be called before any of the other hash functions are used.
  ****************************************************************************/
 void PFhashInit(void)
 {
     for (int i = 0; i < PF_HASH_TBL_SIZE; i++) {
         PFhashtbl[i] = NULL;
     }
 }
 
 /****************************************************************************
  * Given the file descriptor "fd" and page number "page",
  * find the buffer address of this particular page.
  *
  * Returns:
  *   NULL if not found,
  *   Pointer to buffer page if found.
  ****************************************************************************/
 PFbpage *PFhashFind(int fd, int page)
 {
     int bucket = PFhash(fd, page);
     for (PFhash_entry *entry = PFhashtbl[bucket]; entry != NULL; entry = entry->nextentry) {
         if (entry->fd == fd && entry->page == page) {
             return entry->bpage; /* Found it */
         }
     }
     return NULL; /* Not found */
 }
 
 /*****************************************************************************
  * Insert the (fd, page, bpage) mapping into the hash table.
  *
  * Returns:
  *   PFE_OK              if successful
  *   PFE_NOMEM           if memory allocation fails
  *   PFE_HASHPAGEEXIST   if page already exists
  ****************************************************************************/
 int PFhashInsert(int fd, int page, PFbpage *bpage)
 {
     if (PFhashFind(fd, page) != NULL) {
         PFerrno = PFE_HASHPAGEEXIST;
         return PFerrno;
     }
 
     int bucket = PFhash(fd, page);
     PFhash_entry *entry = malloc(sizeof(PFhash_entry));
 
     if (entry == NULL) {
         PFerrno = PFE_NOMEM;
         return PFerrno;
     }
 
     entry->fd = fd;
     entry->page = page;
     entry->bpage = bpage;
     entry->nextentry = PFhashtbl[bucket];
     entry->preventry = NULL;
 
     if (PFhashtbl[bucket] != NULL) {
         PFhashtbl[bucket]->preventry = entry;
     }
 
     PFhashtbl[bucket] = entry;
     return PFE_OK;
 }
 
 /****************************************************************************
  * Delete the entry with file descriptor "fd" and page number "page"
  * from the hash table.
  *
  * Returns:
  *   PFE_OK             if successful
  *   PFE_HASHNOTFOUND   if entry is not found
  ****************************************************************************/
 int PFhashDelete(int fd, int page)
 {
     int bucket = PFhash(fd, page);
     PFhash_entry *entry = PFhashtbl[bucket];
 
     while (entry != NULL) {
         if (entry->fd == fd && entry->page == page) {
             /* Found the entry to delete */
             if (entry == PFhashtbl[bucket]) {
                 PFhashtbl[bucket] = entry->nextentry;
             }
             if (entry->preventry != NULL) {
                 entry->preventry->nextentry = entry->nextentry;
             }
             if (entry->nextentry != NULL) {
                 entry->nextentry->preventry = entry->preventry;
             }
 
             free(entry);
             return PFE_OK;
         }
         entry = entry->nextentry;
     }
 
     PFerrno = PFE_HASHNOTFOUND;
     return PFerrno;
 }
 
 /****************************************************************************
  * Print the hash table entries (for debugging).
  ****************************************************************************/
 void PFhashPrint(void)
 {
     for (int i = 0; i < PF_HASH_TBL_SIZE; i++) {
         printf("bucket %d\n", i);
         if (PFhashtbl[i] == NULL) {
             printf("\tempty\n");
         } else {
             for (PFhash_entry *entry = PFhashtbl[i]; entry != NULL; entry = entry->nextentry) {
                 printf("\tfd: %d, page: %d, bpage: %p\n",
                        entry->fd, entry->page, (void *)entry->bpage);
             }
         }
     }
 }
 