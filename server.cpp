#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <dirent.h>
#include <pthread.h>
#include "common.h"
#include "asrb_int.h"

static WriterCb* g_CurrentWriterCb = NULL;
static long		g_ServerFd = -1;
int ResponseOK(int fd, const char* comment)
{
    const char text[]= "OK\n";
    int n = write(fd,text, strlen(text));
    if (n < 0) {
        perror("ERROR writing to socket");
        return n;
    }
    if(comment){
        n = write(fd,comment, strlen(comment));
    }
    return n;
}
int ResponseFailed(int fd, const char* reason)
{
    char buffer[256];
    sprintf(buffer, "FAILED\n%s\n", reason);
    int n = write(fd,buffer, strlen(buffer));
    if (n < 0) {
        perror("ERROR writing to socket");
        return n;
    }
    return 0;
}


int HandleCommandText(int fd, char* arg)
{
    char buffer[256];
    int nLen = atoi(arg);
    if (nLen < 256)
        ResponseOK(fd, NULL);
    else {
        sprintf(buffer, "FAILED. Message too long, the maximum is %d characters.\n", (int) sizeof(buffer)-1);
        return ResponseFailed(fd, buffer);
    }

    //prepare to read message
    memset(buffer, 0, 256);
    int n = read(fd,buffer,255);
    if (n < 0) {
      perror("ERROR reading from socket");
    }
    buffer[n] = 0;
    printf("Client says: %s\n", buffer);
    ResponseOK(fd, NULL);
    return 0;
}

int HandleCommandInit(int fd)
{
    char buffer[512];

    int len = 512*100;
    int n = 0;
    if (!g_CurrentWriterCb) {
        sprintf(buffer, "FAILED. Writer not init yet.\n");
        return ResponseFailed(fd, buffer);
    } else {
    	ResponseOK(fd, buffer);
    	sprintf(buffer, "\n Header address = %p\n",  asrbMem_VirtualToPhysical(g_CurrentWriterCb->pHeader));
        int m = write(fd, buffer, 512);
        if (m < 0)
        {
            perror("HandleCommandInit: Write socket error!\n");
        }else
        {

        	printf("HandleCommandInit done\n");
	}
    }
    return 0;
}

void* ProcessClientCommand(void* data)
{
    int fd = *(int*) data;
    int n;
    char buffer[256];

    memset(buffer, 0, 256);
    n = read(fd,buffer,255);
    if (n<0) {
        printf("Read client message error!\n");
        return (void*) -1;
    }
    buffer[n] = 0;
    char* pCmd;
    char* pArg = NULL;
    char* pArg2 = NULL;

    pCmd = strtok(buffer, "\n");
    if (!pCmd){
        printf("Unrecognized client message %s\n", buffer);
        return (void*) -2;
    }
    pArg = strtok(NULL, "\n");
    if (pArg) {
        pArg2 = pArg + strlen(pArg) + 1;
    }
    printf("Client commands: %s", pCmd);
    if (pArg) {
        printf("- %s\n", pArg);
    } else {
        printf("\n");
    }
    if (strcmp(pCmd, PUT_TEXT) ==0 ){
        HandleCommandText(fd, pArg);
    } else if(strcmp(pCmd, READER_INIT) ==0 ) {
        HandleCommandInit(fd);
    }

    return (void*) 0;
}

int CreateServer(int port)
{
    struct sockaddr_in serv_addr;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        return -1;
    }
    /* Initialize socket structure */
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    printf("Bind to %d\n", port);
    /* Now bind the host address using bind() call.*/
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        return -1;
    }
    listen(sockfd,5);
    return sockfd;
}
int OnAccept(int fd)
{
    struct sockaddr_in cli_addr;
    socklen_t clilen= sizeof(cli_addr);
    int newsockfd = accept(fd, (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0) {
       perror("server accept abort\n");
    }
    return newsockfd;
}

void* RunServer(void* data)
{
    //set current folder as global, since cwd works for child thread only
	long fd = (long) data;
	g_live_threads |= IPC_SERVER_ID;
    while (g_kill_threads == 0) {
      int fdClient = OnAccept(fd);
      if (fdClient < 0){
          close(fd);
          break;
      }
      pthread_t thread;
      pthread_create(&thread, NULL, ProcessClientCommand, (void*) &fdClient);

    }
    g_live_threads = 0; /* force all other threads to stop */
    printf("Server terminated.\n");
    return (void*) 0;
}
int StartServer(int port, void* data)
{
	g_ServerFd = CreateServer(port);
    if (g_ServerFd <0){
    	fprintf(stderr, "Failed to create server, port(%d).\n", port);
    	return ASRB_ERR_CreateIpcError;
    }
    g_CurrentWriterCb = (WriterCb*)data;
    pthread_t thread;
    pthread_create(&thread, NULL, RunServer, (void*) g_ServerFd);

}
void KillServer()
{
	if (g_ServerFd >=0){
		shutdown(g_ServerFd, SHUT_RDWR);
		close(g_ServerFd);
	}
	g_ServerFd = -1;

}
