#include "hf.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    int  rollno;
    char name[32];
    int  age;
} Student;

int main(void) {
    PF_Init();

    /* create/open table */
    HF_CreateFile("Student.tbl");
    int fd = HF_OpenFile("Student.tbl");

    /* insert */
    Student s = { 101, "Ram", 20 };
    HF_RID rid;
    HF_InsertRecord(fd, &s, sizeof(s), &rid);
    printf("Inserted RID: page=%d slot=%d\n", rid.page, rid.slot);

    /* read back */
    Student out; int len = sizeof(out);
    HF_GetRecord(fd, rid, &out, &len);
    printf("Read: %d %s %d\n", out.rollno, out.name, out.age);

    /* update */
    Student s2 = { 101, "Ram Kumar", 21 };
    HF_UpdateRecord(fd, &rid, &s2, sizeof(s2));

    /* scan */
    HF_Scan scan;
    HF_ScanOpen(fd, &scan);
    while (HF_ScanNext(&scan, &rid, &out, &len) == PFE_OK) {
        printf("Scan RID(%d,%d): %d %s %d\n", rid.page, rid.slot, out.rollno, out.name, out.age);
    }
    HF_ScanClose(&scan);

    /* delete */
    HF_DeleteRecord(fd, rid);

    HF_CloseFile(fd);
    return 0;
}
