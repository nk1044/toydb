#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../hfLayer/hf.h"
#include "../pflayer/pf.h"

/*===============================================================
   DATASET PARAMETERS
 ===============================================================*/
#define DATASET "../data/courses.txt"
#define HF_FILE "courses_var.hf"
#define STATIC_FILE "courses_static.bin"

/* List of static record sizes to test */
int STATIC_SIZES[] = {64, 80, 128, 160};
int NUM_STATIC_SIZES = sizeof(STATIC_SIZES)/sizeof(int);

/*===============================================================
   UTILITIES
 ===============================================================*/
static void trim(char *s) {
    size_t n = strlen(s);
    if (n && s[n-1] == '\n') s[n-1] = '\0';
}

/*===============================================================
   METRICS STRUCT
 ===============================================================*/
typedef struct {
    int totalRecords;
    int totalPages;
    int totalBytesStored;
    int maxRecordLen;
} Metrics;

void metrics_init(Metrics *m) {
    m->totalRecords = 0;
    m->totalPages = 0;
    m->totalBytesStored = 0;
    m->maxRecordLen = 0;
}

/*===============================================================
   COUNT PAGES USING PF (open count + get pages)
 ===============================================================*/
int get_total_pf_pages(const char *fname) {
    int fd = PF_OpenFile(fname);
    if (fd < 0) return 0;

    int count = 0;
    char *pg;
    while (PF_GetThisPage(fd, count, &pg) == PFE_OK) {
        PF_UnfixPage(fd, count, 0);
        count++;
    }

    PF_CloseFile(fd);
    return count;
}

/*===============================================================
   LOAD INTO HF (variable-length slotted pages)
 ===============================================================*/
void build_variable(Metrics *m) {
    printf("\n=== BUILD VARIABLE-LENGTH FILE ===\n");

    remove(HF_FILE);
    PF_Init();
    HF_CreateFile(HF_FILE);

    int hffd = HF_OpenFile(HF_FILE);
    if (hffd < 0) {
        PF_PrintError("HF_OpenFile");
        exit(1);
    }

    FILE *f = fopen(DATASET, "r");
    if (!f) {
        perror("open dataset");
        exit(1);
    }

    char line[512];
    HF_RID rid;

    while (fgets(line, sizeof(line), f)) {
        trim(line);
        if (line[0] == '\0') continue;

        int len = strlen(line) + 1;
        int err = HF_InsertRecord(hffd, line, len, &rid);

        if (err != HF_OK) {
            printf("Insert error: %d on line: %s\n", err, line);
            PF_PrintError("HF_InsertRecord");
            exit(1);
        }

        m->totalRecords++;
        m->totalBytesStored += len;
        if (len > m->maxRecordLen) m->maxRecordLen = len;
    }

    fclose(f);
    HF_CloseFile(hffd);

    m->totalPages = get_total_pf_pages(HF_FILE);

    printf("Variable-length: %d records, %d pages\n", m->totalRecords, m->totalPages);
}

/*===============================================================
   STATIC STORAGE (fixed size)
 ===============================================================*/
void build_static(Metrics *m, int recSize) {
    printf("\n=== BUILD STATIC FILE (recSize=%d) ===\n", recSize);

    remove(STATIC_FILE);

    FILE *out = fopen(STATIC_FILE, "wb");
    if (!out) {
        perror("static fopen");
        exit(1);
    }

    FILE *f = fopen(DATASET, "r");
    if (!f) {
        perror("dataset");
        exit(1);
    }

    char line[512];
    char buf[1024];

    while (fgets(line, sizeof(line), f)) {
        trim(line);
        if (line[0] == '\0') continue;

        /* pack record into fixed length */
        memset(buf, 0, recSize);
        strncpy(buf, line, recSize - 1);

        fwrite(buf, 1, recSize, out);

        m->totalRecords++;
        m->totalBytesStored += recSize;
        int len = strlen(line) + 1;
        if (len > m->maxRecordLen) m->maxRecordLen = len;
    }

    fclose(f);
    fclose(out);

    /* number of pages used = ceil(bytes / pageSize) */
    int totalBytes = m->totalRecords * recSize;
    m->totalPages = (totalBytes + PF_PAGE_SIZE - 1) / PF_PAGE_SIZE;

    printf("Static (recSize=%d): %d records, %d pages\n",
           recSize, m->totalRecords, m->totalPages);
}

/*===============================================================
   PRINT METRICS TABLE
 ===============================================================*/
void print_metrics(
    Metrics *var,
    Metrics staticList[],
    int recSizes[]
) {
    printf("\n\n================== PERFORMANCE COMPARISON ==================\n");
    printf("%-15s %-12s %-12s %-12s %-12s\n",
           "Type", "RecSize", "Pages", "BytesStored", "Utilization");

    /* Variable-length */
    double utilVar = (double)var->totalBytesStored /
                     (var->totalPages * PF_PAGE_SIZE);

    printf("%-15s %-12s %-12d %-12d %.4f\n",
           "Variable", "-", var->totalPages, var->totalBytesStored, utilVar);

    /* Static sizes */
    for (int i = 0; i < NUM_STATIC_SIZES; i++) {
        int rs = recSizes[i];
        Metrics *m = &staticList[i];

        double util = (double)m->totalBytesStored /
                      (m->totalPages * PF_PAGE_SIZE);

        printf("%-15s %-12d %-12d %-12d %.4f\n",
               "Static", rs, m->totalPages, m->totalBytesStored, util);
    }
}

/*===============================================================
   MAIN
 ===============================================================*/
int main() {
    printf("=== VARIABLE vs STATIC STORAGE EXPERIMENT ===\n");
    printf("Dataset: %s\n", DATASET);

    /**** Variable-length HF build ****/
    Metrics var;
    metrics_init(&var);
    build_variable(&var);

    /**** Static builds ****/
    Metrics statList[16];
    for (int i = 0; i < NUM_STATIC_SIZES; i++) {
        metrics_init(&statList[i]);
        build_static(&statList[i], STATIC_SIZES[i]);
    }

    /**** Print summary table ****/
    print_metrics(&var, statList, STATIC_SIZES);

    return 0;
}
