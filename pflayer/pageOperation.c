#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pf.h"
#include "pftypes.h"

int PF_AllocPage(int fd, int *pagenum, char **pageBuf){
    int error;
    PFhdr_str hdr;    
    int newpage;

    if((error=PFreadhdr(fd,&hdr)) !=PFE_OK ){
        return error;
    }

    if(hdr.firstfree!=-1){
        newpage=hdr.firstfree;
        PFfpage tempPage;//to store current free page
        if( (error=PFreadfcn(fd,newpage,&tempPage))!=PFE_OK ){
            return error;
        }
        hdr.firstfree=tempPage.nextfree;
    }else {
        newpage=hdr.numpages;
        hdr.numpages++;
    }

    //write back hdr
    if ((error = PFwritehdr(fd, &hdr)) != PFE_OK) {
        return error;
    }

    //alocate buffer
    PFfpage *fpage;
    if ((error = PFbufAlloc(fd,newpage,&fpage,PFwritefcn)) != PFE_OK){
        return error;
    }
    memset(fpage->pagebuf, 0, PF_PAGE_SIZE);

    *pagenum=newpage;
    *pageBuf = fpage->pagebuf;
    return PFE_OK;
}

int PF_GetNextPage(int fd, int *pagenum, char **pagebuf){
    int nextPageNum=*pagenum+1;
    int error;
    
    PFhdr_str hdr;
    if((error=PFreadhdr(fd,&hdr)) !=PFE_OK ){
        return error;
    }

    if(nextPageNum>=hdr.numpages){
        return PFE_EOF;
    }

    PFfpage *nextPage;
    if((error = PFbufGet(fd, nextPageNum, &nextPage, PFreadfcn, PFwritefcn)) !=PFE_OK){
        return error;
    }
    *pagenum=nextPageNum;
    *pagebuf=nextPage->pagebuf;
    return PFE_OK;
}

int PF_UnfixPage(int fd, int pagenum, int dirty){
    int error;
    if((error=PFbufUnfix(fd,pagenum,dirty))!=PFE_OK){
        return error;
    }
    return PFE_OK;
}
int PF_DisposePage(int fd, int pagenum) {
    PFfpage *page;
    PFhdr_str hdr;
    int error;

    if ((error = PFreadhdr(fd, &hdr)) != PFE_OK)
        return error;

    if (pagenum >= hdr.numpages)
        return -1;

    //Get curr page
    if ((error = PFbufGet(fd, pagenum, &page, PFreadfcn, PFwritefcn)) != PFE_OK)
        return error;

    // Insert into free-page list 
    page->nextfree = hdr.firstfree;
    hdr.firstfree = pagenum;
    // updated header
    if ((error = PFwritehdr(fd, &hdr)) != PFE_OK)
        return error;
    
        //Mark the disposed page dirty and unfix it
    PF_UnfixPage(fd, pagenum, TRUE);

    return PFE_OK;
}
