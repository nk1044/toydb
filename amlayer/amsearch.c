#include <stdio.h>
#include <string.h>
#include "am.h"
#include "pf.h"

/* searches for a key in a B+ tree */
int AM_Search(int fileDesc, char attrType, int attrLength, char *value,
              int *pageNum, char **pageBuf, int *indexPtr)
{
    int errVal;
    int nextPage;
    int retval;
    AM_LEAFHEADER lhead, *lheader = &lhead;
    AM_INTHEADER  ihead, *iheader = &ihead;

    errVal = PF_GetFirstPage(fileDesc, pageNum, pageBuf);
    AM_Check;

    if (**pageBuf == 'l') {
        bcopy(*pageBuf, (char *)lheader, AM_sl);
        if (lheader->attrLength != attrLength)
            return AME_INVALIDATTRLENGTH;
    } else {
        bcopy(*pageBuf, (char *)iheader, AM_sint);
        if (iheader->attrLength != attrLength)
            return AME_INVALIDATTRLENGTH;
    }

    while ((**pageBuf) != 'l') {
        nextPage = AM_BinSearch(*pageBuf, attrType, attrLength, value, indexPtr, iheader);
        AM_PushStack(*pageNum, *indexPtr);

        errVal = PF_UnfixPage(fileDesc, *pageNum, FALSE);
        AM_Check;

        *pageNum = nextPage;

        errVal = PF_GetThisPage(fileDesc, *pageNum, pageBuf);
        AM_Check;

        if (**pageBuf == 'l') {
            bcopy(*pageBuf, (char *)lheader, AM_sl);
            if (lheader->attrLength != attrLength)
                return AME_INVALIDATTRLENGTH;
        } else {
            bcopy(*pageBuf, (char *)iheader, AM_sint);
            if (iheader->attrLength != attrLength)
                return AME_INVALIDATTRLENGTH;
        }
    }

    return AM_SearchLeaf(*pageBuf, attrType, attrLength, value, indexPtr, lheader);
}

/* Finds the child index / page to follow */
int AM_BinSearch(char *pageBuf, char attrType, int attrLength, char *value,
                 int *indexPtr, AM_INTHEADER *header)
{
    int low, high, mid;
    int compareVal;
    int recSize = AM_si + attrLength;
    int pageNum;

    low = 1;
    high = header->numKeys;

    while ((high - low) > 1) {
        mid = (low + high) / 2;
        compareVal = AM_Compare(pageBuf + AM_sint + AM_si + (mid - 1) * recSize,
                                attrType, attrLength, value);
        if (compareVal < 0) high = mid - 1;
        else if (compareVal > 0) low = mid + 1;
        else {
            bcopy(pageBuf + AM_sint + mid * recSize, (char *)&pageNum, AM_si);
            *indexPtr = mid;
            return pageNum;
        }
    }

    if ((high - low) == 0) {
        if (AM_Compare(pageBuf + AM_sint + AM_si + (low - 1) * recSize,
                       attrType, attrLength, value) < 0) {
            bcopy(pageBuf + AM_sint + (low - 1) * recSize, (char *)&pageNum, AM_si);
            *indexPtr = low - 1;
            return pageNum;
        } else {
            bcopy(pageBuf + AM_sint + low * recSize, (char *)&pageNum, AM_si);
            *indexPtr = low;
            return pageNum;
        }
    }

    if ((high - low) == 1) {
        if (AM_Compare(pageBuf + AM_sint + AM_si + (low - 1) * recSize,
                       attrType, attrLength, value) < 0) {
            bcopy(pageBuf + AM_sint + (low - 1) * recSize, (char *)&pageNum, AM_si);
            *indexPtr = low - 1;
            return pageNum;
        } else if (AM_Compare(pageBuf + AM_sint + AM_si + low * recSize,
                              attrType, attrLength, value) < 0) {
            bcopy(pageBuf + AM_sint + low * recSize, (char *)&pageNum, AM_si);
            *indexPtr = low;
            return pageNum;
        } else {
            bcopy(pageBuf + AM_sint + (low + 1) * recSize, (char *)&pageNum, AM_si);
            *indexPtr = low + 1;
            return pageNum;
        }
    }

    return AM_NULL_PAGE; /* unreachable in well-formed nodes */
}

/* search a leaf node for the key */
int AM_SearchLeaf(char *pageBuf, char attrType, int attrLength, char *value,
                  int *indexPtr, AM_LEAFHEADER *header)
{
    int low, high, mid;
    int compareVal;
    int recSize = AM_ss + attrLength;

    low = 1;
    high = header->numKeys;

    if (high == 0) {
        *indexPtr = 1;
        return AM_NOT_FOUND;
    }

    while ((high - low) > 1) {
        mid = (low + high) / 2;
        compareVal = AM_Compare(pageBuf + AM_sl + (mid - 1) * recSize,
                                attrType, attrLength, value);
        if (compareVal < 0) high = mid - 1;
        else if (compareVal > 0) low = mid + 1;
        else {
            *indexPtr = mid;
            return AM_FOUND;
        }
    }

    if ((high - low) == 0) {
        compareVal = AM_Compare(pageBuf + AM_sl + (low - 1) * recSize,
                                attrType, attrLength, value);
        if (compareVal < 0) {
            *indexPtr = low;
            return AM_NOT_FOUND;
        } else if (compareVal > 0) {
            *indexPtr = low + 1;
            return AM_NOT_FOUND;
        } else {
            *indexPtr = low;
            return AM_FOUND;
        }
    }

    if ((high - low) == 1) {
        compareVal = AM_Compare(pageBuf + AM_sl + (low - 1) * recSize,
                                attrType, attrLength, value);
        if (compareVal < 0) {
            *indexPtr = low;
            return AM_NOT_FOUND;
        } else if (compareVal == 0) {
            *indexPtr = low;
            return AM_FOUND;
        } else {
            compareVal = AM_Compare(pageBuf + AM_sl + low * recSize,
                                    attrType, attrLength, value);
            if (compareVal < 0) {
                *indexPtr = low + 1;
                return AM_NOT_FOUND;
            } else if (compareVal > 0) {
                *indexPtr = low + 2;
                return AM_NOT_FOUND;
            } else {
                *indexPtr = low + 1;
                return AM_FOUND;
            }
        }
    }

    return AM_NOT_FOUND;
}

/* Compare value in bufPtr with value in valPtr: returns -1, 0, 1 */
int AM_Compare(char *bufPtr, char attrType, int attrLength, char *valPtr)
{
    int bufint, valint;
    float buffloat, valfloat;

    switch (attrType) {
        case 'i':
            bcopy(bufPtr, (char *)&bufint, AM_si);
            bcopy(valPtr, (char *)&valint, AM_si);
            if (valint < bufint) return -1;
            if (valint > bufint) return 1;
            return 0;
        case 'f':
            bcopy(bufPtr, (char *)&buffloat, AM_sf);
            bcopy(valPtr, (char *)&valfloat, AM_sf);
            if (valfloat < buffloat) return -1;
            if (valfloat > buffloat) return 1;
            return 0;
        case 'c':
            /* byte-wise memcmp on fixed-length char attributes */
            {
                int r = memcmp(valPtr, bufPtr, (size_t)attrLength);
                if (r < 0) return -1;
                if (r > 0) return 1;
                return 0;
            }
        default:
            return 0;
    }
}
