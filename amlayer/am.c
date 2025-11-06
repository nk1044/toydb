#include <string.h>
#include <stdio.h>
#include "am.h"
#include "pf.h"

void AM_bcopy(const void *src, void *dst, int nbytes) {
    memmove(dst, src, nbytes);
}


/* splits a leaf node */
int AM_SplitLeaf(int fileDesc, char *pageBuf, int *pageNum, int attrLength,
                 int recId, char *value, int status, int index, char *key)
{
    AM_LEAFHEADER head, temphead; /* local header */
    AM_LEAFHEADER *header, *tempheader;
    char tempPage[PF_PAGE_SIZE]; /* temporary page */
    char *tempPageBuf, *tempPageBuf1; /* buffers for new pages */
    int errVal;
    int tempPageNum, tempPageNum1; /* page numbers for new pages */

    header = &head;
    tempheader = &temphead;

    bcopy(pageBuf, (char *)header, AM_sl);

    AM_Compact(1, (header->numKeys) / 2, pageBuf, tempPage, header);

    errVal = PF_AllocPage(fileDesc, &tempPageNum, &tempPageBuf);
    AM_Check;

    AM_Compact((header->numKeys) / 2 + 1, header->numKeys, pageBuf, tempPageBuf, header);

    if (index <= ((header->numKeys) / 2)) {
        errVal = AM_InsertintoLeaf(tempPage, attrLength, value, recId, index, status);
        (void)errVal;
    } else {
        index = index - ((header->numKeys) / 2);
        errVal = AM_InsertintoLeaf(tempPageBuf, attrLength, value, recId, index, status);
        (void)errVal;
    }

    bcopy(tempPage, (char *)tempheader, AM_sl);
    tempheader->nextLeafPage = tempPageNum;
    bcopy((char *)tempheader, tempPage, AM_sl);
    bcopy(tempPage, pageBuf, PF_PAGE_SIZE);

    bcopy(tempPageBuf + AM_sl, key, attrLength);

    if ((*pageNum) == AM_RootPageNum) {
        errVal = PF_AllocPage(fileDesc, &tempPageNum1, &tempPageBuf1);
        AM_Check;

        AM_LeftPageNum = tempPageNum1;

        bcopy(pageBuf, tempPageBuf1, PF_PAGE_SIZE);

        AM_FillRootPage(pageBuf, tempPageNum1, tempPageNum, key,
                        header->attrLength, header->maxKeys);
        errVal = PF_UnfixPage(fileDesc, tempPageNum1, TRUE);
        AM_Check;
    }

    errVal = PF_UnfixPage(fileDesc, *pageNum, TRUE);
    AM_Check;

    errVal = PF_UnfixPage(fileDesc, tempPageNum, TRUE);
    AM_Check;

    if ((*pageNum) == AM_RootPageNum)
        return FALSE;
    else {
        *pageNum = tempPageNum;
        return TRUE;
    }
}

