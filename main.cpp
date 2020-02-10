#include<stdio.h>  
#include<unistd.h>  
#include<stdlib.h>  
#include<signal.h>  
#include <errno.h>  
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "asrb_api.h"
/*jitter test */
int g_usPeriod = 30000;
int g_msMaxVar= 30; /* maximum variation between period */
long int g_nBasePhyAdd = 0x1000;
ASRB_READER_STRATEGY g_Strategy = ASRB_STRATEGY_OLDER;
static off_t hex2int(char *hex) 
{
    off_t val = 0;
    while (*hex&& *hex != ' ') {
        // get current character then increment
        unsigned char byte = *hex++; 
        if(byte == 'x'){
            val = 0;
            continue;
        }
        // transform hex character to the 4bit equivalent number, using the ascii table indexes
        if (byte >= '0' && byte <= '9') byte = byte - '0';
        else if (byte >= 'a' && byte <='f') byte = byte - 'a' + 10;
        else if (byte >= 'A' && byte <='F') byte = byte - 'A' + 10;    
        else
            break;
        // shift 4 to make space for new digit, and add the 4 bits of the new digit 
        val = (val << 4) | (byte & 0xF);
    }
    return val;
}
void printUsage(char* argv)
{
    printf("ASRB tester. Usage:\n");
    printf("%s [[-s] -n buffer_numbers -f frame_size | -c -d id -g stratege] [-t period] [-v variation] [-B phy_base] name\n", argv);
    printf("server/writer options:\n");
    printf("\t-s                   launch as writer/server\n");
    printf("\t-n buffer_numbers    numbers of buffers\n");
    printf("\t-f frame_size        bytes of each buffer\n");
    printf("client/reader options:\n");
    printf("\t-c                   launch as reader/client\n");
    printf("\t-d id                id of the reader (1~6)\n");
    printf("\t-g stratege          reader get frame stratege (default 0=older; 1=newer)\n");
    printf("    *The followings are required to be identical to server at experimental stage:\n");
    printf("\t-n buffer_numbers    numbers of buffers\n");
    printf("\t-f frame_size        bytes of each buffer\n");

    printf("common options:\n");
    printf("\t-t period            get buffer period in ms (1~1000)\n");
    printf("\t-v variation         maximum jitter in ms (1~1000)\n");
    printf("\t-B phy_base          <<TEMP>> base physical address, default 0x1000\n");
    printf("\t name                name to identify the shared memory\n\n"); 
    printf("examples:\n");
    printf("<server> sudo %s -B 0x20000 xyz\n", argv);
    printf("<client> sudo %s -c -B 0x20000 xyz\n", argv);
    printf("\n");
}
void testServer(char* name, int buffers, unsigned int size);
void testClient(char* name, int id, int buffers, unsigned int size);

bool gKillThreads = false;
void onSignalInt(int signum) 
{
    gKillThreads = true;
    printf("onSignalInt = %d\n", signum);
}
 


int main(int argc, char* argv[])  
{  
    int ch;  
    bool isServer = true;
    char* name;
    int buffers = 4;
    int size = 1024;
    int id = 1;
    int ret;
    signal(SIGINT, onSignalInt);
//    signal(SIGTERM, onSignalInt);


    ret = gsmmInit("/home/cj/asrb/bin/asrb.conf");
    if (ret != ASRB_OK) {
    	printf("Error = %d\n", ret);
    	exit(-1);
    }
    gsmmDump();
    gsmmRelease();
    exit(0);
    if (argc <2)
    {
        printUsage(argv[0]);
        exit(1);
    }
    while ((ch = getopt(argc, argv, "sn:f:cd:t:v:g:B:")) != -1)
    {
        printf("optind: %d - %s\n", optind, argv[optind]);
        switch (ch) 
        {
        case 'c':
            isServer = false; 
            break;
        case 'd':
            id = atoi(optarg);
            break;
        case 's':
            isServer = true;
            break;
        case 'n':
            buffers = atoi(optarg);
            break;
        case 'f':
            size = atoi(optarg);
            break;
        case 't': //period in ms
        	g_usPeriod = 1000*atoi(optarg);
        	break;
        case 'v': //variation in ms
        	g_msMaxVar = atoi(optarg);
        	break;
        case 'g':
            g_Strategy = (ASRB_READER_STRATEGY)atoi(optarg);
            break;
        case 'B':
            g_nBasePhyAdd = hex2int(optarg);
            break;
        case 'h':
        default:
            printUsage(argv[0]);
            exit(1);
        }
    }

    name = argv[optind];

    if (isServer) 
        testServer(name, buffers, size);
    else
        testClient(name, id, buffers, size);
    exit(0);  
}

