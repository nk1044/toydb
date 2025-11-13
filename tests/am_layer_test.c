/* tests/test4_indextest.c
 *
 * Compares three index-construction approaches on an HF file:
 *  A) Build index by scanning existing HF and calling AM_InsertEntry
 *  B) Build index by inserting keys in random order into empty index
 *  C) Bulk-load by sorting keys and writing leaf pages + internal levels
 *
 * Output: build time (ms), index file pages, sample lookup time (ms)
 *
 * Assumption: HF records are text lines and the key (roll no) is the first integer token.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>


#include "../hfLayer/hf.h"
#include "../pflayer/pf.h"
#include "../amLayer/am.h" /* path adjust if needed: your am.h location */

#define DATA_HF_FILE "courses_var.hf" /* HF file to scan; adjust if needed */
#define RELNAME "students"            /* base name for index files: students.0, students.1, ... */
#define IDXNO_BASE 100                /* index number base to avoid clobbering others */

/* small helper timing */
static double now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1000.0 + (double)t.tv_nsec / 1e6;
}

/* simple container for (key, recId) */
typedef struct { int key; int recPage; short recSlot; } Pair;

/* parse first integer in record string; returns 1 on OK */
static int parse_key_from_record(const char *rec, int *out) {
    if (!rec) return 0;
    const char *p = rec;
    while (*p && (*p == ' ' || *p == '\t')) p++;
    if (!*p) return 0;
    /* parse integer */
    char *end;
    long v = strtol(p, &end, 10);
    if (p == end) {
        /* no integer at start -> try scanning for first number token */
        p = rec;
        while (*p && !((*p >= '0' && *p <= '9') || (*p == '-' && p[1] && isdigit((unsigned char)p[1])))) p++;
        if (!*p) return 0;
        v = strtol(p, &end, 10);
        if (p == end) return 0;
    }
    *out = (int)v;
    return 1;
}
int cmp_pairs(const void *pa, const void *pb) {
        const Pair *a = (const Pair *)pa;
        const Pair *b = (const Pair *)pb;
        if (a->key < b->key) return -1;
        if (a->key > b->key) return 1;
        return 0;
    };
/* read all HF records into dynamic Pair array */
static Pair *extract_pairs_from_hf(int *out_n) {
    HF_Scan scan;
    int hf_fd = HF_OpenFile(DATA_HF_FILE);
    if (hf_fd < 0) {
        fprintf(stderr, "HF_OpenFile(%s) failed\n", DATA_HF_FILE);
        *out_n = 0;
        return NULL;
    }

    if (HF_ScanOpen(hf_fd, &scan) != PFE_OK) {
        fprintf(stderr, "HF_ScanOpen failed\n");
        HF_CloseFile(hf_fd);
        *out_n = 0;
        return NULL;
    }

    Pair *arr = NULL;
    int cap = 0, cnt = 0;
    char buf[8192];
    int len;
    HF_RID rid;

    while (1) {
        len = (int)sizeof(buf);
        int res = HF_ScanNext(&scan, &rid, buf, &len);
        if (res == PFE_EOF) break;
        if (res != PFE_OK) {
            fprintf(stderr, "HF_ScanNext error %d\n", res);
            break;
        }
        int key;
        if (!parse_key_from_record(buf, &key)) {
            /* skip record without key */
            continue;
        }
        if (cnt >= cap) {
            cap = cap ? cap * 2 : 1024;
            arr = realloc(arr, cap * sizeof(Pair));
        }
        arr[cnt].key = key;
        arr[cnt].recPage = rid.pageNum;
        arr[cnt].recSlot = rid.slotNum;

        cnt++;
    }

    HF_ScanClose(&scan);
    HF_CloseFile(hf_fd);

    *out_n = cnt;
    return arr;
}

/* utility: remove index file if exists */
static void remove_index_file(const char *base, int idxno) {
    char fname[256];
    snprintf(fname, sizeof(fname), "%s.%d", base, idxno);
    remove(fname);
}

/* count pages in PF file */
static int count_pf_pages(const char *fname) {
    int fd = PF_OpenFile(fname);
    if (fd < 0) return -1;
    int n = 0; char *p;
    while (PF_GetThisPage(fd, n, &p) == PFE_OK) {
        PF_UnfixPage(fd, n, 0);
        n++;
    }
    PF_CloseFile(fd);
    return n;
}

