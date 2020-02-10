#include <memory.h>
#include <malloc.h>
#include <assert.h>
#include "asrb_int.h"

//#define FIND_OLDER

/* Client/Reader API */
int    asrbReader_Open(HANDLE* pHandle, char* name, AsrbReaderConf* pData)
{
    ReaderCb* pCb = (ReaderCb*) malloc(sizeof(ReaderCb));
    if (!pCb)
    	return ASRB_ERR_OutOfMemory;
    memset(pCb, 0, sizeof(ReaderCb));

    /* TODO: connect server to get header info and physical base address
     * 
     */
    g_BasePhyAddress = pData->basePhyAddress;
    g_PhyBufferSize = pData->count * pData->size;
    printf("Map maximum total buffers size %d bytes\n", g_PhyBufferSize);
    int nHeaderId = pData->id;
    //

    int ret = asrbPhy_Open();
    if (ret != ASRB_OK)
    	return ret;

    pCb->role = ASRB_ROLE_READER;
    pCb->state = STATE_INIT;
    strncpy(pCb->name, name, sizeof(pCb->name));
    pCb->idReader = pData->id;
    pCb->strategy = pData->strategy;

    AsrbBufferHeader* pHeader = (AsrbBufferHeader*)asrbMem_GetHeader(nHeaderId);
    if (!pHeader) {
    	free (pCb);
        return ASRB_ERR_HeaderNotAllocated;
    }
    assert(pHeader->id == nHeaderId);
    assert(pHeader->frameCounts == 4);
    //now update frames to user space
    for (int i=0; i< pHeader->frameCounts; i++){
    	AsrbFrameInfo* pFrame = pHeader->info + i;
        pCb->pData[i] = asrbMem_PhysicalToVirtual(pFrame->pDataPhy); //virtual address
    }
    DUMP_HEADERS(pHeader);
    //now change buffer physical address to virtual
    ///////////////////
    pCb->pHeader = pHeader;
    pCb->state = STATE_READY;
    pCb->currentCount = 0;
    *pHandle = (HANDLE)pCb;
    if (pCb->strategy== ASRB_STRATEGY_OLDER)
        printf("ASMM_READER get buffer by the nearest frame.\n");
    else //ASMM_STRATEGY_NEWER
        printf("ASMM_READER get buffer by the newest frame.\n");

    return ASRB_OK;
}
void*  asrbReader_GetBuffer(HANDLE handle)
{
    ReaderCb* pCb = (ReaderCb*) handle;
    assert(pCb->role == ASRB_ROLE_READER);
    AsrbFrameInfo* pFrame = NULL;

    unsigned int nTh;
    if (pCb->strategy == ASRB_STRATEGY_OLDER)
        nTh = (~0U);
    else //ASMM_STRATEGY_NEWER
        nTh = 0x0;

    int ringId = -1;
    if (pCb->strategy == ASRB_STRATEGY_OLDER) {
        //find unlocked min
        for (int i=0; i< pCb->pHeader->frameCounts; i++) {
            pFrame = &pCb->pHeader->info[i];
            if (IS_UNLOCK(pFrame->lock)){
   printf("frame i=%d, counter=%d, reader_counter=%d, nTh=%d\n", i, pFrame->counter, pCb->currentCount, nTh);
            	if (pFrame->counter > pCb->currentCount && pFrame->counter < nTh) {
                	nTh = pFrame->counter;
                    ringId = i;
                }
            	READER_UNLOCK(pFrame->lock);
            }
        }
    } else {//ASMM_STRATEGY_NEWER
        //find unlocked max counter 
        for (int i=0; i< pCb->pHeader->frameCounts; i++) {
            pFrame = &pCb->pHeader->info[i];
            if (IS_UNLOCK(pFrame->lock)) {
                printf("read i=%d, counter=%d, nTh=%d\n", i, pFrame->counter, nTh);

                 if ( pFrame->counter > pCb->currentCount && pFrame->counter > nTh){
                 	nTh = pFrame->counter;
                    ringId = i;
                }
             	READER_UNLOCK(pFrame->lock);
            }
        }
    }
    if(ringId == -1){
    	//debug
        int n = pCb->pHeader->frameCounts;
     	if( 4 != n)
    	{
    		DUMP_HEADERS(pCb->pHeader);
    		assert(0);
    	}
     	//
    	fprintf(stderr, "No buffer is ready! Try again later\n");
    	return NULL;
    }
    pFrame = &pCb->pHeader->info[ringId];
    READER_LOCK(pFrame->lock, pCb->idReader);
    pCb->currentCount = pFrame->counter;
    pCb->state = STATE_LOCKED;
   	//DUMP_FRAME_INFO(pFrame);

    return pCb->pData[ringId];
}
void   asrbReader_ReleaseBuffer(HANDLE handle, void* pBuffer)
{
    ReaderCb* pCb = (ReaderCb*) handle;
    assert(pCb->role == ASRB_ROLE_READER);
    AsrbFrameInfo*  pFrame = (AsrbFrameInfo* )asrbGetBufferHeader(handle, pBuffer);
    assert (pFrame); /* one writer, single thread, buffer must be the rent buffer */
    READER_UNLOCK(pFrame->lock);
    printf("asrbReader_ReleaseBuffer unlock frame %d \n", pFrame->counter);
    pCb->state = STATE_READY;
}
void   asrbReader_Free(HANDLE handle)
{
    ReaderCb* pCb = (ReaderCb*) handle;
    assert(pCb->role == ASRB_ROLE_READER);
    if (pCb) {
        if(pCb->pHeader){
            //don't free (pCb->pData[0])
            //don't call asrbMem_FreeHeader(pCb->pHeader);
            asrbPhy_Destroy();
        }
        free(pCb);
    }
}
