#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ===============================
   Statistics
   =============================== */
typedef struct {
    struct timespec start_time, end_time;
    unsigned long logical_reads, logical_writes;
    unsigned long physical_reads, physical_writes;
    unsigned long  page_alloc, page_evicted;
    double avg_space_util;
} Stats;

static inline void stats_start(Stats *s) { clock_gettime(CLOCK_MONOTONIC, &s->start_time); }
static inline void stats_stop (Stats *s) { clock_gettime(CLOCK_MONOTONIC, &s->end_time); }
static inline double stats_elapsed_ms(const Stats *s) {
    return (s->end_time.tv_sec - s->start_time.tv_sec)*1000.0 +
           (s->end_time.tv_nsec - s->start_time.tv_nsec)/1e6;
}
static inline void stats_reset(Stats *s) { memset(s, 0, sizeof(*s)); }

extern unsigned long PF_physical_reads, PF_physical_writes;
extern unsigned long PF_logical_reads, PF_logical_writes;
extern unsigned long  PF_page_alloc, PF_page_evicted;

static inline void stats_snapshot_from_pf(Stats *s) {
    s->physical_reads  = PF_physical_reads;
    s->physical_writes = PF_physical_writes;
    s->logical_reads   = PF_logical_reads;
    s->logical_writes  = PF_logical_writes;
    s->page_alloc      = PF_page_alloc;
    s->page_evicted    = PF_page_evicted;
}

static inline void stats_dump(const char *filename, const char *label, Stats *s) {
    FILE *f = fopen(filename, "a");
    if (!f) return;

    fprintf(f, "==================== %s ====================\n", label);
    fprintf(f,
        "Time (ms):           %.3f\n"
        "Logical Reads:       %lu\n"
        "Logical Writes:      %lu\n"
        "Physical Reads:      %lu\n"
        "Physical Writes:     %lu\n"
        "Page Allocations:    %lu\n"
        "Page Evictions:      %lu\n"
        "Avg Space Util (%):  %.2f\n"
        "---------------------------------------------\n\n",
        stats_elapsed_ms(s),
        s->logical_reads,
        s->logical_writes,
        s->physical_reads,
        s->physical_writes,
        s->page_alloc,
        s->page_evicted,
        s->avg_space_util
    );

    fclose(f);
}

#endif
