#include<stdio.h>  
#include<unistd.h>  
#include<sys/mman.h>  
#include<sys/types.h>  
#include<sys/stat.h>  
#include<fcntl.h>  
#include <errno.h>  
#include <string.h>
#include <assert.h>
#include "inifile.h"
#include "asrb_int.h"

static int g_ref = 0;
#ifdef USE_LINUX
void INIT_LOCK(pthread_mutex_t& st)
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&st, &attr);
}
#endif

/*Physical memory managementAPI */
int asrbMem_Init(HANDLE handle, const char* conf)
{
	void* hIni = openIniFile(conf, true);
	if (!hIni)
		return ASRB_ERR_FileNotFound;
	AsrbCbBase* pCb = (AsrbCbBase*)handle;
	pCb->basePhyAddress = GetProfileHex("mrr", "start", 0x10000, hIni);
	int nHeaderCounts = GetProfileInt("mrr", "headerCounts", 1, hIni);
	int sizeKb  = GetProfileInt("mrr", "sizeKb", 1000, hIni);
	pCb->phyBufferSize = sizeKb * 1024;
    int fd = open("/dev/mem", O_RDWR|O_SYNC);
    if (fd == -1)
    {
        fprintf(stderr, "failed to open mem device. err=%d %s\n", errno, strerror(errno));
        closeIniFile(hIni);
        return (ASRB_ERR_OpenMmapFailed);
    }
    VIR_ADDR pBase = (VIR_ADDR) mmap((unsigned char*)NULL, pCb->phyBufferSize,
    		PROT_READ|PROT_WRITE, MAP_SHARED,fd, pCb->basePhyAddress);
    if ( pBase == (VIR_ADDR)(-1)) {
    	fprintf(stderr, "mmap (0x%x KBs at 0x%lx) failed. %d %s\n",
    			(unsigned int) sizeKb, pCb->basePhyAddress,
    			errno, strerror(errno));
        close (fd);
        closeIniFile(hIni);
        return ASRB_ERR_OpenMmapFailed;
    }
    pCb->fdMem = fd;
    pCb->pMrr = (MasterRecordRegion*) pBase;
    printf("asrbPhy_Open OK. Phy 0x%lx maps %d KB to 0x%p\n", pCb->basePhyAddress, sizeKb, pBase);
    //write mast record region
    g_ref ++;
     if ( memcmp(&(pCb->pMrr->tag), GSMM_TAG, 4) != 0) {
    	printf("Physical pool is not initialized.\nInitilize it.\n");
		memcpy(&(pCb->pMrr->tag), GSMM_TAG, 4);
		pCb->pMrr->sizeKb = sizeKb;
		pCb->pMrr->headerCounts = nHeaderCounts;
		pCb->pMrr->offsetHeader = sizeof(MasterRecordRegion);
		pCb->pMrr->offsetBuffers = sizeof(AsrbBufferHeader)*nHeaderCounts + pCb->pMrr->offsetHeader;

		AsrbBufferHeader* pHeader = (AsrbBufferHeader*) (pBase + pCb->pMrr->offsetHeader);
		PHY_ADDR pFrameBuffer = (pCb->basePhyAddress + pCb->pMrr->offsetBuffers);
		for (int i=0; i< nHeaderCounts; i++) {
			char section[16];
			sprintf(section, "header%d", i);
			pHeader->state = 0; /* clear header state */
			pHeader->id = GetProfileInt(section, "id", i, hIni);
			pHeader->frameCounts =  GetProfileInt(section, "frameCounts", 2, hIni);
			if(pHeader->frameCounts > MAX_FRAME_COUNTS) pHeader->frameCounts = MAX_FRAME_COUNTS;
			pHeader->maxSize = GetProfileInt(section, "maxSize", 100, hIni);
			pHeader->frameType = (ASRB_FRAME_TYPE) GetProfileInt(section, "frameType", 0, hIni);
			pHeader->align = GetProfileInt(section, "align", 0, hIni);
			int alignRemained = (1 << pHeader->align)-1;

			for (int j=0; j< pHeader->frameCounts; j++) {
				pHeader->info[j].phyData = (PHY_ADDR) (((pFrameBuffer + alignRemained)>>pHeader->align)<<pHeader->align);
				pFrameBuffer = (PHY_ADDR) (pHeader->info[j].phyData) + pHeader->maxSize;
			}
			pHeader++;
		}
    }
    closeIniFile(hIni);
	return ASRB_OK;
}
AsrbBufferHeader* asrbMem_FindHeaderPointer(HANDLE handle)
{
	AsrbCbBase* pCb = (AsrbCbBase*)handle;
	AsrbBufferHeader* pHeader = NULL;
	AsrbBufferHeader* pItem = (AsrbBufferHeader*)( ((VIR_ADDR)pCb->pMrr) + pCb->pMrr->offsetHeader);
	for (int i=0; i<pCb->pMrr->headerCounts; i++ ){
		if (pItem->id == pCb->id) {
			pHeader = pItem;
			break;
		}
		pItem++;
	}
	return pHeader;
}
AsrbFrameInfo*  asrbMem_GetFrameInfoByBuffer(HANDLE handle, void* pBuffer)
{
	AsrbCbBase* pCb = (AsrbCbBase*) handle;
	for (int i=0; i<pCb->pHeader->frameCounts; i++){
		if (pBuffer == pCb->pData[i])
			return pCb->pHeader->info+i;
	}
	printf("asrbMem_GetFrameInfoByBuffer error!! %d, %d, %p\n", pCb->id, pCb->pHeader->frameCounts, pBuffer);
    return NULL;
}

