#include <stdio.h>
#include <string.h>
#include "pf.h"
#include "am.h"

/* Creates a secondary index file called fileName.indexNo */
int AM_CreateIndex(char *fileName, int indexNo, char attrType, int attrLength)
{
    char *pageBuf;
    char indexfName[AM_MAX_FNAME_LENGTH];
    int pageNum;
    int fileDesc;
    int errVal;
    int maxKeys;
    AM_LEAFHEADER head, *header = &head;

    if ((attrType != 'c') && (attrType != 'f') && (attrType != 'i')) {
        AM_Errno = AME_INVALIDATTRTYPE;
        return AME_INVALIDATTRTYPE;
    }
    if ((attrLength < 1) || (attrLength > 255)) {
        AM_Errno = AME_INVALIDATTRLENGTH;
        return AME_INVALIDATTRLENGTH;
    }
    if (attrLength != 4)
        if (attrType != 'c') {
            AM_Errno = AME_INVALIDATTRLENGTH;
            return AME_INVALIDATTRLENGTH;
        }

    sprintf(indexfName, "%s.%d", fileName, indexNo);
    errVal = PF_CreateFile(indexfName);
    AM_Check;

    fileDesc = PF_OpenFile(indexfName);
    if (fileDesc < 0) {
        AM_Errno = AME_PF;
        return AME_PF;
    }

    errVal = PF_AllocPage(fileDesc, &pageNum, &pageBuf);
    AM_Check;

    header->pageType = 'l';
    header->nextLeafPage = AM_NULL_PAGE;
    header->recIdPtr = PF_PAGE_SIZE;
    header->keyPtr = AM_sl;
    header->freeListPtr = AM_NULL;
    header->numinfreeList = 0;
    header->attrLength = (short)attrLength;
    header->numKeys = 0;

    maxKeys = (PF_PAGE_SIZE - AM_sint - AM_si) / (AM_si + attrLength);
    if ((maxKeys % 2) != 0) header->maxKeys = (short)(maxKeys - 1);
    else header->maxKeys = (short)maxKeys;

    bcopy((char *)header, pageBuf, AM_sl);

    errVal = PF_UnfixPage(fileDesc, pageNum, TRUE);
    AM_Check;

    errVal = PF_CloseFile(fileDesc);
    AM_Check;

    AM_RootPageNum = pageNum;
    return AME_OK;
}

/* Destroys the index fileName.indexNo */
int AM_DestroyIndex(char *fileName, int indexNo)
{
    char indexfName[AM_MAX_FNAME_LENGTH];
    int errVal;

    sprintf(indexfName, "%s.%d", fileName, indexNo);
    errVal = PF_DestroyFile(indexfName);
    AM_Check;
    return AME_OK;
}

/* Deletes the recId from the list for value and deletes value if list becomes empty */
int AM_DeleteEntry(int fileDesc, char attrType, int attrLength, char *value, int recId)
{
    char *pageBuf;
    int pageNum;
    int index;
    int status;
    short nextRec;
    short oldhead;
    short temp;
    char *currRecPtr;
    AM_LEAFHEADER head, *header = &head;
    int recSize;
    int tempRec;
    int errVal;
    int i;

    if ((attrType != 'c') && (attrType != 'f') && (attrType != 'i')) {
        AM_Errno = AME_INVALIDATTRTYPE;
        return AME_INVALIDATTRTYPE;
    }
    if (value == NULL) {
        AM_Errno = AME_INVALIDVALUE;
        return AME_INVALIDVALUE;
    }
    if (fileDesc < 0) {
        AM_Errno = AME_FD;
        return AME_FD;
    }

    status = AM_Search(fileDesc, attrType, attrLength, value, &pageNum, &pageBuf, &index);
    if (status < 0) {
        AM_Errno = status;
        return status;
    }
    if (status == AM_NOT_FOUND) {
        AM_Errno = AME_NOTFOUND;
        return AME_NOTFOUND;
    }

    bcopy(pageBuf, (char *)header, AM_sl);
    recSize = attrLength + AM_ss;
    currRecPtr = pageBuf + AM_sl + (index - 1) * recSize + attrLength;
    bcopy(currRecPtr, (char *)&nextRec, AM_ss);

    while (nextRec != 0) {
        bcopy(pageBuf + nextRec, (char *)&tempRec, AM_si);
        if (recId == tempRec) {
            bcopy(pageBuf + nextRec + AM_si, currRecPtr, AM_ss);
            header->numinfreeList++;
            oldhead = header->freeListPtr;
            header->freeListPtr = nextRec;
            bcopy((char *)&oldhead, pageBuf + nextRec + AM_si, AM_ss);
            break;
        } else {
            currRecPtr = pageBuf + nextRec + AM_si;
            bcopy(currRecPtr, (char *)&nextRec, AM_ss);
        }
    }

    if (nextRec == AM_NULL) {
        AM_Errno = AME_NOTFOUND;
        return AME_NOTFOUND;
    }

    bcopy(pageBuf + AM_sl + (index - 1) * recSize + attrLength, (char *)&temp, AM_ss);
    if (temp == 0) {
        for (i = index; i < (header->numKeys); i++)
            bcopy(pageBuf + AM_sl + i * recSize, pageBuf + AM_sl + (i - 1) * recSize, recSize);
        header->numKeys--;
        header->keyPtr = header->keyPtr - (short)recSize;
    }

    bcopy((char *)header, pageBuf, AM_sl);

    errVal = PF_UnfixPage(fileDesc, pageNum, TRUE);
    (void)errVal; /* PF macro used elsewhere */

    AM_EmptyStack();
    AM_Errno = AME_OK;
    return AME_OK;
}

/* Inserts a value,recId pair into the tree */
int AM_InsertEntry(int fileDesc, char attrType, int attrLength, char *value, int recId)
{
    char *pageBuf;
    int pageNum;
    int index;
    int status;
    int inserted;
    int addtoparent;
    int errVal;
    char key[AM_MAXATTRLENGTH];

    if ((attrType != 'c') && (attrType != 'f') && (attrType != 'i')) {
        AM_Errno = AME_INVALIDATTRTYPE;
        return AME_INVALIDATTRTYPE;
    }
    if (value == NULL) {
        AM_Errno = AME_INVALIDVALUE;
        return AME_INVALIDVALUE;
    }
    if (fileDesc < 0) {
        AM_Errno = AME_FD;
        return AME_FD;
    }

    status = AM_Search(fileDesc, attrType, attrLength, value, &pageNum, &pageBuf, &index);
    if (status < 0) {
        AM_EmptyStack();
        AM_Errno = status;
        return status;
    }

    inserted = AM_InsertintoLeaf(pageBuf, attrLength, value, recId, index, status);

    if (inserted == TRUE) {
        errVal = PF_UnfixPage(fileDesc, pageNum, TRUE);
        AM_Check;
        AM_EmptyStack();
        return AME_OK;
    }

    if (inserted < 0) {
        AM_EmptyStack();
        AM_Errno = inserted;
        return inserted;
    }

    if (inserted == FALSE) {
        addtoparent = AM_SplitLeaf(fileDesc, pageBuf, &pageNum, attrLength, recId, value, status, index, key);
        if (addtoparent < 0) {
            AM_EmptyStack();
            AM_Errno = addtoparent;
            return addtoparent;
        }

        if (addtoparent == TRUE) {
            errVal = AM_AddtoParent(fileDesc, pageNum, key, attrLength);
            if (errVal < 0) {
                AM_EmptyStack();
                AM_Errno = errVal;
                return errVal;
            }
        }
    }

    AM_EmptyStack();
    return AME_OK;
}
