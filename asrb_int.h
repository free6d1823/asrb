/* ASRB internal command utilities */
#ifndef ASRB_INT_H_
#define ASRB_INT_H_

#include "asrb_api.h"


/* IPC related API and variables */
int StartServer(char* serverName);
void KillServer();
void RunClient(char* serverName, int idAsrb, int idRole, int command, void* argument);
#define IPCCMD_SEND_TEXT	0x1000 /* message */
#define IPCCMD_WRITER_JOIN  0x1001 /* 0 */
#define IPCCMD_WRITER_LEAVE	0x1002 /* 0 */
#define IPCCMD_READER_JOIN	0x1003 /* readerID */
#define IPCCMD_READER_LEAVE	0x1004 /* readerID */

/*-- common type declarations --*/

typedef enum _AsrbRole {
	ASRB_ROLE_WRITER = 0,
	ASRB_ROLE_READER1 = 1,
	ASRB_ROLE_READER2 = 2,
	ASRB_ROLE_READER3 = 3,
	ASRB_ROLE_READER4 = 4,
}AsrbRole;

typedef enum _AsrbState {
	STATE_INIT = 0,		/*star up*/
	STATE_IPC = 1,		/*IPC is ready, but buffer pool is not allocated */
	STATE_READY = 2,	/*Buffers are ready */
	STATE_LOCKED = 3	/*one buffer is rent and locked */
}AsrbState;

/* GSMM memory structure */
#define GSMM_TAG "MMSG"
typedef struct _MasterRecordRegion {
	char tag[4];
	int  sizeKb; 					/*bytes of the pool */
	int headerCounts; 				/* numbers of header */
	unsigned int offsetHeader;		/* offset of buffers */
	unsigned int offsetBuffers; 	/* offset of buffers */
}MasterRecordRegion;

typedef struct _asrbCbBase{
	char name[64]; 		/* obsolated */
	int id;
	PHY_ADDR basePhyAddress;
	MasterRecordRegion* pMrr;
	PHY_ADDR phyBufferSize;
	int	fdMem; 								/*file descriptor of memory device */
	AsrbRole  role;							/* Writer or Reader		*/
	AsrbState state;						/* State of this module */
    AsrbBufferHeader* pHeader; 				/* pointer to virt address of header */
    VIR_ADDR  pData[MAX_FRAME_COUNTS];       /* virtual address of each frame data*/
    int currentCount;
    int currentState;
    /* IPC */
    char serverIp[64];

}AsrbCbBase;

typedef struct _writerCb{
	char name[64]; 		/* obsolated */
	int id;
	PHY_ADDR basePhyAddress;
	MasterRecordRegion* pMrr;
	PHY_ADDR phyBufferSize;
	int	fdMem; 								/*file descriptor of memory device */
	AsrbRole  role;							/* Writer or Reader		*/
	AsrbState state;						/* State of this module */
    AsrbBufferHeader* pHeader; 				/* pointer to virt address of header */
    VIR_ADDR  pData[MAX_FRAME_COUNTS];       /* virtual address of each frame data*/
    int currentCount;
    int currentState;
    /* IPC */
    char serverIp[64];

    /*---- writer specified data ----*/
    asrbWriterCallback fnCallback;


}WriterCb;

typedef void* (*GetBufferMethod)(HANDLE handle);

typedef struct _readerCb{
	char name[64]; 		/* obsolated */
	int id;
	PHY_ADDR basePhyAddress;
	MasterRecordRegion* pMrr;
	PHY_ADDR phyBufferSize;
	int	fdMem; 								/*file descriptor of memory device */
	AsrbRole  role;							/* Writer or Reader		*/
	AsrbState state;						/* State of this module */
    AsrbBufferHeader* pHeader; 				/* pointer to virt address of header */
    VIR_ADDR  pData[MAX_FRAME_COUNTS];       /* virtual address of each frame data*/
    int currentCount;
    int currentState;
    /* IPC */
    char serverIp[64];
    /*---- reader specified data ----*/
    ASRB_READER_STRATEGY strategy;
    GetBufferMethod fnGetBufferMethod;

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


int asrbMem_Init(HANDLE handle, const char* conf);
void asrbMem_Release(HANDLE handle);



#endif //ASRB
