#define _POSIX_C_SOURCE 200809L
#include "utils.h"
#include "../pflayer/pf.h"
#include "../hfLayer/hf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DBFILE    "pf_testfile.db"
#define HF_FILE   "courses.hf"
#define DATASET   "../data/courses.txt"
#define N_PAGES   2000

typedef struct {
    char code[16];
    char name[128];
    double credits;
    char type[4];
} CourseRec;

/* Strip trailing newline */
static void strip_newline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && str[len-1] == '\n') {
        str[len-1] = '\0';
    }
    if (len > 1 && str[len-2] == '\r') {
        str[len-2] = '\0';
    }
}
static inline void reset_pf(void) {
    PF_physical_reads = PF_physical_writes = 0;
    PF_logical_reads  = PF_logical_writes  = 0;
    PF_page_alloc = PF_page_evicted = 0;
}

static int load_dataset(const char *path, int hf_fd) {
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("ERROR: Failed to open dataset file: %s\n", path);
        perror("fopen");
        return -1;
    }

    setvbuf(stdout, NULL, _IONBF, 0); // disable stdout buffering
    char line[512];
    int count = 0;

    printf("INFO: Loading dataset from %s\n", path);
    while (fgets(line, sizeof(line), f)) {
        strip_newline(line);
        
        if (!strchr(line, ';')) continue;
        if (strlen(line) == 0) continue;
        
        CourseRec c;
        memset(&c, 0, sizeof(c));

        char *saveptr;
        char *tok = strtok_r(line, ";", &saveptr);
        if (!tok) continue; 
        strncpy(c.code, tok, sizeof(c.code)-1);
        
        tok = strtok_r(NULL, ";", &saveptr);
        if (!tok) continue; 
        strncpy(c.name, tok, sizeof(c.name)-1);
        
        tok = strtok_r(NULL, ";", &saveptr);
        if (!tok) continue; 
        c.credits = atof(tok);
        
        tok = strtok_r(NULL, ";", &saveptr);
        if (tok) strncpy(c.type, tok, sizeof(c.type)-1);

        HF_RID rid;
        int err = HF_InsertRecord(hf_fd, &c, sizeof(c), &rid);
        if (err != PFE_OK) {
            printf("ERROR: Failed to insert record %d (error: %d)\n", count, err);
            PF_PrintError("HF_InsertRecord");
            continue;
        }
        
        count++;
    }

    fclose(f);
    printf("INFO: Finished loading %d records from dataset\n", count);
    return count;
}