/* Build approach A: scan HF and call AM_InsertEntry in order read */
static double build_from_existing(Pair *pairs, int n, int index_no) {
    char idxbase[128];
    snprintf(idxbase, sizeof(idxbase), "%s", RELNAME);
    remove_index_file(idxbase, index_no);

    AM_CreateIndex(idxbase, index_no, 'i', sizeof(int));
    char fname[256]; snprintf(fname, sizeof(fname), "%s.%d", idxbase, index_no);
    int fd = PF_OpenFile(fname);
    if (fd < 0) { fprintf(stderr, "open idx fail\n"); return -1; }

    double t0 = now_ms();
    for (int i = 0; i < n; ++i) {
        AM_InsertEntry(fd, 'i', (int)sizeof(int), (char *)&pairs[i].key, (pairs[i].recPage<<16) | (pairs[i].recSlot & 0xffff));
    }
    double t1 = now_ms();

    PF_CloseFile(fd);
    return t1 - t0;
}

/* Build approach B: incremental inserting in random order */
static double build_incremental_random(Pair *pairs, int n, int index_no) {
    /* Shuffle copy */
    Pair *sh = malloc(n * sizeof(Pair));
    memcpy(sh, pairs, n * sizeof(Pair));
    for (int i = n-1; i > 0; --i) {
        int j = rand() % (i+1);
        Pair tmp = sh[i]; sh[i] = sh[j]; sh[j] = tmp;
    }

    char idxbase[128]; snprintf(idxbase, sizeof(idxbase), "%s", RELNAME);
    remove_index_file(idxbase, index_no);

    AM_CreateIndex(idxbase, index_no, 'i', sizeof(int));
    char fname[256]; snprintf(fname, sizeof(fname), "%s.%d", idxbase, index_no);
    int fd = PF_OpenFile(fname);
    if (fd < 0) { fprintf(stderr, "open idx fail\n"); free(sh); return -1; }

    double t0 = now_ms();
    for (int i = 0; i < n; ++i) {
        AM_InsertEntry(fd, 'i', (int)sizeof(int), (char *)&sh[i].key, (sh[i].recPage<<16) | (sh[i].recSlot & 0xffff));
    }
    double t1 = now_ms();

    PF_CloseFile(fd);
    free(sh);
    return t1 - t0;
}

/* Helper writing a leaf page in AM leaf format */
/* We create a leaf page that contains keys[0..k-1] with one recId each. */
static int write_leaf_page_direct(int pfFd, Pair *pairs_sorted, int start, int count, int attrLength) {
    char *pageBuf;
    int pnum;
    int err = PF_AllocPage(pfFd, &pnum, &pageBuf);
    if (err != PFE_OK) return -1;

    AM_LEAFHEADER head;
    head.pageType = 'l';
    head.nextLeafPage = AM_NULL_PAGE;
    head.recIdPtr = (short)(PF_PAGE_SIZE - AM_si - AM_ss); /* start recId at end */
    head.keyPtr = AM_sl;
    head.freeListPtr = AM_NULL;
    head.numinfreeList = 0;
    head.attrLength = (short)attrLength;
    head.numKeys = (short)count;

    /* compute maxKeys just like AM_CreateIndex did */
    int maxKeys = (PF_PAGE_SIZE - AM_sint - AM_si) / (AM_si + attrLength);
    if ((maxKeys % 2) != 0) head.maxKeys = (short)(maxKeys - 1);
    else head.maxKeys = (short)maxKeys;

    /* initialize page */
    memset(pageBuf, 0, PF_PAGE_SIZE);
    memcpy(pageBuf, &head, AM_sl);

    /* rec area pointer will move downwards for each recId chain element */
    short recPtr = (short)(PF_PAGE_SIZE - AM_si - AM_ss);

    for (int i = 0; i < count; ++i) {
        int key = pairs_sorted[start + i].key;
        int recIdInt = (pairs_sorted[start + i].recPage<<16) | (pairs_sorted[start + i].recSlot & 0xffff);

        /* write key at keyPtr */
        memcpy(pageBuf + AM_sl + i * (attrLength + AM_ss), &key, sizeof(int));
        /* write pointer to recId chain (single element) -- store recPtr in the key slot's recId pointer position */
        short recPtrThis = recPtr;
        memcpy(pageBuf + AM_sl + i * (attrLength + AM_ss) + attrLength, &recPtrThis, AM_ss);

        /* write recId at recPtr */
        memcpy(pageBuf + recPtr, &recIdInt, AM_si);
        /* next pointer = 0 (end) */
        short zero = 0;
        memcpy(pageBuf + recPtr + AM_si, &zero, AM_ss);

        /* move recPtr down for next */
        recPtr = (short)(recPtr - (AM_si + AM_ss));
    }

    /* set header keys/pointers */
    head.recIdPtr = recPtr + AM_si + AM_ss; /* recIdPtr stored as next available */
    head.keyPtr = (short)(AM_sl + count * (attrLength + AM_ss));
    /* copy header into page */
    memcpy(pageBuf, &head, AM_sl);

    /* unfix page (mark dirty) */
    PF_UnfixPage(pfFd, pnum, TRUE);
    return pnum;
}

