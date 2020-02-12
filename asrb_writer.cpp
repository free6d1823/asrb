#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <sched.h>
#include <unistd.h>
#include <errno.h>

#include "asrb_int.h"

unsigned int g_kill_threads = 0;
unsigned int g_live_threads = 0;


int    asrbWriter_Init(HANDLE* pHandle, AsrbWriterConf* pData)
{
    WriterCb* pCb = (WriterCb*) malloc(sizeof(WriterCb));
    if ( !pCb)
        return ASRB_ERR_OutOfMemory;
    memset(pCb, 0, sizeof(WriterCb));

    int ret = asrbMem_Init((HANDLE)pCb, pData->conf);
    if( ret != ASRB_OK){
		return ret;
    }

    pCb->role = ASRB_ROLE_WRITER;
    pCb->id = pData->id;

    /* init IPC */
    g_kill_threads = 0;
    g_live_threads = 0;
    //StartServer(10001, (void*) pCb);

    /* set up buffers */
    AsrbBufferHeader* pHeader = asrbMem_FindHeaderPointer((HANDLE)pCb);
    if (!pHeader) {
    	printf("Header ID %d not found! Please check configure file %s.\n", pData->id, pData->conf);
    	asrbMem_Release((HANDLE)pCb);
    	free (pCb);
        return ASRB_ERR_HeaderNotFound;
    }
    //check if this ASRB is used by other writer
    if(pHeader->state != 0) {
    	printf("ASRB %d has been occupied!\n", pCb->id);
    	asrbMem_Release((HANDLE)pCb);
    	free (pCb);
    	return ASRB_ERR_AsrbIsUsed;
    }
    pHeader->state = 1;

    printf("asrbWriter_Init AsrbBufferHeader id=%d\n", pHeader->id);
    pCb->fnCallback = pData->fnCallback;
    pCb->pHeader = pHeader;
    /*-- dump cb --*/
    printf("Writer CB: id=%d, count=%d/%d, size=%d\n", pCb->id,
    		pCb->pHeader->frameCounts, pHeader->frameCounts, pHeader->maxSize);

    //init frame info
    for (int i=0; i< pCb->pHeader->frameCounts; i++) {
    	AsrbFrameInfo* pFrame = pHeader->info + i;
        INIT_LOCK(pFrame->lock); //unlock
        pFrame->counter = 0;
        pFrame->width = pData->width;
        pFrame->height = pData->height;
        pCb->pData[i] = asrbMem_PhysicalToVirtual((HANDLE) pCb, pFrame->phyData); //virtual address
    }
    printf("asrbWriter_Init OK\n");
    pCb->state = STATE_READY;
    *pHandle = (HANDLE) pCb;
    return ASRB_OK;
}

void*  asrbWriter_GetBuffer(HANDLE handle)
{

    WriterCb* pCb = (WriterCb*) handle;
    assert(pCb->role == ASRB_ROLE_WRITER);
    /* find the oldest unlock buffer */
    AsrbFrameInfo* pFrame = NULL;
    unsigned int nMin = 0xFFFFFFFF;
    int ringId = -1;

    printf("asrbWriter_GetBuffer id=%d frameCounts=%d\n", pCb->pHeader->id, pCb->pHeader->frameCounts);
    for (int i=0; i< pCb->pHeader->frameCounts; i++) {
        pFrame = &pCb->pHeader->info[i];
        printf("i=%d counter=%d nMin=%d\n", i, pFrame->counter, nMin);

        if (IS_UNLOCK(pFrame->lock)) {

            if (pFrame->counter < nMin){
                nMin = pFrame->counter;
                ringId = i;

            }
            WRITER_UNLOCK(pFrame->lock);
        }
    }
    if(ringId == -1){
    	fprintf(stderr, "No free buffer! This should be error for writer!!\n");
    	return NULL;
    }
    pFrame = &pCb->pHeader->info[ringId];

    WRITER_LOCK(pFrame->lock);
    pFrame->counter = ++ pCb->currentCount;

    pCb->state = STATE_LOCKED;

    return pCb->pData[ringId];
}
void   asrbWriter_ReleaseBuffer(HANDLE handle, void* pBuffer)
{
    WriterCb* pCb = (WriterCb*) handle;
    assert(pCb->role == ASRB_ROLE_WRITER);
    AsrbFrameInfo*  pFrame = asrbMem_GetFrameInfoByBuffer(handle, pBuffer);
    assert (pFrame); /* one writer, single thread, buffer must be the rent buffer */
    WRITER_UNLOCK(pFrame->lock);

    pCb->state = STATE_READY;
}
void   asrbWriter_Free(HANDLE handle)
{
    WriterCb* pCb = (WriterCb*) handle;
    printf("asrbWriter_Free\n");
    if(pCb) {
		assert(pCb->role == ASRB_ROLE_WRITER);
		g_kill_threads = (unsigned int)(-1);
		KillServer();
		while(g_live_threads){
			usleep(10000);
		}
		//clear header
		AsrbBufferHeader* pHeader = asrbMem_FindHeaderPointer(handle);
		pHeader->state = 0;

		asrbMem_Release(handle);

        free(pCb);
    }
}