int main(void) {
    Stats s;

    printf("=== PF Layer Read/Write Mix Test ===\n\n");
    
    /* Initialize PF layer */
    printf("INFO: Initializing PF layer...\n");
    PF_Init();

    /* Strategies: LRU (0), MRU (1) */
    const char *names[] = {"LRU", "MRU"};
    struct { int reads; int writes; } mix[] = {
        {10000, 0}, {7500, 2500}, {5000, 5000}, {2500, 7500}, {0, 10000}
    };

    /* Optional: prepare dataset file via HF */
    printf("\n--- Preparing HF dataset file ---\n");
    remove(HF_FILE);
    int hf_result = HF_CreateFile(HF_FILE);
    if (hf_result != PFE_OK) {
        printf("ERROR: HF_CreateFile failed with error: %d\n", hf_result);
        PF_PrintError("HF_CreateFile");
    } else {
        printf("INFO: HF_CreateFile succeeded\n");
        
        int hf_fd = HF_OpenFile(HF_FILE);
        if (hf_fd >= 0) {
            printf("INFO: HF_OpenFile succeeded, fd=%d\n", hf_fd);
            int n = load_dataset(DATASET, hf_fd);
            if (n > 0) {
                printf("INFO: Loaded %d records from dataset into %s\n", n, HF_FILE);
            } else {
                printf("WARNING: Failed to load dataset (error code: %d)\n", n);
            }
            HF_CloseFile(hf_fd);
            printf("INFO: Closed HF file\n");
        } else {
            printf("ERROR: HF_OpenFile failed, fd=%d\n", hf_fd);
            PF_PrintError("HF_OpenFile");
        }
    }

    /* ===== PF TEST ===== */
    for (int st = 0; st < 2; ++st) {
        printf("\n========================================\n");
        printf("Testing with strategy: %s\n", names[st]);
        printf("========================================\n");
        
        USE_MRU = (st == 1);  // set global strategy

        remove(DBFILE);
        if (PF_CreateFile(DBFILE) != PFE_OK) {
            PF_PrintError("PF_CreateFile");
            printf("ERROR: PF_CreateFile failed for strategy %s\n", names[st]);
            return 1;
        }
        printf("INFO: PF_CreateFile succeeded\n");

        int fd = PF_OpenFile(DBFILE);
        if (fd < 0) {
            PF_PrintError("PF_OpenFile");
            printf("ERROR: PF_OpenFile failed, fd=%d\n", fd);
            return 1;
        }
        printf("INFO: PF_OpenFile succeeded, fd=%d\n", fd);

        /* ----- Create initial N pages ----- */
        reset_pf();
        stats_reset(&s);
        printf("\n--- Creating %d initial pages ---\n", N_PAGES);
        stats_start(&s);

        int alloc_errors = 0;
        for (int i = 0; i < N_PAGES; ++i) {
            int pno;
            char *page;
            if (PF_AllocPage(fd, &pno, &page) != PFE_OK) {
                PF_PrintError("PF_AllocPage");
                alloc_errors++;
                continue;
            }
            page[0] = (char)(i & 0xFF);
            if (PF_UnfixPage(fd, pno, 1) != PFE_OK) {
                PF_PrintError("PF_UnfixPage(alloc)");
                alloc_errors++;
            }
            
            if ((i + 1) % 500 == 0) {
                printf("INFO: Allocated %d pages so far...\n", i + 1);
            }
        }
        
        if (alloc_errors > 0) {
            printf("WARNING: Had %d allocation errors\n", alloc_errors);
        }

        stats_stop(&s);
        {
            char label[128];
            snprintf(label, sizeof(label), "%s create %d pages", names[st], N_PAGES);
            stats_dump("pf_stats.txt", label, &s);
            printf("RESULT: %s - Page creation completed in %.3f ms\n", 
                   names[st], stats_elapsed_ms(&s));
        }

        /* ----- Read/Write Mixtures ----- */
        for (int i = 0; i < (int)(sizeof(mix)/sizeof(mix[0])); ++i) {
            printf("\n--- Mix %d: R=%d, W=%d ---\n", 
                   i, mix[i].reads, mix[i].writes);
            
            reset_pf();
            stats_reset(&s);
            stats_start(&s);

            int read_errors = 0, write_errors = 0;

            /* Reads */
            for (int r = 0; r < mix[i].reads; ++r) {
                int pno = (r * 13) % N_PAGES;
                char *page;
                if (PF_GetThisPage(fd, pno, &page) == PFE_OK) {
                    volatile char tmp = page[0];
                    (void)tmp;
                    if (PF_UnfixPage(fd, pno, 0) != PFE_OK) {
                        read_errors++;
                    }
                } else {
                    read_errors++;
                }
                
                if (r > 0 && r % 2000 == 0) {
                    printf("INFO: Completed %d/%d reads...\n", r, mix[i].reads);
                }
            }

            /* Writes */
            for (int w = 0; w < mix[i].writes; ++w) {
                int pno = (w * 7) % N_PAGES;
                char *page;
                if (PF_GetThisPage(fd, pno, &page) == PFE_OK) {
                    page[1] ^= (char)(w & 0xFF);
                    if (PF_UnfixPage(fd, pno, 1) != PFE_OK) {
                        write_errors++;
                    }
                } else {
                    write_errors++;
                }
                
                if (w > 0 && w % 2000 == 0) {
                    printf("INFO: Completed %d/%d writes...\n", w, mix[i].writes);
                }
            }

            stats_stop(&s);
            char label[128];
            snprintf(label, sizeof(label), "%s mix R=%d W=%d (PF_MAX_BUFS=%d)",
                     names[st], mix[i].reads, mix[i].writes, PF_MAX_BUFS);
            stats_dump("pf_stats.txt", label, &s);
            
            printf("RESULT: %s mix %d completed in %.3f ms\n", 
                   names[st], i, stats_elapsed_ms(&s));
            if (read_errors > 0 || write_errors > 0) {
                printf("WARNING: read errors: %d, write errors: %d\n", 
                       read_errors, write_errors);
            }
        }

        printf("\n--- Closing PF file for strategy %s ---\n", names[st]);
        if (PF_CloseFile(fd) != PFE_OK) {
            PF_PrintError("PF_CloseFile");
            printf("ERROR: PF_CloseFile failed\n");
        } else {
            printf("INFO: PF_CloseFile succeeded\n");
        }
    }

    printf("\n=== All tests completed successfully! ===\n");
    return 0;
}