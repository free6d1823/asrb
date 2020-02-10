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

/* GSMM memory structure */
#define GSMM_TAG "MMSG"
typedef struct _MasterRecordRegion {
	char tag[4];
	int  sizeKb; 					/*bytes of the pool */
	int headerCounts; 				/* numbers of header */
	unsigned int offsetHeader;		/* offset of buffers */
	unsigned int offsetBuffers; 	/* offset of buffers */
}MasterRecordRegion;
////////////////////////////////////
static int g_fdMem = -1;
static unsigned char* g_pvMapBase = NULL; //virtual base address
long int g_BasePhyAddress = 0;
unsigned long int g_PhyBufferSize = 0; //total pool size. This can be used to verify address.

static int g_openCount = 0;
static int g_Ref = 0;
#define POOL_TAG	"ASMPOOL"
typedef struct _AsrbPool {
	char tag[8];
	int  size; /*bytes of the pool */
	char headerTable[MAX_ASRB_NUM];
	unsigned int offsetHeader;
	unsigned int offsetBuffers; /* offset of buffers */
}AsrbPool;

#ifdef USE_LINUX
void INIT_LOCK(pthread_mutex_t& st)
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&st, &attr);
}
#endif

#define HEADER_OFFSET	16	/* start off header area */
#define BUFFER_OFFSET   0x400  /* start of buffer */

/* layout in physical memory
 * [header 0][header 1]......[header MAX_HEADER-1]
 * --- header ---
 * [header_id]                             name[64]
 * [buffer_count  ]  <-------------------  pHeader
 * [max_size_of_buffer]
 * [frame_type]
 * - for (i=0~buffer_count )
 * [frame_i_size]    <-------------------  pFrame[i]
 * [frame_i_width]
 * [frame_i_height]
 * [frame_i_data_phy]
 *                                         pVirtData
 *
 *
 * */

/*Physical memory managementAPI */
int gsmmInit(const char* conf)
{
	void* hIni = openIniFile(conf, true);
	if (!hIni)
		return ASRB_ERR_FileNotFound;
	g_BasePhyAddress = GetProfileHex("mrr", "start", 0x10000, hIni);
	int nHeaderCounts = GetProfileInt("mrr", "headerCounts", 1, hIni);
	int sizeKb  = GetProfileInt("mrr", "sizeKb", 1000, hIni);
	g_PhyBufferSize = sizeKb * 1024;
    int fd = open("/dev/mem", O_RDWR|O_SYNC);
    if (fd == -1)
    {
        fprintf(stderr, "failed to open mem device. err=%d %s\n", errno, strerror(errno));
        return (ASRB_ERR_OpenMmapFailed);
    }
    void* pBase = mmap((unsigned char*)NULL, g_PhyBufferSize,
    		PROT_READ|PROT_WRITE, MAP_SHARED,fd, g_BasePhyAddress);
    if ( pBase == (void*)(-1)) {
    	fprintf(stderr, "mmap (0x%x KBs at 0x%lx) failed. %d %s\n",
    			(unsigned int) sizeKb, g_BasePhyAddress,
    			errno, strerror(errno));
        close (fd);
        return ASRB_ERR_OpenMmapFailed;
    }
    g_fdMem = fd;
    g_pvMapBase = (unsigned char*) pBase;
    printf("asrbPhy_Open OK. Phy 0x%lx maps %d KB to 0x%p\n", g_BasePhyAddress, sizeKb, g_pvMapBase);
    //write mast record region
    MasterRecordRegion* pMrr = (MasterRecordRegion*)g_pvMapBase;
    memcpy(&(pMrr->tag), GSMM_TAG, 4);
    pMrr->sizeKb = sizeKb;
    pMrr->headerCounts = nHeaderCounts;
    pMrr->offsetHeader = sizeof(MasterRecordRegion);
    pMrr->offsetBuffers = sizeof(AsrbBufferHeader)*nHeaderCounts + pMrr->offsetHeader;

    AsrbBufferHeader* pHeader = (AsrbBufferHeader*) (g_pvMapBase + pMrr->offsetHeader);
	long int pFrameBuffer = (g_BasePhyAddress + pMrr->offsetBuffers);
	for (int i=0; i< nHeaderCounts; i++) {
		char section[16];
		sprintf(section, "header%d", i);
		pHeader->id = GetProfileInt(section, "id", i, hIni);
		pHeader->frameCounts =  GetProfileInt(section, "frameCounts", 2, hIni);
		if(pHeader->frameCounts > MAX_FRAME_COUNT) pHeader->frameCounts = MAX_FRAME_COUNT;
		pHeader->maxSize = GetProfileInt(section, "maxSize", 100, hIni);
		pHeader->frameType = (ASRB_FRAME_TYPE) GetProfileInt(section, "frameType", 0, hIni);
		pHeader->align = GetProfileInt(section, "align", 0, hIni);
		int alignRemained = (1 << pHeader->align)-1;

		for (int j=0; j< pHeader->frameCounts; j++) {
			pHeader->info[j].pDataPhy = (void*) (((pFrameBuffer + alignRemained)>>pHeader->align)<<pHeader->align);
			pFrameBuffer = (long int) (pHeader->info[j].pDataPhy) + pHeader->maxSize;
		}
		pHeader++;
	}
	return ASRB_OK;
}

