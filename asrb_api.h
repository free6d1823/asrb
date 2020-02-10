#ifndef ASRB_H_
#define ASRB_H_

/* Error Code */
#define ASRB_OK	0
#define ASRB_ERR_OutOfMemory	-1
#define ASRB_ERR_OutOfPhyMem	-2
#define ASRB_ERR_InvalidParameters	-3
#define ASRB_ERR_CreateIpcError	-4
#define ASRB_ERR_OpenMmapFailed -5
#define ASRB_ERR_AsrbOpened		-6
#define ASRB_ERR_HeaderNotAllocated	-7
#define ASRB_ERR_FileNotFound	-8


/* ASRB API */
#ifndef HANDLE
#define HANDLE void*
#endif //HANDLE

typedef enum _ASRB_FRAME_TYPE{
	ASRB_FRAME_TYPE_RGB24 = 0,
	ASRB_FRAME_TYPE_RGB32 = 1,
	ASRB_FRAME_TYPE_YUV422 = 2,
	ASRB_FRAME_TYPE_COMPRESSED = 5
}ASRB_FRAME_TYPE;

void*  asrbGetBufferHeader(HANDLE handle, void* pBuffer);

/* Server/Writer  API */
typedef void (*asrbWriterCallback)(HANDLE* pHandle, int event, void* data);
typedef struct _asrbWriterConf{
    int  count;     /* buffer numbers */
    int size;       /* size n bytes of each frame */
    ASRB_FRAME_TYPE type; /*content of the frame */
    int width;
    int height;
    asrbWriterCallback fnCallback;
    //temp: base physical address should be managed and predefined. This is for experiment before SM Manager is implemented.
    long int basePhyAddress;
}AsrbWriterConf;
int gsmmInit(const char* conf);
void gsmmDump();
void gsmmDumpHeader(int id);
void gsmmRelease();

int asrbPhy_Open();
int    asrbWriter_Init(HANDLE* pHandle, char* name, AsrbWriterConf* pData);
void*  asrbWriter_GetBuffer(HANDLE handle);
void   asrbWriter_ReleaseBuffer(HANDLE handle, void* pBuffer);
void   asrbWriter_Free(HANDLE handle);

/* Client/Reader API */
typedef enum _ASRB_READER_STRATEGY{
	ASRB_STRATEGY_OLDER = 0,
	ASRB_STRATEGY_NEWER = 1,
}ASRB_READER_STRATEGY;

typedef struct _asrbReaderConf{
    unsigned int id;  /* id of reader */
    ASRB_READER_STRATEGY strategy;
    //temp: base physical address should be managed and predefined. This is for experiment before SM Manager is implemented.
    long int basePhyAddress;
    int  count;     /* buffer numbers */
    int size;       /* size n bytes of each frame */
}AsrbReaderConf;

int    asrbReader_Open(HANDLE* pHandle, char* name, AsrbReaderConf* pData);
void*  asrbReader_GetBuffer(HANDLE handle);
void   asrbReader_ReleaseBuffer(HANDLE handle, void* pBuffer);
void   asrbReader_Free(HANDLE handle);
#endif //ASRB_H_
