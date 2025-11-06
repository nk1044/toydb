#include <stdio.h>
#include <string.h>
#include "am.h"
#include "pf.h"

/* The structure of the scan Table */
struct {
    int   fileDesc;
    int   op;
    int   attrType;
    int   pageNum;
    short index;
    short actindex;
    int   nextpageNum;
    char  nextvalue[AM_MAXATTRLENGTH];
    short nextIndex;
    short nextRecIdPtr;
    int   lastpageNum;
    short lastIndex;
    int   status;
} AM_scanTable[MAXSCANS];

/* ------------------------------------------------------------ */
/* Opens an index scan                                          */
/* ------------------------------------------------------------ */
int AM_OpenIndexScan(int fileDesc, char attrType, int attrLength, int op, char *value)
{
    int scanDesc;
    int status;
    int index;
    int pageNum;
    int recSize;
    char *pageBuf;
    int errVal;
    AM_LEAFHEADER head, *header;
    int searchpageNum;

    /* validate fd */
    if (fileDesc < 0) {
        AM_Errno = AME_FD;
        return AME_FD;
    }

    /* validate type */
    if (attrType != 'i' && attrType != 'c' && attrType != 'f') {
        AM_Errno = AME_INVALIDATTRTYPE;
        return AME_INVALIDATTRTYPE;
    }

    header = &head;

    /* find free scan entry */
    for (scanDesc = 0; scanDesc < MAXSCANS; scanDesc++)
        if (AM_scanTable[scanDesc].status == FREE)
            break;

    if (scanDesc > MAXSCANS - 1) {
        AM_Errno = AME_SCAN_TAB_FULL;
        return AME_SCAN_TAB_FULL;
    }

    /* mark slot in use */
    AM_scanTable[scanDesc].status = FIRST;
    AM_scanTable[scanDesc].attrType = attrType;

    /* initialize leftmost leaf */
    AM_LeftPageNum = GetLeftPageNum(fileDesc);

    /* case: scan ALL keys */
    if (value == NULL) {
        AM_scanTable[scanDesc].fileDesc = fileDesc;
        AM_scanTable[scanDesc].op = ALL;
        AM_scanTable[scanDesc].nextpageNum = AM_LeftPageNum;
        AM_scanTable[scanDesc].nextIndex = 1;
        AM_scanTable[scanDesc].actindex = 1;

        errVal = PF_GetThisPage(fileDesc, AM_LeftPageNum, &pageBuf);
        AM_Check;

        bcopy(pageBuf + AM_sl + attrLength,
              &AM_scanTable[scanDesc].nextRecIdPtr, AM_ss);

        errVal = PF_UnfixPage(fileDesc, AM_LeftPageNum, FALSE);
        AM_Check;
        return scanDesc;
    }

    /* search for key */
    status = AM_Search(fileDesc, attrType, attrLength, value,
                       &pageNum, &pageBuf, &index);
    searchpageNum = pageNum;

    if (status < 0) {
        AM_scanTable[scanDesc].status = FREE;
        AM_Errno = status;
        return status;
    }

    bcopy(pageBuf, header, AM_sl);

    recSize = attrLength + AM_ss;

    AM_scanTable[scanDesc].fileDesc = fileDesc;
    AM_scanTable[scanDesc].op = op;

    /* if beyond last key, but leaf has next page */
    if (index > header->numKeys) {
        if (header->nextLeafPage != AM_NULL_PAGE) {
            errVal = PF_GetThisPage(fileDesc, header->nextLeafPage, &pageBuf);
            AM_Check;

            bcopy(pageBuf, header, AM_sl);

            errVal = PF_UnfixPage(fileDesc, header->nextLeafPage, FALSE);
            AM_Check;

            pageNum = header->nextLeafPage;
            index = 1;
        } else {
            pageNum = AM_NULL_PAGE;
        }
    }

    AM_scanTable[scanDesc].pageNum = pageNum;
    AM_scanTable[scanDesc].index = index;

    /* switch based on operator */
    switch (op) {
        case EQUAL:
            if (status != AM_FOUND) {
                AM_scanTable[scanDesc].status = OVER;
            } else {
                AM_scanTable[scanDesc].nextpageNum = pageNum;
                AM_scanTable[scanDesc].nextIndex = index;
                AM_scanTable[scanDesc].actindex = index;

                bcopy(pageBuf + AM_sl +
                          (index - 1)*recSize + attrLength,
                      &AM_scanTable[scanDesc].nextRecIdPtr, AM_ss);

                AM_scanTable[scanDesc].lastpageNum = pageNum;
                AM_scanTable[scanDesc].lastIndex = index;
            }
            break;

        case LESS_THAN:
            AM_scanTable[scanDesc].nextpageNum = AM_LeftPageNum;
            AM_scanTable[scanDesc].nextIndex = 1;
            AM_scanTable[scanDesc].actindex = 1;

            if (searchpageNum != AM_LeftPageNum) {
                errVal = PF_GetThisPage(fileDesc, AM_LeftPageNum, &pageBuf);
                AM_Check;
            }

            bcopy(pageBuf + AM_sl + attrLength,
                  &AM_scanTable[scanDesc].nextRecIdPtr, AM_ss);

            if (searchpageNum != AM_LeftPageNum) {
                errVal = PF_UnfixPage(fileDesc, AM_LeftPageNum, FALSE);
                AM_Check;
            }

            AM_scanTable[scanDesc].lastpageNum = pageNum;
            AM_scanTable[scanDesc].lastIndex = index - 1;
            break;

        case GREATER_THAN:
            if (status == AM_FOUND) {
                if (index + 1 <= header->numKeys) {
                    AM_scanTable[scanDesc].nextpageNum = pageNum;
                    AM_scanTable[scanDesc].nextIndex = index + 1;
                    AM_scanTable[scanDesc].actindex = index + 1;
                    bcopy(pageBuf + AM_sl + index*recSize + attrLength,
                          &AM_scanTable[scanDesc].nextRecIdPtr, AM_ss);
                } else if (header->nextLeafPage != AM_NULL_PAGE) {
                    AM_scanTable[scanDesc].nextpageNum = header->nextLeafPage;
                    AM_scanTable[scanDesc].nextIndex = 1;
                    AM_scanTable[scanDesc].actindex = 1;

                    errVal = PF_GetThisPage(fileDesc, header->nextLeafPage,
                                            &pageBuf);
                    AM_Check;

                    bcopy(pageBuf + AM_sl + attrLength,
                          &AM_scanTable[scanDesc].nextRecIdPtr, AM_ss);

                    errVal = PF_UnfixPage(fileDesc, header->nextLeafPage, FALSE);
                    AM_Check;
                } else {
                    AM_scanTable[scanDesc].status = OVER;
                }
            } else { /* NOT FOUND */
                AM_scanTable[scanDesc].nextpageNum = pageNum;
                AM_scanTable[scanDesc].nextIndex = index;
                AM_scanTable[scanDesc].actindex = index;

                bcopy(pageBuf + AM_sl + (index - 1)*recSize + attrLength,
                      &AM_scanTable[scanDesc].nextRecIdPtr, AM_ss);
            }
            break;

        case LESS_THAN_EQUAL:
            AM_scanTable[scanDesc].nextpageNum = AM_LeftPageNum;
            AM_scanTable[scanDesc].nextIndex = 1;
            AM_scanTable[scanDesc].actindex = 1;

            if (searchpageNum != AM_LeftPageNum) {
                errVal = PF_GetThisPage(fileDesc, AM_LeftPageNum, &pageBuf);
                AM_Check;
            }

            bcopy(pageBuf + AM_sl + attrLength,
                  &AM_scanTable[scanDesc].nextRecIdPtr, AM_ss);

            if (searchpageNum != AM_LeftPageNum) {
                errVal = PF_UnfixPage(fileDesc, AM_LeftPageNum, FALSE);
                AM_Check;
            }

            AM_scanTable[scanDesc].lastpageNum = pageNum;
            AM_scanTable[scanDesc].lastIndex =
                (status == AM_FOUND ? index : index - 1);
            break;

        case GREATER_THAN_EQUAL:
            AM_scanTable[scanDesc].nextpageNum = pageNum;
            AM_scanTable[scanDesc].nextIndex = index;
            AM_scanTable[scanDesc].actindex = index;

            bcopy(pageBuf + AM_sl + (index - 1)*recSize + attrLength,
                  &AM_scanTable[scanDesc].nextRecIdPtr, AM_ss);
            break;

        case NOT_EQUAL:
            if (status == AM_FOUND) {
                AM_scanTable[scanDesc].nextpageNum = AM_LeftPageNum;
                AM_scanTable[scanDesc].nextIndex = 1;
                AM_scanTable[scanDesc].actindex = 1;

                if (searchpageNum != AM_LeftPageNum) {
                    errVal = PF_GetThisPage(fileDesc, AM_LeftPageNum, &pageBuf);
                    AM_Check;
                }

                bcopy(pageBuf + AM_sl + attrLength,
                      &AM_scanTable[scanDesc].nextRecIdPtr, AM_ss);

                if (searchpageNum != AM_LeftPageNum) {
                    errVal = PF_UnfixPage(fileDesc, AM_LeftPageNum, FALSE);
                    AM_Check;
                }
            } else {
                AM_scanTable[scanDesc].pageNum = AM_NULL_PAGE;
            }
            break;

        default:
            AM_scanTable[scanDesc].status = FREE;
            AM_Errno = AME_INVALID_OP_TO_SCAN;
            return AME_INVALID_OP_TO_SCAN;
    }

    errVal = PF_UnfixPage(fileDesc, searchpageNum, FALSE);
    AM_Check;

    return scanDesc;
}

