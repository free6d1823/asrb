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
int g_usPeriod = 100000;
int g_msMaxVar= 0; /* maximum variation between period */

ASRB_READER_STRATEGY g_Strategy = ASRB_STRATEGY_OLDER;

const char def_conf[] = "/home/cj/asrb/bin/asrb.conf";
char* conf_name = (char*) def_conf;
void printUsage(char* argv)
{
    printf("ASRB tester. Usage:\n");
    printf("%s [-s] |[-w width -h height | -c reader -d id -g stratege] [-t period] [-v variation] -f conf id\n", argv);
    printf("server/writer options:\n");
    printf("\t-s                   launch as ASRB manager server\n");
    printf("\t-w width			    width\n");
    printf("\t-h height        		height\n");

    printf("client/reader options:\n");
    printf("\t-c reader            launch as reader (1~4)\n");
    printf("\t-g stratege          reader get frame stratege (default 0=older; 1=newer)\n");

    printf("common options:\n");
    printf("\t-t period            get buffer period in ms (1~1000)\n");
    printf("\t-v variation         maximum jitter in ms (1~1000)\n");
    printf("\t-f conf		       configure file path\n");

    printf("\t id                  ID to identify the ASRB\n\n");
    printf("examples:\n");
    printf("<writer> sudo %s -w 10 -h 10 -f /home/cj/asrb/bin/asrb.conf 1\n", argv);
    printf("<reader> sudo %s -c 2 -f /home/cj/asrb/bin/asrb.conf 1\n", argv);
    printf("<server> sudo %s -s -f /home/cj/asrb/bin/asrb.conf 1\n", argv);
    printf("\n");
}

bool isServer = false;
bool isWriter = true;

bool gKillThreads = false;
void onSignalInt(int signum) 
{
    gKillThreads = true;
    printf("onSignalInt = %d\n", signum);
    if(isServer)
    	asrbMem_KillServer();
}
 

void AsrbWriterCallback(HANDLE* pHandle, int event, void* data)
{
    printf("AsrbWriterCallback event=%x\n", event);
}

void testWriter(char* conf, int id, int width, int height)
{
    srand((int)time(0));

    printf ("Test server %d \n", id);
    AsrbWriterConf asrb;
    asrb.conf = conf;
    asrb.id = id;
    asrb.width = width;
    asrb.height = height;
    asrb.fnCallback = AsrbWriterCallback;


    HANDLE handle;
    int err = asrbWriter_Init(&handle, &asrb);
    if (err != 0){
    	fprintf(stderr, "asrbWriter_Init error = %d\n", err);
    	return;
    }
    asrbMem_Dump(handle);
    int n = 0;
	struct timeval tv;
	struct timeval tvNext;

    while (!gKillThreads) {
		gettimeofday(&tv, NULL);
    	char* pBuffer = (char*) asrbWriter_GetBuffer(handle);

    	if(pBuffer){
    	    time_t t = time(NULL);
    	    struct tm *tp;
    	    tp = localtime(&t);
    		n++;

    		struct tm *localtime(const time_t *calptr);
    		memset(pBuffer, 0, 64);//simple guest length
    		usleep(10000);
    		sprintf(pBuffer, "%d [%ld.%06ld] Date: %4d/%2d/%2d %d:%d:%d\n",
    				n, tv.tv_sec, tv.tv_usec,
    				tp->tm_year+1900, tp->tm_mon+1,tp->tm_mday,	tp->tm_hour,tp->tm_min,tp->tm_sec);

    		AsrbFrameInfo*  pFrame = asrbMem_GetFrameInfoByBuffer(handle, pBuffer);
    		pFrame->size = strlen(pBuffer) +1;
    		asrbWriter_ReleaseBuffer(handle, pBuffer);
    		asrbMem_DumpFrameInfo(handle, asrbMem_GetFrameInfoByBuffer(handle, pBuffer));
    	}

		long detExpt = (rand()%1000) - 500;
		detExpt = g_usPeriod + g_msMaxVar* detExpt;
		gettimeofday(&tvNext, NULL);
		long det = (tvNext.tv_sec - tv.tv_sec)*1000000 + (tvNext.tv_usec - tv.tv_usec);
		detExpt = detExpt - det -250;
		usleep(detExpt);

    }
    if(handle)
        asrbWriter_Free(handle);
    printf ("testWriter Terminated\n\n");
}
void testReader(char* conf, int id, int reader)
{
	printf ("Test Client %s %d\n", conf, id);
	srand((int)time(0));

	AsrbReaderConf asrb;
	asrb.conf = conf;
	asrb.id = id;
    asrb.strategy = g_Strategy;
    asrb.idReader = reader;

	HANDLE handle;
	int err = asrbReader_Open(&handle, &asrb);
	if (err != 0){
		fprintf(stderr, "asrbReader_Open error = %d\n", err);
		return;
	}
	int n = 0;
	struct timeval tv;
	struct timeval tvNext;

	while (!gKillThreads) {
		gettimeofday(&tv, NULL);
		char* pBuffer = (char*) asrbReader_GetBuffer(handle);
   	    time_t t = time(NULL);
 	    struct tm *tp;
	    tp = localtime(&t);
		struct tm *localtime(const time_t *calptr);
		printf("[R:]%d [%ld.%06ld] now %d:%d:%d \n",
		    				n, tv.tv_sec, tv.tv_usec,
		    				tp->tm_hour,tp->tm_min,tp->tm_sec);
		if(pBuffer){
    		n++;
    		printf("[W:]%s\n", pBuffer);

    		asrbReader_ReleaseBuffer(handle, pBuffer);
		}else {
			printf("No buffer!\n");
		}
		long detExpt = (rand()%1000) - 500;
		detExpt = g_usPeriod + g_msMaxVar* detExpt;
		gettimeofday(&tvNext, NULL);
		long det = (tvNext.tv_sec - tv.tv_sec)*1000000 + (tvNext.tv_usec - tv.tv_usec);
		detExpt = detExpt - det - 250;
		if(detExp < 1000){
			printf("time")
		}
		usleep(detExpt);
	}
	if(handle)
		asrbReader_Free(handle);

	printf ("testReader Terminated\n\n");
}



int main(int argc, char* argv[])
{
    int ch;

    int width = 0;
    int height = 0;
    int id = 1;
    int reader=1;
    int ret;
    signal(SIGINT, onSignalInt);

//    signal(SIGTERM, onSignalInt);

    if (argc <2)
    {
        printUsage(argv[0]);
        exit(1);
    }
    while ((ch = getopt(argc, argv, "sw:h:f:c:t:v:g:?")) != -1)
    {
        switch (ch)
        {
        case 'c':
        	isWriter = false;
        	reader = atoi(optarg);
            break;
        case 's':
            isServer = true;
            break;
        case 'w':
        	width = atoi(optarg);
        	break;
        case 'h':
        	height = atoi(optarg);
        	break;
        case 'f':
        	conf_name = optarg;
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
        case '?':
        default:
            printUsage(argv[0]);
            exit(1);
        }
    }

    if(optind>=0)
   		id = atoi(argv[optind]);

    if (isServer){
    	asrbMem_StartServer(conf_name);
    }else if(isWriter)
        testWriter(conf_name, id, width, height);
    else
        testReader(conf_name, id, reader);
    exit(0);
}
