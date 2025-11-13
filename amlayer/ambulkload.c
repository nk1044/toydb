#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "am.h"
#include "pf.h"
#include "pftypes.h"   // for PF_PAGE_SIZE etc.

/**
 * Struct to hold key and record-id pairs temporarily during bulk-load
 */
typedef struct {
    int key;     // roll number
    int recId;   // page number or record identifier
} KeyRecPair;

/**
 * Bulk-load an index on a file assuming it is pre-sorted by the key (rollno).
 * Creates a B+ tree index efficiently in one pass.
 */
void AM_BulkLoad(const char *fileName, int indexNo, const char *dataFile) {
    int fd, pagenum;
    char *pagebuf;
    KeyRecPair *pairs = NULL;
    int capacity = 100;
    int n = 0;

    // Open the data file using PF layer
    fd = PF_OpenFile(dataFile);
    if (fd < 0) {
        PF_PrintError("PF_OpenFile (dataFile) failed");
        return;
    }

    pairs = malloc(capacity * sizeof(KeyRecPair));
    if (!pairs) {
        printf("Memory allocation failed.\n");
        PF_CloseFile(fd);
        return;
    }

    // Iterate through all pages of the data file
    int ret = PF_GetFirstPage(fd, &pagenum, &pagebuf);
    while (ret == PFE_OK) {
        // For simplicity, assume each page starts with an int rollno
        int rollno = *(int *)pagebuf;

        if (n >= capacity) {
            capacity *= 2;
            pairs = realloc(pairs, capacity * sizeof(KeyRecPair));
            if (!pairs) {
                printf("Realloc failed.\n");
                PF_UnfixPage(fd, pagenum, 0);
                PF_CloseFile(fd);
                return;
            }
        }

        pairs[n].key = rollno;
        pairs[n].recId = pagenum; // using page number as record identifier
        n++;

        // Unfix current page and move to next
        PF_UnfixPage(fd, pagenum, 0);
        ret = PF_GetNextPage(fd, &pagenum, &pagebuf);
    }

    PF_CloseFile(fd);
    printf("BulkLoad: Collected %d records from %s.\n", n, dataFile);

    // Create index
    if (AM_CreateIndex((char *)fileName, indexNo, 'i', sizeof(int)) < 0) {
        PF_PrintError("AM_CreateIndex failed");
        free(pairs);
        return;
    }

    // Open index file using PF layer
    char indexFile[80];
    sprintf(indexFile, "%s.%d", fileName, indexNo);
    int indexFd = PF_OpenFile(indexFile);
    if (indexFd < 0) {
        PF_PrintError("PF_OpenFile (indexFile) failed");
        free(pairs);
        return;
    }

    // Insert keys sequentially
    for (int i = 0; i < n; i++) {
        AM_InsertEntry(indexFd, 'i', sizeof(int),
                       (char *)&pairs[i].key, pairs[i].recId);
    }

    PF_CloseFile(indexFd);
    free(pairs);

    printf("BulkLoad: Index successfully built on %s.%d\n", fileName, indexNo);
}