void asrbMem_Dump(HANDLE handle)
{
	AsrbCbBase* pCb = (AsrbCbBase*)handle;
	if(pCb->pMrr) {
		VIR_ADDR baseVirt = (VIR_ADDR)pCb->pMrr;
		printf("GSMM memory pool:\n");
		printf("\tStart: 0x%08lx (Virt 0x%p)\n", pCb->basePhyAddress, baseVirt);
		printf("\t  End: 0x%08lx (Virt 0x%p)\n", pCb->basePhyAddress+pCb->phyBufferSize, baseVirt+pCb->phyBufferSize);

	    MasterRecordRegion* pMrr = pCb->pMrr;
		printf("MasterRecordRegion:\n");
	    printf("\tTag[4]= %c%c%c%c\n", pMrr->tag[0],pMrr->tag[1],pMrr->tag[2], pMrr->tag[3]);
	    printf("\tsizeKb= %d\n", pMrr->sizeKb);
	    printf("\theaderCounts= %d\n", pMrr->headerCounts);
	    printf("\toffsetHeader= 0x%0x\n", pMrr->offsetHeader);
	    printf("\toffsetBuffers= 0x%0x\n", pMrr->offsetBuffers);
    	long int pFrameBuffer = pCb->basePhyAddress + pMrr->offsetBuffers;
    	printf("\tFrame buffer pool start, phy=0x%0lx\n", pFrameBuffer);
		printf("HeaderRegion:\n");
		AsrbBufferHeader* pHeader = (AsrbBufferHeader*) (baseVirt + pMrr->offsetHeader);
	    for (int i=0; i<pMrr->headerCounts; i++) {
	    	printf("-- header #%d --\n", i);
	    	printf("\tid= %d\t\n", pHeader->id);
	    	printf("\tframeCounts= %d\t\n", pHeader->frameCounts);
	    	printf("\tframeType= %d\n", pHeader->frameType);
	    	printf("\tmaxSize= %d bytes\n", pHeader->maxSize);
	    	printf("\talign= 0x%x bytes\n", (1<<pHeader->align));

	    	for (int j=0; j< pHeader->frameCounts; j++) {
	    		printf("\tframe #%d phy address = 0x%0lx (virt=0x%p)\n", j, pHeader->info[j].phyData,
	    				baseVirt + (pHeader->info[j].phyData - pCb->basePhyAddress));
	    	}
	    	pHeader ++;
	    }
	} else {
		printf("GSMM is not initialized.\n");
	}
	printf("\n");
}