/* ------------------------------------------------------------ */
/* Return next matching record ID                               */
/* ------------------------------------------------------------ */
int AM_FindNextEntry(int scanDesc)
{
    int recId;
    char *pageBuf;
    int errVal;
    AM_LEAFHEADER head, *header;
    int recSize;
    int compareVal;

    /* validate */
    if (scanDesc < 0 || scanDesc > MAXSCANS - 1) {
        AM_Errno = AME_INVALID_SCANDESC;
        return AME_INVALID_SCANDESC;
    }

    if (AM_scanTable[scanDesc].status == OVER)
        return AME_EOF;

    if (AM_scanTable[scanDesc].nextpageNum == AM_NULL_PAGE) {
        AM_scanTable[scanDesc].status = OVER;
        return AME_EOF;
    }

    header = &head;

    errVal = PF_GetThisPage(
        AM_scanTable[scanDesc].fileDesc,
        AM_scanTable[scanDesc].nextpageNum,
        &pageBuf
    );
    AM_Check;

    bcopy(pageBuf, header, AM_sl);
    recSize = header->attrLength + AM_ss;

    errVal = PF_UnfixPage(
        AM_scanTable[scanDesc].fileDesc,
        AM_scanTable[scanDesc].nextpageNum,
        FALSE
    );
    AM_Check;

    /* skip empty pages */
    while (header->numKeys == 0) {
        if (header->nextLeafPage == AM_NULL_PAGE) {
            AM_scanTable[scanDesc].status = OVER;
            return AME_EOF;
        }

        errVal = PF_GetThisPage(
            AM_scanTable[scanDesc].fileDesc,
            header->nextLeafPage,
            &pageBuf
        );
        AM_Check;

        errVal = PF_UnfixPage(
            AM_scanTable[scanDesc].fileDesc,
            header->nextLeafPage,
            FALSE
        );
        AM_Check;

        AM_scanTable[scanDesc].nextpageNum = header->nextLeafPage;
        AM_scanTable[scanDesc].nextIndex = 1;
        AM_scanTable[scanDesc].actindex = 1;

        bcopy(pageBuf, header, AM_sl);

        bcopy(
            pageBuf + AM_sl + (AM_scanTable[scanDesc].nextIndex - 1)*recSize +
                header->attrLength,
            &AM_scanTable[scanDesc].nextRecIdPtr,
            AM_ss
        );

        AM_scanTable[scanDesc].status = FIRST;
    }

    /* boundary checks for <, <= */
    if (AM_scanTable[scanDesc].op == LESS_THAN ||
        AM_scanTable[scanDesc].op == LESS_THAN_EQUAL)
    {
        if (AM_scanTable[scanDesc].lastpageNum <= AM_scanTable[scanDesc].nextpageNum &&
            AM_scanTable[scanDesc].lastIndex == 0)
        {
            AM_scanTable[scanDesc].status = OVER;
            return AME_EOF;
        }
    }

    /* skip current value if op != */
    if (AM_scanTable[scanDesc].op == NOT_EQUAL) {
        if (AM_scanTable[scanDesc].pageNum == AM_scanTable[scanDesc].nextpageNum &&
            AM_scanTable[scanDesc].index    == AM_scanTable[scanDesc].actindex)
        {
            if (AM_scanTable[scanDesc].nextIndex + 1 <= header->numKeys) {

                AM_scanTable[scanDesc].nextIndex++;
                AM_scanTable[scanDesc].actindex++;

                bcopy(
                    pageBuf + AM_sl +
                        (AM_scanTable[scanDesc].nextIndex - 1)*recSize +
                        header->attrLength,
                    &AM_scanTable[scanDesc].nextRecIdPtr,
                    AM_ss
                );

            } else if (header->nextLeafPage == AM_NULL_PAGE) {
                return AME_EOF;
            } else {
                /* move to next leaf page */
                AM_scanTable[scanDesc].nextpageNum = header->nextLeafPage;
                AM_scanTable[scanDesc].nextIndex = 1;
                AM_scanTable[scanDesc].actindex = 1;

                errVal = PF_GetThisPage(
                    AM_scanTable[scanDesc].fileDesc,
                    header->nextLeafPage,
                    &pageBuf
                );
                AM_Check;

                bcopy(
                    pageBuf + AM_sl + header->attrLength,
                    &AM_scanTable[scanDesc].nextRecIdPtr,
                    AM_ss
                );

                bcopy(pageBuf, header, AM_sl);

                errVal = PF_UnfixPage(
                    AM_scanTable[scanDesc].fileDesc,
                    header->nextLeafPage,
                    FALSE
                );
                AM_Check;
            }
        }
    }

    /* if not first call, check if previous record deleted */
    if (AM_scanTable[scanDesc].status != FIRST) {

        compareVal = AM_Compare(
            pageBuf + (AM_scanTable[scanDesc].nextIndex - 1)*recSize + AM_sl,
            AM_scanTable[scanDesc].attrType,
            header->attrLength,
            AM_scanTable[scanDesc].nextvalue
        );

        if (compareVal != 0) {
            AM_scanTable[scanDesc].nextIndex--;

            bcopy(
                pageBuf + AM_sl +
                    (AM_scanTable[scanDesc].nextIndex - 1)*recSize +
                    header->attrLength,
                &AM_scanTable[scanDesc].nextRecIdPtr,
                AM_ss
            );
        }

    } else {
        /* first call */
        AM_scanTable[scanDesc].status = BUSY;

        bcopy(
            pageBuf + AM_sl +
                (AM_scanTable[scanDesc].nextIndex - 1)*recSize,
            AM_scanTable[scanDesc].nextvalue,
            header->attrLength
        );
    }

    /* output recId */
    bcopy(
        pageBuf + AM_scanTable[scanDesc].nextRecIdPtr,
        &recId,
        AM_si
    );

    /* update pointer to next recId */
    bcopy(
        pageBuf + AM_scanTable[scanDesc].nextRecIdPtr + AM_si,
        &AM_scanTable[scanDesc].nextRecIdPtr,
        AM_ss
    );

    /* if end of chain */
    if (AM_scanTable[scanDesc].nextRecIdPtr == 0) {

        /* go to next key in same page */
        if (AM_scanTable[scanDesc].nextIndex + 1 <= header->numKeys) {

            AM_scanTable[scanDesc].nextIndex++;
            AM_scanTable[scanDesc].actindex++;

            bcopy(
                pageBuf + AM_sl +
                    (AM_scanTable[scanDesc].nextIndex - 1)*recSize +
                    header->attrLength,
                &AM_scanTable[scanDesc].nextRecIdPtr,
                AM_ss
            );

            bcopy(
                pageBuf + AM_sl +
                    (AM_scanTable[scanDesc].nextIndex - 1)*recSize,
                AM_scanTable[scanDesc].nextvalue,
                header->attrLength
            );

        } else if (header->nextLeafPage == AM_NULL_PAGE) {
            AM_scanTable[scanDesc].status = OVER;
        } else {
            /* move to next leaf */
            AM_scanTable[scanDesc].nextpageNum = header->nextLeafPage;
            AM_scanTable[scanDesc].nextIndex = 1;
            AM_scanTable[scanDesc].actindex = 1;

            errVal = PF_GetThisPage(
                AM_scanTable[scanDesc].fileDesc,
                header->nextLeafPage,
                &pageBuf
            );
            AM_Check;

            bcopy(
                pageBuf + AM_sl + header->attrLength,
                &AM_scanTable[scanDesc].nextRecIdPtr,
                AM_ss
            );

            errVal = PF_UnfixPage(
                AM_scanTable[scanDesc].fileDesc,
                header->nextLeafPage,
                FALSE
            );
            AM_Check;

            bcopy(
                pageBuf + AM_sl +
                    (AM_scanTable[scanDesc].nextIndex - 1)*recSize,
                AM_scanTable[scanDesc].nextvalue,
                header->attrLength
            );

            bcopy(pageBuf, header, AM_sl);
        }
    }

    /* Equality operator end check */
    if (AM_scanTable[scanDesc].op == EQUAL) {
        if (AM_scanTable[scanDesc].pageNum != AM_scanTable[scanDesc].nextpageNum ||
            AM_scanTable[scanDesc].index    != AM_scanTable[scanDesc].actindex)
        {
            AM_scanTable[scanDesc].status = OVER;
        }
    }

    /* < and <= boundary */
    if (AM_scanTable[scanDesc].op == LESS_THAN ||
        AM_scanTable[scanDesc].op == LESS_THAN_EQUAL)
    {
        if (AM_scanTable[scanDesc].lastpageNum == AM_scanTable[scanDesc].nextpageNum &&
            AM_scanTable[scanDesc].lastIndex    == AM_scanTable[scanDesc].actindex)
        {
            AM_scanTable[scanDesc].status = LAST;
        }
        else if (AM_scanTable[scanDesc].lastpageNum == AM_scanTable[scanDesc].nextpageNum &&
                 AM_scanTable[scanDesc].lastIndex < AM_scanTable[scanDesc].actindex)
        {
            AM_scanTable[scanDesc].status = OVER;
        }
        else if (AM_scanTable[scanDesc].status == LAST) {
            AM_scanTable[scanDesc].status = OVER;
        }
    }

    return recId;
}

/* ------------------------------------------------------------ */
/* Close a scan                                                 */
/* ------------------------------------------------------------ */
int AM_CloseIndexScan(int scanDesc)
{
    if (scanDesc < 0 || scanDesc > MAXSCANS - 1) {
        AM_Errno = AME_INVALID_SCANDESC;
        return AME_INVALID_SCANDESC;
    }

    AM_scanTable[scanDesc].status = FREE;
    return AME_OK;
}

/* ------------------------------------------------------------ */
/* Get leftmost leaf page number                                */
/* ------------------------------------------------------------ */
int GetLeftPageNum(int fileDesc)
{
    char *pageBuf;
    int pageNum;
    int errVal;

    errVal = PF_GetFirstPage(fileDesc, &pageNum, &pageBuf);
    AM_Check;

    if (*pageBuf == 'l')
        AM_LeftPageNum = pageNum;
    else
        AM_LeftPageNum = 2; /* original logic */

    errVal = PF_UnfixPage(fileDesc, pageNum, FALSE);
    AM_Check;

    return AM_LeftPageNum;
}