void gsmmDump()
{
	if(g_pvMapBase) {
		printf("GSMM memory pool:\n");
		printf("\tStart: 0x%08lx (Virt 0x%p)\n", g_BasePhyAddress, g_pvMapBase);
		printf("\t  End: 0x%08lx (Virt 0x%p)\n", g_BasePhyAddress+g_PhyBufferSize, g_pvMapBase+g_PhyBufferSize);

	    MasterRecordRegion* pMrr = (MasterRecordRegion*)g_pvMapBase;
		printf("MasterRecordRegion:\n");
	    printf("\tTag[4]= %c%c%c%c\n", pMrr->tag[0],pMrr->tag[1],pMrr->tag[2], pMrr->tag[3]);
	    printf("\tsizeKb= %d\n", pMrr->sizeKb);
	    printf("\theaderCounts= %d\n", pMrr->headerCounts);
	    printf("\toffsetHeader= 0x%0x\n", pMrr->offsetHeader);
	    printf("\toffsetBuffers= 0x%0x\n", pMrr->offsetBuffers);
    	long int pFrameBuffer = g_BasePhyAddress + pMrr->offsetBuffers;
    	printf("\tFrame buffer pool start, phy=0x%0lx\n", pFrameBuffer);
		printf("HeaderRegion:\n");
		AsrbBufferHeader* pHeader = (AsrbBufferHeader*) (g_pvMapBase + pMrr->offsetHeader);
	    for (int i=0; i<pMrr->headerCounts; i++) {
	    	printf("-- header #%d --\n", i);
	    	printf("\tid= %d\t\n", pHeader->id);
	    	printf("\tframeCounts= %d\t\n", pHeader->frameCounts);
	    	printf("\tframeType= %d\n", pHeader->frameType);
	    	printf("\tmaxSize= %d bytes\n", pHeader->maxSize);
	    	printf("\talign= 0x%x bytes\n", (1<<pHeader->align));

	    	for (int j=0; j< pHeader->frameCounts; j++) {
	    		printf("\tframe #%d phy address = 0x%p (virt=0x%p)\n", j, pHeader->info[j].pDataPhy,
	    				g_pvMapBase + ((long int)pHeader->info[j].pDataPhy - g_BasePhyAddress));
	    	}
	    	pHeader ++;
	    }
	} else {
		printf("GSMM is not initialized.\n");
	}
	printf("\n");
}

void gsmmDumpHeader(int id)
{
	if(g_pvMapBase) {
		MasterRecordRegion* pMrr = (MasterRecordRegion*)g_pvMapBase;
		AsrbBufferHeader* pHeader = (AsrbBufferHeader*) (g_pvMapBase + pMrr->offsetHeader);
		pHeader += id;
		printf("-- header #%d --\n", id);
		printf("id= %d\t\n", pHeader->id);
		printf("frameCounts= %d\t\n", pHeader->frameCounts);
		printf("\tframeType= %d\n", pHeader->frameType);
		printf("\tmaxSize= %d bytes\n", pHeader->maxSize);
		long int pFrameBuffer = g_BasePhyAddress + pMrr->offsetBuffers;
		printf("Frame buffer start, phy=%ld\n", pFrameBuffer);
		for (int j=0; j< pHeader->frameCounts; j++) {
			printf("---- frame #%d info ----\n", j);
			printf("\tlock= %x\n", pHeader->info[j].lock);
			printf("\tcounter= %d\n", pHeader->info[j].counter);
			printf("\tsize= %d bytes\n", pHeader->info[j].size);
			printf("\twidth= %d\n", pHeader->info[j].width);
			printf("\theight= %d\n", pHeader->info[j].height);
    		printf("\tphy address = 0x%p (virt=0x%p)\n",  pHeader->info[j].pDataPhy,
    				g_pvMapBase + ((long int)pHeader->info[j].pDataPhy - g_BasePhyAddress));
		}
	}
	printf("\n");
}

