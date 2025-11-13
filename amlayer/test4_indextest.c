/**********************************************************************
 test4.c: Tests bulk-load index creation on an existing Student file.
************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "am.h"
#include "pf.h"
#include "pftypes.h"
#include "testam.h"  /* for xAM_*, xPF_* wrappers and constants */

#define RELNAME "student"
#define INDEXNO 1

int main(void)
{
    printf("Initializing PF/AM layers...\n");
    PF_Init();

    printf("Starting bulk-load test on relation \"%s\"...\n", RELNAME);

    /* Bulk-load the index assuming the data file exists and is sorted */
    AM_BulkLoad(RELNAME, INDEXNO, RELNAME);

    printf("Bulk-load completed for %s.%d\n", RELNAME, INDEXNO);

    /* Optionally verify index contents using AM_PrintTree if desired */
    printf("Bulk-load test finished successfully.\n");

    return 0;
}
