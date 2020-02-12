#ifndef ASRB_H_
#define ASRB_H_

#define USE_LINUX
#ifdef USE_LINUX
#include <pthread.h>
#endif

/* Error Code */
#define ASRB_OK	0
#define ASRB_ERR_OutOfMemory	-1
#define ASRB_ERR_OutOfPhyMem	-2
#define ASRB_ERR_InvalidParameters	-3
#define ASRB_ERR_CreateIpcError	-4
#define ASRB_ERR_OpenMmapFailed -5
#define ASRB_ERR_AsrbOpened		-6
#define ASRB_ERR_HeaderNotFound	-7
#define ASRB_ERR_FileNotFound	-8
#define ASRB_ERR_AsrbIsUsed		-9

#define MAX_FRAME_COUNTS	4 /* max frames in each buffers */

/* ASRB API */
#ifndef HANDLE
#define HANDLE void*
#endif //HANDLE

#define PHY_ADDR long int		/*physical address data type */
#define VIR_ADDR char*  		/*virtual address data type */

typedef enum _ASRB_FRAME_TYPE{
	ASRB_FRAME_TYPE_RGB24 = 0,
	ASRB_FRAME_TYPE_RGB32 = 1,
	ASRB_FRAME_TYPE_YUV422 = 2,
	ASRB_FRAME_TYPE_COMPRESSED = 5
}ASRB_FRAME_TYPE;


/* Memory pool structure */
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
    PHY_ADDR phyData; /*start physical address of frame */
}AsrbFrameInfo;

typedef struct _asrbBufferHeader{
	int state;			/* 1 if is used */
	int id; 			/* index of the header, 0~MAX_ASRB_NUM-1) */
	int	frameCounts;	/* numbers of frame buffers */
	int maxSize; 		/*max size of each frame */
	int align; 			/*aligned shift bits of frame start address */
	ASRB_FRAME_TYPE frameType;     /*frame type, RGB,YUV or compressed */
    AsrbFrameInfo info[MAX_FRAME_COUNTS];/* each frame */
}AsrbBufferHeader;

AsrbBufferHeader* asrbMem_FindHeaderPointer(HANDLE handle);
AsrbFrameInfo*  asrbMem_GetFrameInfoByBuffer(HANDLE handle, void* pBuffer);
VIR_ADDR asrbMem_PhysicalToVirtual(HANDLE handle, PHY_ADDR phy);
PHY_ADDR asrbMem_VirtualToPhysical(HANDLE handle, VIR_ADDR pVirt);

void asrbMem_Dump(HANDLE handle);
void asrbMem_DumpHeader(HANDLE handle);
void asrbMem_DumpFrameInfo(HANDLE handle, AsrbFrameInfo* pFrame);


/* Server/Writer  API */
typedef void (*asrbWriterCallback)(HANDLE* pHandle, int event, void* data);
typedef struct _asrbWriterConf{
    const char* conf;				/* path of ASRB configure file */
    int  id;    					/* ASRB id */
    asrbWriterCallback fnCallback;
    int width;
    int height;

}AsrbWriterConf;

int    asrbWriter_Init(HANDLE* pHandle, AsrbWriterConf* pData);
void*  asrbWriter_GetBuffer(HANDLE handle);
void   asrbWriter_ReleaseBuffer(HANDLE handle, void* pBuffer);
void   asrbWriter_Free(HANDLE handle);

/* Client/Reader API */
typedef enum _ASRB_READER_STRATEGY{
	ASRB_STRATEGY_OLDER = 0,
	ASRB_STRATEGY_NEWER = 1,
}ASRB_READER_STRATEGY;

typedef struct _asrbReaderConf{
    const char* conf;				/* path of ASRB configure file */
    int  id;    					/* ASRB id */
    ASRB_READER_STRATEGY strategy;
}AsrbReaderConf;

int    asrbReader_Open(HANDLE* pHandle, AsrbReaderConf* pData);
void*  asrbReader_GetBuffer(HANDLE handle);
void   asrbReader_ReleaseBuffer(HANDLE handle, void* pBuffer);
void   asrbReader_Free(HANDLE handle);
#endif //ASRB_H_
