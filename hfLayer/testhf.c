#include "hf.h"
#include "pf.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    int  rollno;
    char name[32];
    int  age;
} Student;

void print_error(const char* msg, int err) {
    printf("ERROR: %s (error code: %d)\n", msg, err);
}

int main(void) {
    int err;
    
    PF_Init();

    /* create table */
    printf("Creating Student.tbl...\n");
    err = HF_CreateFile("Student.tbl");
    if (err != PFE_OK) {
        print_error("Failed to create file", err);
        return 1;
    }
    
    /* open table */
    printf("Opening Student.tbl...\n");
    int fd = HF_OpenFile("Student.tbl");
    if (fd < 0) {
        print_error("Failed to open file", fd);
        return 1;
    }
    printf("File opened successfully (fd=%d)\n", fd);

    /* insert */
    printf("\n--- Testing INSERT ---\n");
    Student s = { 101, "Ram", 20 };
    HF_RID rid = {0};
    
    err = HF_InsertRecord(fd, &s, sizeof(s), &rid);
    if (err != PFE_OK) {
        print_error("Failed to insert record", err);
        HF_CloseFile(fd);
        return 1;
    }
    printf("✓ Inserted RID: page=%d slot=%d\n", rid.page, rid.slot);

    /* read back */
    printf("\n--- Testing GET ---\n");
    Student out;
    int len = sizeof(out);
    
    err = HF_GetRecord(fd, rid, &out, &len);
    if (err != PFE_OK) {
        print_error("Failed to get record", err);
        HF_CloseFile(fd);
        return 1;
    }
    printf("✓ Read: rollno=%d name=\"%s\" age=%d\n", out.rollno, out.name, out.age);

    /* update */
    printf("\n--- Testing UPDATE ---\n");
    Student s2 = { 101, "Ram Kumar", 21 };
    
    err = HF_UpdateRecord(fd, &rid, &s2, sizeof(s2));
    if (err != PFE_OK) {
        print_error("Failed to update record", err);
        HF_CloseFile(fd);
        return 1;
    }
    printf("✓ Updated RID: page=%d slot=%d\n", rid.page, rid.slot);
    
    /* verify update */
    len = sizeof(out);
    err = HF_GetRecord(fd, rid, &out, &len);
    if (err != PFE_OK) {
        print_error("Failed to get updated record", err);
        HF_CloseFile(fd);
        return 1;
    }
    printf("✓ After update: rollno=%d name=\"%s\" age=%d\n", out.rollno, out.name, out.age);

    /* insert more records for scan test */
    printf("\n--- Inserting more records ---\n");
    Student students[] = {
        { 102, "Sita", 19 },
        { 103, "Lakshman", 22 },
        { 104, "Bharat", 20 }
    };
    
    for (int i = 0; i < 3; i++) {
        HF_RID tmpRid;
        err = HF_InsertRecord(fd, &students[i], sizeof(Student), &tmpRid);
        if (err != PFE_OK) {
            print_error("Failed to insert student", err);
            continue;
        }
        printf("✓ Inserted: %s (page=%d, slot=%d)\n", 
               students[i].name, tmpRid.page, tmpRid.slot);
    }

    /* scan */
    printf("\n--- Testing SCAN ---\n");
    HF_Scan scan;
    err = HF_ScanOpen(fd, &scan);
    if (err != PFE_OK) {
        print_error("Failed to open scan", err);
        HF_CloseFile(fd);
        return 1;
    }
    
    int count = 0;
    HF_RID scanRid;
    while (1) {
        len = sizeof(out);
        err = HF_ScanNext(&scan, &scanRid, &out, &len);
        if (err == PFE_EOF) {
            break;
        }
        if (err != PFE_OK) {
            print_error("Scan error", err);
            break;
        }
        printf("✓ Scan RID(%d,%d): rollno=%d name=\"%s\" age=%d\n", 
               scanRid.page, scanRid.slot, out.rollno, out.name, out.age);
        count++;
    }
    printf("Total records scanned: %d\n", count);
    
    err = HF_ScanClose(&scan);
    if (err != PFE_OK) {
        print_error("Failed to close scan", err);
    }

    /* delete */
    printf("\n--- Testing DELETE ---\n");
    err = HF_DeleteRecord(fd, rid);
    if (err != PFE_OK) {
        print_error("Failed to delete record", err);
    } else {
        printf("✓ Deleted RID: page=%d slot=%d\n", rid.page, rid.slot);
    }
    
    /* verify deletion */
    len = sizeof(out);
    err = HF_GetRecord(fd, rid, &out, &len);
    if (err == PFE_PAGEFREE) {
        printf("✓ Record correctly marked as deleted\n");
    } else if (err == PFE_OK) {
        printf("✗ ERROR: Record still exists after deletion!\n");
    } else {
        print_error("Unexpected error on deleted record", err);
    }

    /* close file */
    printf("\n--- Closing file ---\n");
    err = HF_CloseFile(fd);
    if (err != PFE_OK) {
        print_error("Failed to close file", err);
        return 1;
    }
    printf("✓ File closed successfully\n");
    
    printf("\n=== All tests completed ===\n");
    return 0;
}