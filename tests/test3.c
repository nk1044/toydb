#define _POSIX_C_SOURCE 200809L
#include "utils.h"
#include "../hfLayer/hf.h"
#include "../amlayer/am.h"

#define HF_FILE "courses_var.hf"
#define IDX_FILE "courses.idx"

int main() {
    Stats s; stats_reset(&s);

    // 1️⃣ Build from existing HF
    PF_ResetCounters();
    stats_start(&s);
    AM_CreateFromFile(IDX_FILE, HF_FILE, 0, sizeof(int)); // assume key=rollno
    stats_stop(&s);
    stats_snapshot_from_pf(&s);
    stats_dump("am_stats.txt", "Index build from existing file", &s);

    // 2️⃣ Incremental inserts
    PF_ResetCounters();
    stats_start(&s);
    int fd = HF_Open(HF_FILE);
    AM_CreateEmpty(IDX_FILE, sizeof(int));
    HF_ScanHandle scan;
    HF_ScanOpen(fd, &scan);
    void *rec; int len;
    while (HF_ScanNext(&scan) == 0) {
        HF_ScanGet(&scan, &rec, &len);
        const int *roll = (const int*)rec;
        AM_Insert(IDX_FILE, roll, rec, len);
    }
    HF_ScanClose(&scan);
    HF_Close(fd);
    stats_stop(&s);
    stats_snapshot_from_pf(&s);
    stats_dump("am_stats.txt", "Index incremental inserts", &s);

    // 3️⃣ Bulk-load sorted
    PF_ResetCounters();
    stats_start(&s);
    AM_BulkLoadSorted(IDX_FILE, HF_FILE, sizeof(int));
    stats_stop(&s);
    stats_snapshot_from_pf(&s);
    stats_dump("am_stats.txt", "Bulk-load sorted", &s);

    return 0;
}
