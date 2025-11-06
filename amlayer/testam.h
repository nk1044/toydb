#ifndef TESTAM_H
#define TESTAM_H

#include <stddef.h>   /* for size_t */
#include <stdint.h>   /* for standard integer types */

/*
 *  Record ID type (recid)
 *  Your AM layer uses simple integers, so this remains int.
 */
typedef int RecIdType;

/* Convert between record-id and integer */
#define RecIdToInt(recid)   (recid)
#define IntToRecId(intval)  (intval)

/*
 *  Attribute types
 */
#define CHAR_TYPE   'c'
#define INT_TYPE    'i'
#define FLOAT_TYPE  'f'

#define INT_SIZE    (sizeof(int))
#define FLOAT_SIZE  (sizeof(float))

/*
 *  Compare operators
 */
#define EQ_OP   1
#define LT_OP   2
#define GT_OP   3
#define LE_OP   4
#define GE_OP   5
#define NE_OP   6

/*
 *  Relation name used by tests
 */
#define RELNAME "testrel"

/*
 *  Test record structure
 */
#define NAMELENGTH 11
#define RECNAME_INDEXNO 0
#define RECVAL_INDEXNO  1

typedef struct smallrec {
    char recname[NAMELENGTH];
    int  recval;
} smallrec;

/* successor of character ch */
#define succ(ch) ((char)((int)(ch) + 1))

/*
 * Prototypes for the safe wrapper functions from misc.c
 *
 * These wrappers call AM_* and exit(1) on error.
 */
int        xAM_CreateIndex(char *fname, int indexno, char attrtype, int attrlen);
int        xAM_DestroyIndex(char *fname, int indexno);
int        xAM_InsertEntry(int fd, char attrtype, int attrlen, char *val, RecIdType recid);
int        xAM_DeleteEntry(int fd, char attrtype, int attrlen, char *val, RecIdType recid);
int        xAM_OpenIndexScan(int fd, char attrtype, int attrlen, int op, char *value);
RecIdType  xAM_FindNextEntry(int sd);
int        xAM_CloseIndexScan(int sd);

int        xPF_OpenFile(char *fname);
int        xPF_CloseFile(int fd);

/*
 * padstring() from misc.c
 */
void padstring(char *str, int length);

#endif /* TESTAM_H */
