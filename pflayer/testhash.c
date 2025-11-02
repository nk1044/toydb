#include <stdio.h>
#include <stdlib.h>
#include "pftypes.h"
#include "pf.h"

int main() {
    int i;
    long j;
    PFbpage *k;

    PFhashInit();

    /* insert a few entries */
    for (i = 1; i < 11; i++)
        for (j = 1; j < 11; j++) {
            PFbpage *dummy = (PFbpage *)(long)(i + j); // cast integer to pointer safely
            if (PFhashInsert(i, j, dummy) != PFE_OK) {
                printf("PFhashInsert failed\n");
                exit(1);
            }
        }

    /* Now, find all the entries */
    for (i = 1; i < 11; i++)
        for (j = 1; j < 11; j++) {
            k = PFhashFind(i, j);
            if (k == NULL) {
                printf("PFfind failed at %d %ld\n", i, j);
                exit(1);
            } else {
                printf("found\n");
            }
        }

    /* Now, delete them in reverse */
    for (j = 10; j > 0; j--)
        for (i = 10; i > 0; i--)
            if (PFhashDelete(i, j) != PFE_OK) {
                printf("PFhashDelete failed at %d %ld\n", i, j);
                exit(1);
            }

    /* print the hash table out */
    PFhashPrint();

    return 0;
}