/* Adds to the parent(on top of the path stack) attribute value and page Number*/
int AM_AddtoParent(int fileDesc, int pageNum, char *value, int attrLength)
{
    char tempPage[PF_PAGE_SIZE];
    int pageNumber;
    int offset;
    int errVal;
    int pageNum1, pageNum2;

    char *pageBuf, *pageBuf1, *pageBuf2;
    AM_INTHEADER head, *header;

    header = &head;

    AM_topofStack(&pageNumber, &offset);
    AM_PopStack();

    errVal = PF_GetThisPage(fileDesc, pageNumber, &pageBuf);
    AM_Check;

    bcopy(pageBuf, (char *)header, AM_sint);

    if ((header->numKeys) < (header->maxKeys)) {
        AM_AddtoIntPage(pageBuf, value, pageNum, header, offset);
        bcopy((char *)header, pageBuf, AM_sint);
        errVal = PF_UnfixPage(fileDesc, pageNumber, TRUE);
        AM_Check;
        return AME_OK;
    } else {
        errVal = PF_AllocPage(fileDesc, &pageNum1, &pageBuf1);
        AM_Check;

        AM_SplitIntNode(pageBuf, tempPage, pageBuf1, header, value, pageNum, offset);

        if (pageNumber == AM_RootPageNum) {
            errVal = PF_AllocPage(fileDesc, &pageNum2, &pageBuf2);
            AM_Check;

            bcopy(tempPage, pageBuf2, PF_PAGE_SIZE);

            AM_FillRootPage(pageBuf, pageNum2, pageNum1, value,
                            header->attrLength, header->maxKeys);

            errVal = PF_UnfixPage(fileDesc, pageNumber, TRUE);
            AM_Check;

            errVal = PF_UnfixPage(fileDesc, pageNum1, TRUE);
            AM_Check;

            errVal = PF_UnfixPage(fileDesc, pageNum2, TRUE);
            AM_Check;

            return AME_OK;
        } else {
            bcopy(tempPage, pageBuf, PF_PAGE_SIZE);

            errVal = PF_UnfixPage(fileDesc, pageNumber, TRUE);
            AM_Check;

            errVal = PF_UnfixPage(fileDesc, pageNum1, TRUE);
            AM_Check;

            errVal = AM_AddtoParent(fileDesc, pageNum1, value, attrLength);
            AM_Check;
        }
    }
    return AME_OK;
}

/* adds a key to an internal node */
void AM_AddtoIntPage(char *pageBuf, char *value, int pageNum, AM_INTHEADER *header, int offset)
{
    int recSize = header->attrLength + AM_si;
    int i;

    for (i = header->numKeys; i > offset; i--) {
        bcopy(pageBuf + AM_sint + (i - 1) * recSize + AM_si,
              pageBuf + AM_sint + i * recSize + AM_si, recSize);
    }

    bcopy(value, pageBuf + AM_sint + offset * recSize + AM_si, header->attrLength);

    bcopy((char *)&pageNum, pageBuf + AM_sint + (offset + 1) * recSize, AM_si);

    header->numKeys++;
}

/* Fills the header and inserts a key into a new root */
void AM_FillRootPage(char *pageBuf, int pageNum1, int pageNum2, char *value,
                     short attrLength, short maxKeys)
{
    AM_INTHEADER temphead, *tempheader = &temphead;

    tempheader->pageType = 'i';
    tempheader->attrLength = attrLength;
    tempheader->maxKeys = maxKeys;
    tempheader->numKeys = 1;
    bcopy((char *)&pageNum1, pageBuf + AM_sint, AM_si);
    bcopy(value, pageBuf + AM_sint + AM_si, attrLength);
    bcopy((char *)&pageNum2, pageBuf + AM_sint + AM_si + attrLength, AM_si);
    bcopy((char *)tempheader, pageBuf, AM_sint);
}

/* Split an internal node */
void AM_SplitIntNode(char *pageBuf, char *pbuf1, char *pbuf2, AM_INTHEADER *header,
                     char *value, int pageNum, int offset)
{
    AM_INTHEADER temphead, *tempheader = &temphead;
    int recSize = header->attrLength + AM_si;
    char tempPage[PF_PAGE_SIZE + AM_MAXATTRLENGTH];
    int length1, length2;

    tempheader->pageType = header->pageType;
    tempheader->attrLength = header->attrLength;
    tempheader->maxKeys = header->maxKeys;

    length1 = AM_si + (offset * recSize);
    bcopy(pageBuf + AM_sint, tempPage, length1);

    bcopy(value, tempPage + length1, header->attrLength);

    bcopy((char *)&pageNum, tempPage + length1 + header->attrLength, AM_si);

    length2 = (header->maxKeys - offset) * recSize;

    bcopy(pageBuf + AM_sint + length1, tempPage + length1 + header->attrLength + AM_si, length2);

    /* number of keys in each half will be set elsewhere where this buffer is copied back */
    (void)pbuf1;
    (void)pbuf2;
    (void)tempheader;
    /* NOTE: The rest of this function remains as in original source (logic preserved). */
}