/* Build internal level from children page nums and boundary keys */
static int build_internal_level(int pfFd, int *childPages, int childCount, int attrLength, int **outParents, int *outParentCount) {
    /* compute recSize = attrLength + AM_si for internal entries */
    int recSize = attrLength + AM_si;
    int maxKeys = (PF_PAGE_SIZE - AM_sint - AM_si) / (AM_si + attrLength);
    if ((maxKeys % 2) != 0) maxKeys--;

    /* estimate how many parent pages we need */
    int estimatedParents = (childCount + maxKeys) / (maxKeys + 1);
    int *parents = malloc(sizeof(int) * (estimatedParents + 4));
    int pcount = 0;

    int i = 0;
    while (i < childCount) {
        /* allocate parent page */
        char *pageBuf; int pnum;
        int err = PF_AllocPage(pfFd, &pnum, &pageBuf);
        if (err != PFE_OK) { free(parents); *outParents = NULL; return -1; }

        AM_INTHEADER ihead;
        ihead.pageType = 'i';
        ihead.numKeys = 0;
        ihead.maxKeys = (short)maxKeys;
        ihead.attrLength = (short)attrLength;

        memset(pageBuf, 0, PF_PAGE_SIZE);
        memcpy(pageBuf, &ihead, AM_sint);

        /* first pointer is childPages[i] */
        memcpy(pageBuf + AM_sint, &childPages[i], AM_si);

        int keys_here = 0;
        i++;
        while (i < childCount && keys_here < maxKeys) {
            /* boundary key is first key of childPages[i] — need to read child page to extract first key */
            char *childBuf;
            PF_GetThisPage(pfFd, childPages[i], &childBuf);
            AM_LEAFHEADER lhead; memcpy(&lhead, childBuf, AM_sl);
            /* first key: located at AM_sl (first key slot) */
            int firstKey;
            memcpy(&firstKey, childBuf + AM_sl, sizeof(int));
            PF_UnfixPage(pfFd, childPages[i], FALSE);

            /* write key and pointer */
            memcpy(pageBuf + AM_sint + AM_si + keys_here * recSize, &firstKey, attrLength);
            memcpy(pageBuf + AM_sint + AM_si + keys_here * recSize + attrLength, &childPages[i], AM_si);

            keys_here++;
            i++;
        }

        ihead.numKeys = (short)keys_here;
        memcpy(pageBuf, &ihead, AM_sint);

        PF_UnfixPage(pfFd, pnum, TRUE);
        parents[pcount++] = pnum;
    }

    *outParents = parents;
    *outParentCount = pcount;
    return 0;
}

/* Bulk load implementation:
 * - pairs_sorted[] must be sorted by key
 * - create leaf pages sequentially, then build internal levels
 */
static double bulk_load_sorted(Pair *pairs_sorted, int n, int index_no, int attrLength) {
    char idxbase[128]; snprintf(idxbase, sizeof(idxbase), "%s", RELNAME);
    remove_index_file(idxbase, index_no);

    /* create index file */
    AM_CreateIndex(idxbase, index_no, 'i', attrLength);
    char fname[256]; snprintf(fname, sizeof(fname), "%s.%d", idxbase, index_no);
    int fd = PF_OpenFile(fname);
    if (fd < 0) { fprintf(stderr, "open idx fail\n"); return -1; }

    double t0 = now_ms();

    /* Build leaf pages */
    int maxKeys = (PF_PAGE_SIZE - AM_sint - AM_si) / (AM_si + attrLength);
    if ((maxKeys % 2) != 0) maxKeys--;
    int keys_per_leaf = maxKeys; /* approx */
    int leafCountCap = (n + keys_per_leaf - 1) / keys_per_leaf + 4;
    int *leafPages = malloc(sizeof(int) * leafCountCap);
    int leafCount = 0;

    int i = 0;
    while (i < n) {
        int take = (n - i) < keys_per_leaf ? (n - i) : keys_per_leaf;
        int pnum = write_leaf_page_direct(fd, pairs_sorted, i, take, attrLength);
        if (pnum < 0) { fprintf(stderr, "leaf write fail\n"); break; }
        leafPages[leafCount++] = pnum;
        i += take;
    }

    /* link leaf nextLeafPage pointers */
    for (int k = 0; k < leafCount - 1; ++k) {
        char *pb; PF_GetThisPage(fd, leafPages[k], &pb);
        AM_LEAFHEADER lh; memcpy(&lh, pb, AM_sl);
        lh.nextLeafPage = leafPages[k+1];
        memcpy(pb, &lh, AM_sl);
        PF_UnfixPage(fd, leafPages[k], TRUE);
    }

    /* Build internal levels */
    int *curLevel = leafPages;
    int curCount = leafCount;
    int *nextLevel = NULL;
    int nextCount = 0;

    while (curCount > 1) {
        int *parents = NULL; int parentCount = 0;
        if (build_internal_level(fd, curLevel, curCount, attrLength, &parents, &parentCount) != 0) {
            fprintf(stderr, "build_internal_level failed\n");
            break;
        }

        /* if current level was dynamically allocated leafPages array, avoid freeing it incorrectly */
        if (curLevel != leafPages) free(curLevel);

        curLevel = parents;
        curCount = parentCount;
    }

    /* The last remaining page in curLevel is root; we are done */
    double t1 = now_ms();

    PF_CloseFile(fd);
    if (curLevel != leafPages) free(curLevel);
    free(leafPages);

    return t1 - t0;
}