void gsmmRelease()
{
	if (g_pvMapBase) {
	    MasterRecordRegion* pMrr = (MasterRecordRegion*)g_pvMapBase;
	    memset(g_pvMapBase, 0, 4); //erase tag
	    printf("munmap 0x%p %d Bytes\n", g_pvMapBase, g_PhyBufferSize);
		munmap(g_pvMapBase, g_PhyBufferSize);
	}
	if (g_fdMem != -1)
		close(g_fdMem);
	printf("gsmmRelease OK.\n");

	g_fdMem = -1;
	g_pvMapBase = NULL;
	g_BasePhyAddress = 0;
	g_PhyBufferSize = 0;
}

void* asrbMem_PhysicalToVirtual(void* pPyh)
{
	if (g_pvMapBase) {
		return (void*) ((unsigned long int)g_pvMapBase + (unsigned long int)(pPyh-g_BasePhyAddress));
	} else {
		return NULL;
	}
}
void* asrbMem_VirtualToPhysical(void* pVirt)
{
	assert (pVirt >= g_pvMapBase && pVirt < g_pvMapBase+ g_PhyBufferSize);
	return (void*)(((unsigned char*)pVirt - g_pvMapBase)+ g_BasePhyAddress);
}

AsrbBufferHeader* asrbMem_GetHeader(int idHeader)
{
	 AsrbPool* pBase = (AsrbPool*)g_pvMapBase;
	 AsrbBufferHeader* pHeader =  (AsrbBufferHeader*)(g_pvMapBase + pBase->offsetHeader);

	 int i = idHeader -1;//index in header table
	 if (i >=0 && i < MAX_ASRB_NUM) {
		 if (pBase->headerTable[i] == idHeader){
			 pHeader = pHeader +i;
			 assert(pHeader->id == idHeader);//id is index+1
			 printf("asrbMem_GetHeader(%d) returns %p\n", idHeader, pHeader);
			 return pHeader;
		 }
	 }
	 return NULL;
}
AsrbBufferHeader* asrbMem_AllocHeader()
{
	 AsrbPool* pBase = (AsrbPool*)g_pvMapBase;
	 AsrbBufferHeader* pHeader =  (AsrbBufferHeader*)(g_pvMapBase + pBase->offsetHeader);

	 for (int i=0; i< MAX_ASRB_NUM; i++) {
		 if (pBase->headerTable[i] <=0){
			 pBase->headerTable[i] =i+1;
			 pHeader = pHeader +i;
			 pHeader->id = i+1;//id is index+1
			 printf("asrbMem_AllocHeader(%d) returns %p\n", pHeader->id, pHeader);
			 return pHeader;
		 }
	 }
	 printf("asrbMem_AllocHeader(): no free header!\n");
	 return NULL;
}
/*free AsrbBufferHeader area */
/* get header pointer by index. -1 to get a new free one */
void asrbMem_FreeHeader(AsrbBufferHeader* pHeader)
{
	int i = pHeader->id -1;

	assert(i >=0 && i < MAX_ASRB_NUM);
	 AsrbPool* pBase = (AsrbPool*)g_pvMapBase;
	 pBase->headerTable[i] = 0;
}
int asrbPhy_Init()
{
    int fd;
    if (asrbPhy_Open() != ASRB_OK){
    	return ASRB_ERR_AsrbOpened;
    }
    AsrbPool* pBase = (AsrbPool*)g_pvMapBase;
    memset(pBase, 0, sizeof(AsrbPool));
    strncpy(pBase->tag, POOL_TAG, sizeof(POOL_TAG));
    pBase->size  = ASRB_MAX_POOL_SIZE;
    pBase->offsetHeader =sizeof(AsrbPool);
    pBase->offsetBuffers =pBase->offsetHeader + ASRB_HEADER_SIZE;

    printf("ASRB: pool initiated at %p-%p, 0x%x bytes\n", g_pvMapBase, g_pvMapBase+pBase->size, pBase->size);
    printf("- 0x%8x - %p MapBase - 0x%x bytes\n", 0, g_pvMapBase, pBase->offsetHeader);
    printf("- 0x%8x - %p Header  - 0x%x bytes\n", pBase->offsetHeader, g_pvMapBase+pBase->offsetHeader, (unsigned int)ASRB_HEADER_SIZE);
    printf("- 0x%8x - %p Buffer  - 0x%x bytes\n", pBase->offsetBuffers, g_pvMapBase+pBase->offsetBuffers, g_PhyBufferSize);

    return ASRB_OK;
}
/* called by reader only*/
int asrbPhy_Open()
{
    int fd;
    if (g_openCount >0){
    	return ASRB_ERR_AsrbOpened;
    }

    fd = open("/dev/mem", O_RDWR|O_SYNC);
    if (fd == -1)
    {
        fprintf(stderr, "failed to open device. err=%d %s\n", errno, strerror(errno));
        return (ASRB_ERR_OpenMmapFailed);
    }
    void* pBase = mmap((unsigned char*)NULL, ASRB_MAX_POOL_SIZE,
    		PROT_READ|PROT_WRITE, MAP_SHARED,fd, g_BasePhyAddress);
    if ( pBase == (void*)(-1)) {
    	fprintf(stderr, "mmap (0x%x bytes at 0x%lx) failed. %d %s\n",
    			(unsigned int) ASRB_MAX_POOL_SIZE, g_BasePhyAddress,
    			errno, strerror(errno));
        close (fd);
        return ASRB_ERR_OpenMmapFailed;
    }
    g_fdMem = fd;
    g_pvMapBase = (unsigned char*) pBase;
    printf("asrbPhy_Open OK. Phy 0x%lx maps %ld bytes to %p\n", g_BasePhyAddress, ASRB_MAX_POOL_SIZE, g_pvMapBase);
    g_openCount ++;
	return ASRB_OK;
}

