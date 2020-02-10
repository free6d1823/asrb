#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <sched.h>
#include <unistd.h>

#include "asrb_int.h"

unsigned int g_kill_threads = 0;
unsigned int g_live_threads = 0;


void*  asrbGetBufferHeader(HANDLE handle, void* pBuffer)
{
	AsrbCbBase* pCb = (AsrbCbBase*) handle;
	for (int i=0; i<pCb->pHeader->frameCounts; i++){
		if (pBuffer == pCb->pData[i])
			return &(pCb->pHeader->info[i]);
	}
    return NULL;
}

int    asrbWriter_Init(HANDLE* pHandle, char* name, AsrbWriterConf* pData)
{
    void* pBufferList = NULL;
    unsigned char * pBuffer;
    AsrbBufferHeader* pHeader = NULL;
    WriterCb* pCb = (WriterCb*) malloc(sizeof(WriterCb));
    if ( !pCb)
        return ASRB_ERR_OutOfMemory;
    //TEMP: assigned phy base address by user
    g_BasePhyAddress = pData->basePhyAddress;
    g_PhyBufferSize = pData->count * pData->size;
    printf("Allocate maximum total buffers size %d bytes\n", g_PhyBufferSize);
    //
    int ret = asrbPhy_Init();
    if (ret != ASRB_OK)
    	return ret;

    memset(pCb, 0, sizeof(WriterCb));
    pCb->role = ASRB_ROLE_WRITER;
    strncpy(pCb->name, name, sizeof(pCb->name));

    /* init IPC */
    g_kill_threads = 0;
    g_live_threads = 0;
    StartServer(10001, (void*) pCb);
    /* set up buffers */
    pHeader = (AsrbBufferHeader*)asrbMem_AllocHeader();
    if (!pHeader) {
    	free (pCb);
        return ASRB_ERR_OutOfPhyMem;
    }

    printf("asrbWriter_Init AsrbBufferHeaderid=%d\n", pHeader->id);
    pHeader->frameCounts = pData->count;
    pHeader->maxSize = pData->size;
    pHeader->frameType = pData->type;
    pCb->fnCallback = pData->fnCallback;
    pCb->pHeader = pHeader;
    /*-- dump cb --*/
    printf("Writer CB: name=%s, count=%d/%d, size=%d\n", pCb->name,
    		pCb->pHeader->frameCounts, pHeader->frameCounts, pHeader->maxSize);

    /* setup each frame buffer */
    pBufferList = asrbMem_Get(pData->count * pData->size);
    if (!pBufferList){
    	free (pCb);
    	asrbMem_Return(pBufferList);
        return ASRB_ERR_OutOfPhyMem;
    }
    pBuffer = (unsigned char * )pBufferList;
    unsigned char * pPhy = (unsigned char * )asrbMem_VirtualToPhysical(pBufferList);
    printf("Buffer address Phy %p->Vir%p\n", pPhy, pBufferList);
    for (int i=0; i< pCb->pHeader->frameCounts; i++) {
    	AsrbFrameInfo* pFrame = pHeader->info + i;
         /*-dump buffer address--*/
        printf("Buffer %d ->Virt 0x%p\n", i, pBuffer);
        INIT_LOCK(pFrame->lock); //unlock
        pFrame->counter = 0;
        pFrame->width = pData->width;
        pFrame->height = pData->height;
        pFrame->pDataPhy =  pPhy; //phy address
        pCb->pData[i] = pBuffer; //virtual address
        pBuffer += pData->size;
        pPhy += pData->size;
    }
    printf("asrbWriter_Init OK\n");
    pCb->state = STATE_READY;
    *pHandle = (HANDLE) pCb;
    return ASRB_OK;
}

void*  asrbWriter_GetBuffer(HANDLE handle)
{
	printf("asrbWriter_GetBuffer--\n");
    WriterCb* pCb = (WriterCb*) handle;
    assert(pCb->role == ASRB_ROLE_WRITER);
    /* find the oldest unlock buffer */
    AsrbFrameInfo* pFrame = NULL;
    unsigned int nMin = 0xFFFFFFFF;
    int ringId = -1;

    //debug
    int n = pCb->pHeader->frameCounts;
 	if( 4 != n)
	{
		DUMP_HEADERS(pCb->pHeader);
		assert(0);
	}
    for (int i=0; i< pCb->pHeader->frameCounts; i++) {
        pFrame = &pCb->pHeader->info[i];
        if (IS_UNLOCK(pFrame->lock)) {
            printf("i=%d, counter=%d, nMin=%d\n", i, pFrame->counter, nMin);
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
    printf("Get buffer %d=%p, counter=%d\n", ringId,  pFrame, pFrame->counter);

    return pCb->pData[ringId];
}
void   asrbWriter_ReleaseBuffer(HANDLE handle, void* pBuffer)
{
    WriterCb* pCb = (WriterCb*) handle;
    assert(pCb->role == ASRB_ROLE_WRITER);
    AsrbFrameInfo*  pFrame = (AsrbFrameInfo* )asrbGetBufferHeader(handle, pBuffer);
    assert (pFrame); /* one writer, single thread, buffer must be the rent buffer */
    WRITER_UNLOCK(pFrame->lock);

    DUMP_FRAME_INFO(pFrame);

    pCb->state = STATE_READY;
}
void   asrbWriter_Free(HANDLE handle)
{
    WriterCb* pCb = (WriterCb*) handle;
    printf("asrbWriter_Free\n");
    assert(pCb->role == ASRB_ROLE_WRITER);
    g_kill_threads = (unsigned int)(-1);
    KillServer();
    while(g_live_threads){
    	usleep(10000);
    }

    if(pCb->pHeader){
    	for (int i=0; i< pCb->pHeader->frameCounts; i++){
    		DEST_LOCK(pCb->pHeader->info[i].lock);
    	}
        if(pCb->pData[0])
        	asrbMem_Return(pCb->pData[0]);

        asrbMem_FreeHeader(pCb->pHeader);
        asrbPhy_Destroy();
    }

    if (pCb)
        free(pCb);
}