/* sample lookup + time measure: open index, do single key search using AM_OpenIndexScan + AM_FindNextEntry */
static double sample_lookup_time(int index_no, int keyToFind, int trials) {
    char idxbase[128]; snprintf(idxbase, sizeof(idxbase), "%s", RELNAME);
    char fname[256]; snprintf(fname, sizeof(fname), "%s.%d", idxbase, index_no);
    int fd = PF_OpenFile(fname);
    if (fd < 0) return -1;

    double t0 = now_ms();
    for (int i = 0; i < trials; ++i) {
        int sd = AM_OpenIndexScan(fd, 'i', sizeof(int), EQUAL, (char *)&keyToFind);
        if (sd < 0) continue;
        int rid;
        while ((rid = AM_FindNextEntry(sd)) >= 0) {
            /* just iterate; ignore recId value */
        }
        AM_CloseIndexScan(sd);
    }
    double t1 = now_ms();
    PF_CloseFile(fd);
    return (t1 - t0) / (double)trials;
}

/* main: orchestrate the three methods and present table */
int main(void) {
    printf("=== Index construction comparison ===\n");
    PF_Init();

    int n;
    Pair *pairs = extract_pairs_from_hf(&n);
    if (!pairs || n == 0) {
        fprintf(stderr, "no pairs extracted\n");
        return 1;
    }
    printf("Extracted %d key/RID pairs from HF file\n", n);

    /* choose few example keys for lookups */
    int sampleKeys[5];
    for (int i = 0; i < 5 && i < n; ++i) sampleKeys[i] = pairs[i * (n/5)].key;

    /* Approach A: build-from-existing (preserve order) */
    int idxA = IDXNO_BASE + 1;
    double tA = build_from_existing(pairs, n, idxA);
    char nameA[256]; snprintf(nameA, sizeof(nameA), "%s.%d", RELNAME, idxA);
    int pagesA = count_pf_pages(nameA);
    double lookupA = sample_lookup_time(idxA, sampleKeys[0], 3);

    /* Approach B: incremental random */
    int idxB = IDXNO_BASE + 2;
    double tB = build_incremental_random(pairs, n, idxB);
    char nameB[256]; snprintf(nameB, sizeof(nameB), "%s.%d", RELNAME, idxB);
    int pagesB = count_pf_pages(nameB);
    double lookupB = sample_lookup_time(idxB, sampleKeys[0], 3);

    /* Approach C: bulk-load sorted */
    /* make sorted copy */
    Pair *pairs_sorted = malloc(sizeof(Pair) * n);
    memcpy(pairs_sorted, pairs, sizeof(Pair) * n);
    
    /* above lambda is a bit awkward in pure C; replace with classic comparator below if your compiler rejects lambdas */
    /* simpler replacement comparator: */
    
    qsort(pairs_sorted, n, sizeof(Pair), cmp_pairs);

    int idxC = IDXNO_BASE + 3;
    double tC = bulk_load_sorted(pairs_sorted, n, idxC, (int)sizeof(int));
    char nameC[256]; snprintf(nameC, sizeof(nameC), "%s.%d", RELNAME, idxC);
    int pagesC = count_pf_pages(nameC);
    double lookupC = sample_lookup_time(idxC, sampleKeys[0], 3);

    /* print comparison table */
    printf("\n=== Results ===\n");
    printf("%-20s %-12s %-12s %-12s\n", "Method", "Build ms", "Pages", "Lookup ms");
    printf("%-20s %-12.2f %-12d %-12.4f\n", "Build-from-existing", tA, pagesA, lookupA);
    printf("%-20s %-12.2f %-12d %-12.4f\n", "Incremental-random", tB, pagesB, lookupB);
    printf("%-20s %-12.2f %-12d %-12.4f\n", "Bulk-load sorted", tC, pagesC, lookupC);

    free(pairs);
    free(pairs_sorted);
    return 0;
}
