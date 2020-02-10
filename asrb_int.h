/* ASRB internal command utilities */
#ifndef ASRB_INT_H_
#define ASRB_INT_H_
#define USE_LINUX

#include "asrb_api.h"
#ifdef USE_LINUX
#include <pthread.h>
#endif

/* IPC related API and variables */
#define IPC_SERVER_ID	0x0001
#define IPC_CLIENT_ID	0x0002
extern unsigned int g_kill_threads;	/* main thread to kill threads */
extern unsigned int g_live_threads; /* set 0 if thread killed */
int StartServer(int port, void* data);
void KillServer();
/*-- common type declarations --*/

typedef enum _AsrbRole {
	ASRB_ROLE_WRITER = 0,
	ASRB_ROLE_READER = 1,
}AsrbRole;

typedef enum _AsrbState {
	STATE_INIT = 0,		/*star up*/
	STATE_IPC = 1,		/*IPC is ready, but buffer pool is not allocated */
	STATE_READY = 2,	/*Buffers are ready */
	STATE_LOCKED = 3	/*one buffer is rent and locked */
}AsrbState;

#define MAX_ASRB_NUM	16 /*ISP-AVM*4, ISP-Encoder, ISP-Display, Decoder-Display*/
#define MAX_FRAME_COUNT	4 /* max frames in each buffers */

typedef struct _asrbFrameInfo{
#ifdef USE_LINUX
	pthread_mutex_t lock;
#else
    unsigned char lock;    /* buffer lock state */
#endif
    unsigned int counter;   /* buffer counter */
	unsigned int size; /* real size of data, filled by writer */
    unsigned int width;
    unsigned int height;
    void*		pDataPhy; /*start physical address of frame */
}AsrbFrameInfo;

typedef struct _asrbBufferHeader{
	int id; 			/* index of the header, 0~MAX_ASRB_NUM-1) */
	int	frameCounts;	/* numbers of frame buffers */
	int maxSize; 		/*max size of each frame */
	int align; 			/*aligned shift bits of frame start address */
	ASRB_FRAME_TYPE frameType;     /*frame type, RGB,YUV or compressed */
    AsrbFrameInfo info[MAX_FRAME_COUNT];/* each frame */
}AsrbBufferHeader;
/* physical address allocation 10000000 0x80000000 0xf0000000
 * Test in PC ubuntu
 * 0x20000 - 55sec; 0x80000 51 sec; 0x800000 28 sec; 0x8000000 29 sec; 0x40000 26 sec, 0x00 35 sec
 * */
extern  long int g_BasePhyAddress;
extern unsigned int g_PhyBufferSize;
extern int g_HeaderCounts;

#define ASRB_BASE_PHY_ADDRESS 0x1000
#define ASRB_HEADER_SIZE ((((MAX_ASRB_NUM*sizeof(AsrbBufferHeader))+15 )>>4)<<4)
#define ASRB_MAX_POOL_SIZE	(sizeof(AsrbPool)+ASRB_HEADER_SIZE+g_PhyBufferSize)/*temp 100x1K buffer*/

typedef struct _asrbCbBase{
	char name[64]; 		/* name of the Control block */
	AsrbRole  role;		/* Writer or Reader		*/
	AsrbState state;	/* State of this module */
    AsrbBufferHeader* pHeader; /* pointer to virt address of header */
    void*  pData[MAX_FRAME_COUNT];       /* virtual address of each frame data*/
    int currentCount;
    int currentState;
}AsrbCbBase;

typedef struct _writerCb{
    /* common part of the module */
	char name[64]; 		/* name of the Control block */
	AsrbRole  role;		/* Writer or Reader		*/
	AsrbState state;	/* State of this module */
    AsrbBufferHeader* pHeader; /* pointer to virt address of header */
    void*  pData[MAX_FRAME_COUNT];       /* virtual address of each frame */
    int currentCount;
    int currentState;
    /* writer specified data */
    asrbWriterCallback fnCallback;


}WriterCb;


typedef struct _readerCb{
    /* common part of the module */
	char name[64]; 		/* name of the Control block */
	AsrbRole  role;		/* Writer or Reader		*/
	AsrbState state;	/* State of this module */
    AsrbBufferHeader* pHeader; /* pointer to the first buffer header */
    void*  pData[MAX_FRAME_COUNT];       /* virtual address of each frame */
    int currentCount;
    int currentState;
    /* Reader specified data */
    int idReader;
    ASRB_READER_STRATEGY strategy;
}ReaderCb;

#ifdef USE_LINUX
void INIT_LOCK(pthread_mutex_t& st);
//#define IS_LOCKED(st) (pthread_mutex_trylock(&(st)) != 0)
#define IS_LOCKED(st) (pthread_mutex_trylock(&(st))

#define IS_UNLOCK(st) (pthread_mutex_trylock(&(st)) == 0)
#define WRITER_LOCK(st) ( pthread_mutex_lock(&(st)) )              /*set bit[7] & bit[0] */
#define WRITER_UNLOCK(st) (pthread_mutex_unlock(&(st)))             /*clear all bits but bit[0] */
#define READER_LOCK(st, n)  (pthread_mutex_lock(&(st)))   /*lock and sets reader bit */
#define READER_UNLOCK(st) (pthread_mutex_unlock(&(st)) )   /*unlock */
#define DEST_LOCK(st) (pthread_mutex_destroy(&(st)))
#else
#define INIT_LOCK(st) (st = 0)
#define IS_LOCKED(st) ( (st &0x80) != 0)               /*test bit[7] */
#define IS_UNLOCK(st) ( (st &0x80) == 0)
#define WRITER_LOCK(st) ( st |= 0x81 )              /*set bit[7] & bit[0] */
#define WRITER_UNLOCK(st) (st &= 0x01)             /*clear all bits but bit[0] */
#define READER_LOCK(st, n)  (st  |= ( ( (1<<n) | 0x80)))   /*lock and sets reader bit */
#define READER_UNLOCK(st) (st  &= 0x7F )   /*unlock */
#define DEST_LOCK(st) ()
#endif

int asrbPhy_Init();
int asrbPhy_Open();
void asrbPhy_Destroy();
/* get header pointer by index */
AsrbBufferHeader* asrbMem_GetHeader(int idHeader);
AsrbBufferHeader* asrbMem_AllocHeader();
/*free AsrbBufferHeader area */
void asrbMem_FreeHeader(AsrbBufferHeader* pHeader);

/*Physical memory managementAPI */
void* asrbMem_Get(int size);
void  asrbMem_Return(void* pBuffer);
/* convert pool offset to virtual address */
void* asrbMem_PhysicalToVirtual(void* pPyh);
/* convert virtual address to pool offset */
void* asrbMem_VirtualToPhysical(void* pVirt);

/*-- Writer specific declarations --*/


/*-- Reader specific declarations --*/

/*-- debug function --*/
void DUMP_HEADERS(AsrbBufferHeader* pHeader);
void DUMP_FRAME_INFO(AsrbFrameInfo* pInfo);
#endif //ASRB