void AsrbWriterCallback(HANDLE* pHandle, int event, void* data)
{
    printf("AsrbWriterCallback event=%x\n", event);
}

void testServer(char* name, int buffers, unsigned int size)
{
    srand((int)time(0));

    printf ("Test server %s %d %d\n", name, buffers, size);
    AsrbWriterConf asrb;
    asrb.count = buffers;
    asrb.size = size;
    asrb.type = ASRB_FRAME_TYPE_RGB24;
    asrb.width = 32;
    asrb.height = 32;
    asrb.fnCallback = AsrbWriterCallback;
    asrb.basePhyAddress = g_nBasePhyAdd;
    HANDLE handle;
    int err = asrbWriter_Init(&handle, name, &asrb);
    if (err != 0){
    	fprintf(stderr, "asrbWriter_Init error = %d\n", err);
    	return;
    }
    int n = 0;
    struct timeval tv;

    while (!gKillThreads) {
    	char* pBuffer = (char*) asrbWriter_GetBuffer(handle);

    	if(pBuffer){
    	    time_t t = time(NULL);
    	    struct tm *tp;
    	    tp = localtime(&t);
    		n++;
    		gettimeofday(&tv, NULL);
    		struct tm *localtime(const time_t *calptr);
    		memset(pBuffer, 0, size);
    		usleep(10000);
    		sprintf(pBuffer, "%d [%ld.%06ld] Date: %4d/%2d/%2d %d:%d:%d\n",
    				n, tv.tv_sec, tv.tv_usec,
    				tp->tm_year+1900, tp->tm_mon+1,tp->tm_mday,	tp->tm_hour,tp->tm_min,tp->tm_sec);
    		asrbWriter_ReleaseBuffer(handle, pBuffer);
    	}
    	long det = (rand()%1000) - 500;
        usleep(g_usPeriod + g_msMaxVar* det);
    }
    if(handle)
        asrbWriter_Free(handle);
    printf ("TestServer Terminated\n\n");
}
void testClient(char* name, int id, int buffers, unsigned int size)
{
	printf ("Test Client %s %d\n", name, id);
	srand((int)time(0));

	AsrbReaderConf asrb;
	asrb.id = id;
    asrb.strategy = g_Strategy;
    asrb.basePhyAddress = g_nBasePhyAdd;
    asrb.count = buffers;
    asrb.size = size;
	HANDLE handle;
	int err = asrbReader_Open(&handle, name, &asrb);
	if (err != 0){
		fprintf(stderr, "asrbReader_Open error = %d\n", err);
		return;
	}
	int n = 0;
	struct timeval tv;

	while (!gKillThreads) {
		char* pBuffer = (char*) asrbReader_GetBuffer(handle);

		if(pBuffer){
    	    time_t t = time(NULL);
    	    struct tm *tp;
    	    tp = localtime(&t);
    		n++;
    		gettimeofday(&tv, NULL);
    		struct tm *localtime(const time_t *calptr);
    		printf("R:%d [%16ld.%06ld] %d:%d:%d--(buffer)%s\n",
    				n, tv.tv_sec, tv.tv_usec,
    				tp->tm_hour,tp->tm_min,tp->tm_sec,
					pBuffer);

    		asrbReader_ReleaseBuffer(handle, pBuffer);
		}
		long det = (rand()%1000) - 500;
		usleep(g_usPeriod + g_msMaxVar* det);
	}
	if(handle)
		asrbReader_Free(handle);
	printf ("TestServer Terminated\n\n");
}
