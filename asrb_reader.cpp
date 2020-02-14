#include <memory.h>
#include <malloc.h>
#include <assert.h>
#include "asrb_int.h"

//#define FIND_OLDER
void*  GetBuffer_Newer(HANDLE handle)
{
    ReaderCb* pCb = (ReaderCb*) handle;
    assert(pCb->role != ASRB_ROLE_WRITER);
    AsrbFrameInfo* pFrame = NULL;

    unsigned int nTh = 0x0;

    int ringId = -1;

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

    if(ringId == -1){
    	return NULL;
    }
    pFrame = &pCb->pHeader->info[ringId];
    READER_LOCK(pFrame->lock, pCb->idReader);
    pCb->currentCount = pFrame->counter;
    pCb->state = STATE_LOCKED;
   	//DUMP_FRAME_INFO(pFrame);

    return pCb->pData[ringId];
}
void*  GetBuffer_Older(HANDLE handle)
{
    ReaderCb* pCb = (ReaderCb*) handle;
    assert(pCb->role != ASRB_ROLE_WRITER);
    AsrbFrameInfo* pFrame = NULL;

    unsigned int nTh = (~0U);

    int ringId = -1;

	for (int i=0; i< pCb->pHeader->frameCounts; i++) {
		pFrame = &pCb->pHeader->info[i];
		if (IS_UNLOCK(pFrame->lock)){
			if (pFrame->counter > pCb->currentCount && pFrame->counter < nTh) {
				nTh = pFrame->counter;
				ringId = i;
			}
			READER_UNLOCK(pFrame->lock);
		}
	}

    if(ringId == -1){
    	return NULL;
    }
    pFrame = &pCb->pHeader->info[ringId];
    READER_LOCK(pFrame->lock, pCb->idReader);
    pCb->currentCount = pFrame->counter;
    pCb->state = STATE_LOCKED;
   	//DUMP_FRAME_INFO(pFrame);

    return pCb->pData[ringId];
}
/* Client/Reader API */
int    asrbReader_Open(HANDLE* pHandle, AsrbReaderConf* pData)
{
    ReaderCb* pCb = (ReaderCb*) malloc(sizeof(ReaderCb));
    if (!pCb)
    	return ASRB_ERR_OutOfMemory;
    memset(pCb, 0, sizeof(ReaderCb));

    int ret = asrbMem_Init((HANDLE)pCb, pData->conf);
    if( ret != ASRB_OK){
		return ret;
    }

    asrbMem_Dump((HANDLE)pCb);

    pCb->id = pData->id;
    switch (pData->idReader) {
    case 4:pCb->role = ASRB_ROLE_READER4;break;
    case 2:pCb->role = ASRB_ROLE_READER2;break;
    case 3:pCb->role = ASRB_ROLE_READER3;break;
    default:pCb->role = ASRB_ROLE_READER1;break;
    }
    printf("pCb->id %d\n", pData->id);
    printf("pCb->role %d\n", pCb->role);

    pCb->state = STATE_INIT;
    pCb->strategy = pData->strategy;

    if (pData->strategy == ASRB_STRATEGY_OLDER) {
    	pCb->fnGetBufferMethod = GetBuffer_Older;
    } else {
    	pCb->fnGetBufferMethod = GetBuffer_Newer;
    }
    AsrbBufferHeader* pHeader = asrbMem_FindHeaderPointer((HANDLE)pCb);
     if (!pHeader) {
      	asrbMem_Release((HANDLE)pCb);
     	free (pCb);
     	printf("Header ID %d not found! Please check configure file %s.\n", pData->id, pData->conf);
        return ASRB_ERR_HeaderNotFound;
     }
     //check if this ASRB is used by other writer
     if(pHeader->state == 0) {
     	printf("ASRB %d writer has not initiated. Try again later!\n", pCb->id);
     	free (pCb);
     	return ASRB_ERR_AsrbNotEstablished;
     }

     pCb->pHeader = pHeader;

     //init frame info
     for (int i=0; i< pCb->pHeader->frameCounts; i++) {
     	AsrbFrameInfo* pFrame = pHeader->info + i;
         INIT_LOCK(pFrame->lock); //unlock
         pFrame->counter = 0;
         pCb->pData[i] = asrbMem_PhysicalToVirtual((HANDLE) pCb, pFrame->phyData); //virtual address
     }

    pCb->state = STATE_READY;
    pCb->currentCount = 0;
    *pHandle = (HANDLE)pCb;
    if (pCb->strategy== ASRB_STRATEGY_OLDER)
        printf("ASMM_READER get buffer by the nearest frame.\n");
    else //ASMM_STRATEGY_NEWER
        printf("ASMM_READER get buffer by the newest frame.\n");

    RunClient(pCb->serverIp,  pCb->id, (int)pCb->role, IPCCMD_READER_JOIN, 0);
    return ASRB_OK;
}
void*  asrbReader_GetBuffer(HANDLE handle)
{
    ReaderCb* pCb = (ReaderCb*) handle;
    return pCb->fnGetBufferMethod(handle);
}
void   asrbReader_ReleaseBuffer(HANDLE handle, void* pBuffer)
{
    ReaderCb* pCb = (ReaderCb*) handle;
    assert(pCb->role != ASRB_ROLE_WRITER);
    AsrbFrameInfo*  pFrame = asrbMem_GetFrameInfoByBuffer(handle, pBuffer);
    assert (pFrame); /* one writer, single thread, buffer must be the rent buffer */
    READER_UNLOCK(pFrame->lock);

    pCb->state = STATE_READY;
}
void   asrbReader_Free(HANDLE handle)
{
    ReaderCb* pCb = (ReaderCb*) handle;
    if (pCb) {
        assert(pCb->role != ASRB_ROLE_WRITER);
        RunClient(pCb->serverIp,  pCb->id, (int)pCb->role, IPCCMD_READER_LEAVE, 0);
		asrbMem_Release(handle);
		free (pCb);
    }
}
