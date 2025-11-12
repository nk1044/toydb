#define _POSIX_C_SOURCE 200809L
#include "utils.h"
#include "../pflayer/pf.h"
#include "../hfLayer/hf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HF_FILE      "courses_var.hf"
#define STATIC_FILE  "courses_static.hf"
#define DATASET      "../data/courses.txt"

/* Zero PF globals */
static inline void reset_pf(void) {
    PF_physical_reads = PF_physical_writes = 0;
    PF_logical_reads  = PF_logical_writes  = 0;
    PF_page_fix = PF_page_unfix = PF_page_alloc = PF_page_evicted = 0;
}

/* Dataset record structure */
typedef struct {
    char code[16];
    char name[128];
    double credits;
    char type[4];
} CourseRec;

/* Strip trailing newline from string */
static void strip_newline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && str[len-1] == '\n') {
        str[len-1] = '\0';
    }
    if (len > 1 && str[len-2] == '\r') {
        str[len-2] = '\0';
    }
}

/* Load the dataset into a slotted-page HF file (variable-length) */
static int load_courses_into_hf(const char *dataset, const char *file) {
    FILE *f = fopen(dataset, "r");
    if (!f) { 
        perror("fopen dataset"); 
        printf("ERROR: Could not open %s\n", dataset);
        return -1; 
    }

    remove(file);
    printf("[HF] Creating file: %s\n", file);
    int err = HF_CreateFile(file);
    if (err != PFE_OK) {
        PF_PrintError("HF_CreateFile");
        fclose(f);
        return -1;
    }

    int fd = HF_OpenFile(file);
    if (fd < 0) {
        PF_PrintError("HF_OpenFile");
        fclose(f);
        return -1;
    }

    char line[512];
    int count = 0;

    while (fgets(line, sizeof(line), f)) {
        strip_newline(line);
        
        if (!strchr(line, ';')) continue;
        if (strlen(line) == 0) continue;

        CourseRec c; 
        memset(&c, 0, sizeof(c));

        char *saveptr;
        char *tok = strtok_r(line, ";", &saveptr);
        if (!tok) continue;
        strncpy(c.code, tok, sizeof(c.code) - 1);

        tok = strtok_r(NULL, ";", &saveptr);
        if (!tok) continue;
        strncpy(c.name, tok, sizeof(c.name) - 1);

        tok = strtok_r(NULL, ";", &saveptr);
        if (!tok) continue;
        c.credits = atof(tok);

        tok = strtok_r(NULL, ";", &saveptr);
        if (tok) {
            strncpy(c.type, tok, sizeof(c.type) - 1);
        }

        HF_RID rid;
        err = HF_InsertRecord(fd, &c, (int)sizeof(c), &rid);
        if (err != PFE_OK) {
            PF_PrintError("HF_InsertRecord");
            printf("ERROR: Failed to insert record %d\n", count);
            HF_CloseFile(fd);
            fclose(f);
            return -1;
        }

        count++;
        if (count % 100 == 0)
            printf("[HF] Inserted %d records...\n", count);
    }

    fclose(f);
    HF_CloseFile(fd);
    printf("[HF] Finished inserting %d records into %s\n", count, file);
    return count;
}

int main(void) {
    Stats s; 
    stats_reset(&s);

    printf("=== HF LAYER SPACE UTILIZATION TEST ===\n");
    printf("Dataset: %s\n\n", DATASET);

    /* Initialize PF layer - CRITICAL! */
    PF_Init();

    /* -------- Slotted-page (variable-length) load -------- */
    printf("[TEST] Loading dataset into slotted-page file (%s)\n", HF_FILE);
    reset_pf();
    stats_reset(&s);
    stats_start(&s);

    int n_var = load_courses_into_hf(DATASET, HF_FILE);
    if (n_var < 0) {
        printf("ERROR: Failed to load variable-length file\n");
        return 1;
    }

    stats_stop(&s);
    stats_snapshot_from_pf(&s);
    stats_dump("hf_stats.txt", "Slotted-page load", &s);

    printf("[DONE] Slotted-page load complete: %d records.\n", n_var);
    printf("[INFO] Logical R/W: %lu/%lu | Physical R/W: %lu/%lu\n",
           PF_logical_reads, PF_logical_writes,
           PF_physical_reads, PF_physical_writes);
    printf("[INFO] Time taken: %.2f ms\n\n", stats_elapsed_ms(&s));

    /* -------- Static layout simulation -------- */
    printf("[TEST] Loading dataset into static (fixed 256-byte) file (%s)\n", STATIC_FILE);
    reset_pf();
    stats_reset(&s);
    stats_start(&s);

    remove(STATIC_FILE);
    int err = HF_CreateFile(STATIC_FILE);
    if (err != PFE_OK) {
        PF_PrintError("HF_CreateFile(static)");
        return 1;
    }

    int fd = HF_OpenFile(STATIC_FILE);
    if (fd < 0) {
        PF_PrintError("HF_OpenFile(static)");
        return 1;
    }

    FILE *f = fopen(DATASET, "r");
    if (!f) {
        perror("fopen dataset");
        printf("ERROR: Could not open %s\n", DATASET);
        HF_CloseFile(fd);
        return 1;
    }

    char line[512];
    int n_static = 0;
    while (fgets(line, sizeof(line), f)) {
        strip_newline(line);
        
        if (strlen(line) == 0) continue;
        
        char buf[256];
        memset(buf, 0, sizeof(buf));
        strncpy(buf, line, sizeof(buf) - 1);

        HF_RID rid;
        err = HF_InsertRecord(fd, buf, (int)sizeof(buf), &rid);
        if (err != PFE_OK) {
            PF_PrintError("HF_InsertRecord(static)");
            printf("ERROR: Failed to insert static record %d\n", n_static);
            fclose(f);
            HF_CloseFile(fd);
            return 1;
        }

        n_static++;
        if (n_static % 100 == 0)
            printf("[HF] Static inserted %d records...\n", n_static);
    }

    fclose(f);
    HF_CloseFile(fd);

    stats_stop(&s);
    stats_snapshot_from_pf(&s);
    stats_dump("hf_stats.txt", "Static-page load", &s);

    printf("[DONE] Static-page load complete: %d records.\n", n_static);
    printf("[INFO] Logical R/W: %lu/%lu | Physical R/W: %lu/%lu\n",
           PF_logical_reads, PF_logical_writes,
           PF_physical_reads, PF_physical_writes);
    printf("[INFO] Time taken: %.2f ms\n", stats_elapsed_ms(&s));

    printf("\n[OUTPUT] All statistics written to hf_stats.txt\n");
    return 0;
}