void asrbPhy_Destroy()
{
	if (g_pvMapBase)
		munmap(g_pvMapBase, ASRB_MAX_POOL_SIZE);
	if (g_fdMem != -1)
		close(g_fdMem);
	printf("asrbPhy_Destroy\n");
	g_Ref = 0;
	g_fdMem = -1;
	g_pvMapBase = NULL;
}


/* called by writer only */
void* asrbMem_Get(int size)
{

    g_Ref++;
    return g_pvMapBase + ((AsrbPool*)g_pvMapBase)->offsetBuffers;
}
void asrbMem_Return(void* pBuffer)
{
	g_Ref --;

}
void DUMP_HEADERS(AsrbBufferHeader* pHeader)
{
	printf("Dump ASRB header at 0x%p, id=%d\n", pHeader, pHeader->id);
	printf(" %d buffers\n", pHeader->frameCounts);
	printf(" max frame size = %d bytes\n", pHeader->maxSize);
	printf(" frame type = %d \n", pHeader->frameType);
	for(int i=0; i<pHeader->frameCounts; i++) {
		printf("--frame %d:\n", i);
		DUMP_FRAME_INFO(&pHeader->info[i]);
	}
}
void DUMP_FRAME_INFO(AsrbFrameInfo* pInfo)
{
	if(pInfo) {
		//printf("\tlock state %d\n", IS_LOCKED(pInfo->lock));
		//WRITER_UNLOCK(pInfo->lock);
		printf("\tcounter=%d\n", pInfo->counter);
		printf("\tbuffer real size=%d\n", pInfo->size);
		printf("\twidth=%d\n", pInfo->width);
		printf("\theight=%d\n", pInfo->height);
		printf("\tphysical address=%p\n", pInfo->pDataPhy);
		char* pVirt = (char*) asrbMem_PhysicalToVirtual(pInfo->pDataPhy);
		if(pVirt[0]){
			printf("\t%s\n", pVirt);
		}

	}

}
