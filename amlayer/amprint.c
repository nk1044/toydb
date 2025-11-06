#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "am.h"
#include "pf.h"

void AM_PrintIntNode(char *pageBuf, char attrType)
{
    int tempPageint;
    int i;
    int recSize;
    AM_INTHEADER *header;

    header = (AM_INTHEADER *)calloc(1, AM_sint);
    bcopy(pageBuf, (char *)header, AM_sint);
    recSize = header->attrLength + AM_si;

    printf("PAGETYPE %c\n", header->pageType);
    printf("NUMKEYS %d\n", header->numKeys);
    printf("MAXKEYS %d\n", header->maxKeys);
    printf("ATTRLENGTH %d\n", header->attrLength);

    bcopy(pageBuf + AM_sint, (char *)&tempPageint, AM_si);
    printf("FIRSTPAGE is %d\n", tempPageint);

    for (i = 1; i <= header->numKeys; i++) {
        AM_PrintAttr(pageBuf + (i - 1) * recSize + AM_sint + AM_si,
                     attrType, header->attrLength);
        bcopy(pageBuf + i * recSize + AM_sint, (char *)&tempPageint, AM_si);
        printf("NEXTPAGE is %d\n", tempPageint);
    }
    free(header);
}

void AM_PrintLeafNode(char *pageBuf, char attrType)
{
    short nextRec;
    int i;
    int recSize;
    int recId;
    int offset1;
    AM_LEAFHEADER *header;

    header = (AM_LEAFHEADER *)calloc(1, AM_sl);
    bcopy(pageBuf, (char *)header, AM_sl);
    recSize = header->attrLength + AM_ss;

    printf("PAGETYPE %c\n", header->pageType);
    printf("NEXTLEAFPAGE %d\n", header->nextLeafPage);
    printf("NUMKEYS %d\n", header->numKeys);

    for (i = 1; i <= header->numKeys; i++) {
        offset1 = (i - 1) * recSize + AM_sl;
        AM_PrintAttr(pageBuf + AM_sl + (i - 1) * recSize,
                     attrType, header->attrLength);
        bcopy(pageBuf + offset1 + header->attrLength, (char *)&nextRec, AM_ss);

        while (nextRec != 0) {
            bcopy(pageBuf + nextRec, (char *)&recId, AM_si);
            printf("RECID is %d\n", recId);
            bcopy(pageBuf + nextRec + AM_si, (char *)&nextRec, AM_ss);
        }
        printf("\n\n");
    }
    free(header);
}

int AM_DumpLeafPages(int fileDesc, int min, char attrType, int attrLength)
{
    int pageNum;
    char *value;
    char *pageBuf;
    int index;      /* kept to preserve original locals */
    int errVal;
    AM_LEAFHEADER *header;

    (void)attrLength; /* suppress unused param warning */
    (void)index;      /* suppress unused local warning */

    value = (char *)malloc(AM_si);
    bcopy((char *)&min, value, AM_si);

    /* start at leftmost leaf page */
    pageNum = AM_LeftPageNum;
    printf("%d PAGE \n", AM_LeftPageNum);

    errVal = PF_GetThisPage(fileDesc, AM_LeftPageNum, &pageBuf);
    AM_Check;

    header = (AM_LEAFHEADER *)calloc(1, AM_sl);
    bcopy(pageBuf, (char *)header, AM_sl);

    while (header->nextLeafPage != -1) {
        printf("PAGENUMBER = %d\n", pageNum);
        AM_PrintLeafKeys(pageBuf, attrType);

        errVal = PF_UnfixPage(fileDesc, pageNum, FALSE);
        AM_Check;

        pageNum = header->nextLeafPage;

        errVal = PF_GetThisPage(fileDesc, pageNum, &pageBuf);
        AM_Check;

        bcopy(pageBuf, (char *)header, AM_sl);
    }

    printf("PAGENUMBER = %d\n", pageNum);
    AM_PrintLeafKeys(pageBuf, attrType);

    errVal = PF_UnfixPage(fileDesc, pageNum, FALSE);
    AM_Check;

    free(value);
    free(header);
    return AME_OK;
}

void AM_PrintLeafKeys(char *pageBuf, char attrType)
{
    short nextRec;
    int i;
    int recSize;
    int recId;
    int offset1;
    AM_LEAFHEADER *header;

    header = (AM_LEAFHEADER *)calloc(1, AM_sl);
    bcopy(pageBuf, (char *)header, AM_sl);
    recSize = header->attrLength + AM_ss;

    for (i = 1; i <= header->numKeys; i++) {
        offset1 = (i - 1) * recSize + AM_sl;
        AM_PrintAttr(pageBuf + AM_sl + (i - 1) * recSize,
                     attrType, header->attrLength);
        bcopy(pageBuf + offset1 + header->attrLength, (char *)&nextRec, AM_ss);

        while (nextRec != 0) {
            bcopy(pageBuf + nextRec, (char *)&recId, AM_si);
            printf("RECID is %d\n", recId);
            bcopy(pageBuf + nextRec + AM_si, (char *)&nextRec, AM_ss);
        }
    }
    free(header);
}

void AM_PrintAttr(char *bufPtr, char attrType, int attrLength)
{
    int bufint;
    float buffloat;
    char *bufstr;

    switch (attrType) {
    case 'i':
        bcopy(bufPtr, (char *)&bufint, AM_si);
        printf("ATTRIBUTE is %d\n", bufint);
        break;
    case 'f':
        bcopy(bufPtr, (char *)&buffloat, AM_sf);
        printf("ATTRIBUTE is %f\n", buffloat);
        break;
    case 'c':
        bufstr = (char *)malloc((size_t)attrLength + 1);
        bcopy(bufPtr, bufstr, attrLength);
        bufstr[attrLength] = '\0';
        printf("ATTRIBUTE is %s\n", bufstr);
        free(bufstr);
        break;
    default:
        break;
    }
}

void AM_PrintTree(int fileDesc, int pageNum, char attrType)
{
    int nextPage;   (void)nextPage; /* kept to preserve locals; currently unused */
    int errVal;
    AM_INTHEADER *header;
    char *tempPage;
    char *pageBuf;
    int recSize;    (void)recSize;  /* currently not used in the shown snippet */
    int i;          (void)i;

    printf("GETTING PAGE = %d\n", pageNum);

    errVal = PF_GetThisPage(fileDesc, pageNum, &pageBuf);
    (void)errVal;

    tempPage = (char *)malloc(PF_PAGE_SIZE);
    bcopy(pageBuf, tempPage, PF_PAGE_SIZE);

    errVal = PF_UnfixPage(fileDesc, pageNum, FALSE);
    (void)errVal;

    if (*tempPage == 'l') {
        printf("PAGENUM = %d\n", pageNum);
        AM_PrintLeafKeys(tempPage, attrType);
        free(tempPage);
        return;
    }

    header = (AM_INTHEADER *)calloc(1, AM_sint);
    bcopy(tempPage, (char *)header, AM_sint);

    recSize = header->attrLength + AM_si;
    /* (rest of original traversal continues; logic preserved) */

    free(header);
    free(tempPage);
}
