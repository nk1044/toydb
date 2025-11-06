#include <stdio.h>
#include <string.h>
#include "am.h"
#include "pf.h"

/* Inserts a key into a leaf node */
int AM_InsertintoLeaf(char *pageBuf, int attrLength, char *value, int recId, int index, int status)
{
    int recSize;
    char tempPage[PF_PAGE_SIZE];
    AM_LEAFHEADER head, *header;
    int errVal;

    header = &head;
    bcopy(pageBuf, (char *)header, AM_sl);

    recSize = attrLength + AM_ss;
    if (status == AM_FOUND) {
        if (header->freeListPtr == 0)
            if ((header->recIdPtr - header->keyPtr) < (AM_si + AM_ss)) {
                return FALSE;
            }
        AM_InsertToLeafFound(pageBuf, recId, index, header);
        bcopy((char *)header, pageBuf, AM_sl);
        return TRUE;
    }

    if ((header->freeListPtr) == 0) {
        if ((header->recIdPtr - header->keyPtr) < (AM_si + AM_ss + recSize))
            return FALSE;
        else {
            AM_InsertToLeafNotFound(pageBuf, value, recId, index, header);
            header->numKeys++;
            bcopy((char *)header, pageBuf, AM_sl);
            return TRUE;
        }
    } else if ((header->recIdPtr - header->keyPtr) > recSize) {
        AM_InsertToLeafNotFound(pageBuf, value, recId, index, header);
        header->numKeys++;
        bcopy((char *)header, pageBuf, AM_sl);
        return TRUE;
    } else if (((header->numinfreeList) * (AM_si + AM_ss) + header->recIdPtr - header->keyPtr)
               > (recSize + AM_si + AM_ss)) {
        AM_Compact(1, header->numKeys, pageBuf, tempPage, header);

        bcopy(tempPage, pageBuf, PF_PAGE_SIZE);
        bcopy(pageBuf, (char *)header, AM_sl);

        AM_InsertToLeafNotFound(pageBuf, value, recId, index, header);
        header->numKeys++;
        bcopy((char *)header, pageBuf, AM_sl);
        return TRUE;
    } else
        return FALSE;
}

/* Insert into leaf given the fact that the key is old */
void AM_InsertToLeafFound(char *pageBuf, int recId, int index, AM_LEAFHEADER *header)
{
    int recSize;
    short tempPtr;
    short oldhead;

    recSize = header->attrLength + AM_ss;
    if ((header->freeListPtr) == 0) {
        header->recIdPtr = header->recIdPtr - (short)(AM_si + AM_ss);
        tempPtr = header->recIdPtr;
    } else {
        tempPtr = header->freeListPtr;
        header->numinfreeList--;
        bcopy(pageBuf + tempPtr + AM_si, (char *)&(header->freeListPtr), AM_ss);
    }

    bcopy(pageBuf + AM_sl + (index - 1) * recSize + header->attrLength, (char *)&oldhead, AM_ss);

    bcopy((char *)&tempPtr, pageBuf + AM_sl + (index - 1) * recSize + header->attrLength, AM_ss);

    bcopy((char *)&recId, pageBuf + tempPtr, AM_si);

    bcopy((char *)&oldhead, pageBuf + tempPtr + AM_si, AM_ss);
}

/* Insert to a leaf given that the key is new */
void AM_InsertToLeafNotFound(char *pageBuf, char *value, int recId, int index, AM_LEAFHEADER *header)
{
    int recSize;
    short nullv = AM_NULL;
    int i;

    recSize = header->attrLength + AM_ss;
    for (i = header->numKeys; i >= index; i--) {
        bcopy(pageBuf + AM_sl + (i - 1) * recSize, pageBuf + AM_sl + i * recSize, recSize);
    }

    header->keyPtr = header->keyPtr + (short)recSize;

    bcopy(value, pageBuf + AM_sl + (index - 1) * recSize, header->attrLength);

    bcopy((char *)&nullv, pageBuf + AM_sl + (index - 1) * recSize + header->attrLength, AM_ss);

    AM_InsertToLeafFound(pageBuf, recId, index, header);
}

/* Compact recIds so that there is enough space in the middle */
void AM_Compact(int low, int high, char *pageBuf, char *tempPage, AM_LEAFHEADER *header)
{
    short nextRec;
    AM_LEAFHEADER temphead, *tempheader;
    short recIdPtr;
    int recSize;
    int i, j;
    int offset1, offset2;

    tempheader = &temphead;
    bcopy((char *)header, (char *)tempheader, AM_sl);

    recSize = header->attrLength + AM_ss;
    recIdPtr = (short)(PF_PAGE_SIZE - AM_si - AM_ss);

    for (i = low, j = 1; i <= high; i++, j++) {
        offset1 = (i - 1) * recSize + AM_sl;
        offset2 = (j - 1) * recSize + AM_sl;
        bcopy(pageBuf + offset1, tempPage + offset2, header->attrLength);
        bcopy(pageBuf + offset1 + header->attrLength, (char *)&nextRec, AM_ss);
        bcopy((char *)&recIdPtr, tempPage + offset2 + header->attrLength, AM_ss);
        while (nextRec != 0) {
            bcopy(pageBuf + nextRec, tempPage + recIdPtr, AM_si);
            recIdPtr = (short)(recIdPtr - AM_si - AM_ss);
            bcopy((char *)&recIdPtr, tempPage + recIdPtr + 2 * AM_si + AM_ss, AM_ss);
            bcopy(pageBuf + nextRec + AM_si, (char *)&nextRec, AM_ss);
        }
        bcopy((char *)&nextRec, tempPage + recIdPtr + 2 * AM_si + AM_ss, AM_ss);
    }

    tempheader->pageType = header->pageType;
    tempheader->nextLeafPage = header->nextLeafPage;
    tempheader->recIdPtr = recIdPtr + AM_si + AM_ss;
    tempheader->keyPtr = (short)(AM_sl + (high - low + 1) * recSize);
    tempheader->freeListPtr = AM_NULL;
    tempheader->numinfreeList = 0;
    tempheader->attrLength = header->attrLength;
    tempheader->numKeys = (short)(high - low + 1);
    tempheader->maxKeys = header->maxKeys;

    bcopy((char *)tempheader, tempPage, AM_sl);
}