void asrbMem_DumpHeader(HANDLE handle)
{
	AsrbCbBase* pCb = (AsrbCbBase*)handle;
	if(pCb) {
		AsrbBufferHeader* pHeader = pCb->pHeader;
		printf("-- header id %d --\n", pHeader->id);
		printf("frameCounts= %d\t\n", pHeader->frameCounts);
		printf("\tframeType= %d\n", pHeader->frameType);
		printf("\tmaxSize= %d bytes\n", pHeader->maxSize);
		long int pFrameBuffer = pCb->basePhyAddress + pCb->pMrr->offsetBuffers;
		printf("Frame buffer start, phy=%ld\n", pFrameBuffer);
		for (int j=0; j< pHeader->frameCounts; j++) {
			printf("---- frame #%d info ----\n", j);
			printf("\tlock= %lx bytes long\n", sizeof(pHeader->info[j].lock));
			printf("\tcounter= %d\n", pHeader->info[j].counter);
			printf("\tsize= %d bytes\n", pHeader->info[j].size);
			printf("\twidth= %d\n", pHeader->info[j].width);
			printf("\theight= %d\n", pHeader->info[j].height);
    		printf("\tphy address = 0x%0lx (virt=0x%p)\n",  pHeader->info[j].phyData, pCb->pData[j]);
		}
	}
	printf("\n");
}
void asrbMem_DumpFrameInfo(HANDLE handle, AsrbFrameInfo* pFrame)
{
	AsrbCbBase* pCb = (AsrbCbBase*)handle;
	printf("----ASRB %d dump frame ---\n", pCb->id);
	printf("\tlock= %lx bytes long\n", sizeof(pFrame->lock));
	printf("\tcounter= %d\n", pFrame->counter);
	printf("\tsize= %d bytes\n", pFrame->size);
	printf("\twidth= %d\n", pFrame->width);
	printf("\theight= %d\n", pFrame->height);
	VIR_ADDR pVert = asrbMem_PhysicalToVirtual(pCb, pFrame->phyData); //virtual address
	printf("\tphy address = 0x%lx(0x%p) \n",  pFrame->phyData, pVert);

	if(pVert[0])
		printf(">>%s\n", pVert);

}
void asrbMem_Release(HANDLE handle)
{
	AsrbCbBase* pCb = (AsrbCbBase*)handle;
	if (pCb) {
		char* baseVirt = (char*)pCb->pMrr;
		g_ref --;
		if(g_ref ==0)
			memset(baseVirt, 0, 4); //erase tag
	    printf("munmap 0x%p %ld Bytes\n", baseVirt, pCb->phyBufferSize);
		munmap(baseVirt, pCb->phyBufferSize);

		if (pCb->fdMem != -1)
			close(pCb->fdMem);
		printf("asrbMem_Release OK.\n");

		memset(pCb, 0, sizeof(AsrbCbBase));
	}
}

VIR_ADDR asrbMem_PhysicalToVirtual(HANDLE handle, PHY_ADDR phy)
{
	AsrbCbBase* pCb = (AsrbCbBase*)handle;
	if (pCb) {
		return (VIR_ADDR) ((PHY_ADDR)pCb->pMrr +  phy -pCb->basePhyAddress);
	} else {
		return NULL;
	}
}
PHY_ADDR asrbMem_VirtualToPhysical(HANDLE handle, VIR_ADDR pVirt)
{
	AsrbCbBase* pCb = (AsrbCbBase*)handle;
	assert ( ((unsigned long int)pVirt >= (unsigned long int)pCb->pMrr )  &&  ( (unsigned long int)pVirt < ((unsigned long int)pCb->pMrr) + pCb->phyBufferSize));

	return (PHY_ADDR)(((PHY_ADDR)pVirt - (PHY_ADDR)pCb->pMrr)+ pCb->basePhyAddress);
}

