#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../hfLayer/hf.h"
#include "../pflayer/pf.h"

#define HF_FILE "hf_testfile.hf"

/* Simple record generator */
static void make_record(char *buf, int i) {
    sprintf(buf, "Record number %d -- some text data...", i);
}

int main() {
    printf("=== HF LAYER BASIC TEST ===\n");

    PF_Init();

    /* Delete old file */
    remove(HF_FILE);

    printf("[1] Creating HF file: %s\n", HF_FILE);
    if (HF_CreateFile(HF_FILE) != HF_OK) {
        PF_PrintError("HF_CreateFile");
        return 1;
    }

    printf("[2] Opening HF file\n");
    int fd = HF_OpenFile(HF_FILE);
    if (fd < 0) {
        PF_PrintError("HF_OpenFile");
        return 1;
    }

    printf("[3] Inserting records...\n");
    int NUM = 3000;   /* enough to force many page allocations */
    char rec[256];
    HF_RID rid;

    for (int i = 0; i < NUM; i++) {
        make_record(rec, i);

        int err = HF_InsertRecord(fd, rec, strlen(rec) + 1, &rid);
        if (err != HF_OK) {
            printf("Insert failed at %d\n", i);
            PF_PrintError("HF_InsertRecord");
            return 1;
        }
    }
    printf("Inserted %d records.\n", NUM);

    printf("[4] Closing file\n");
    HF_CloseFile(fd);

    printf("[5] Re-opening for scan test\n");
    fd = HF_OpenFile(HF_FILE);

    HF_Scan scan;
    HF_ScanOpen(fd, &scan);

    printf("[6] Scanning records...\n");
    int count = 0;
    char buffer[400];
    int recLen;
    HF_RID readRID;

    while (HF_ScanNext(&scan, &readRID, buffer, &recLen) == HF_OK) {
        /* Verify record content */
        char expected[256];
        make_record(expected, count);

        if (strcmp(expected, buffer) != 0) {
            printf("MISMATCH at %d!\n", count);
            printf("Expected: %s\n", expected);
            printf("Got     : %s\n", buffer);
            return 1;
        }

        count++;
    }

    HF_ScanClose(&scan);
    HF_CloseFile(fd);

    printf("[7] Scan completed. Verified %d records.\n", count);

    if (count == NUM)
        printf("\n*** HF LAYER TEST PASSED SUCCESSFULLY ***\n");
    else
        printf("\n*** HF LAYER TEST FAILED: count mismatch (%d vs %d) ***\n", count, NUM);

    return 0;
}
