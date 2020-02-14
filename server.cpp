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
#include <sys/socket.h>
#include <sys/un.h>
#include "common.h"
#include "asrb_int.h"

static long		g_ServerFd = -1;
static int g_kill_threads;
static int g_live_threads;

int ResponseOK(int fd, const char* comment, struct sockaddr * from, socklen_t fromlen)
{
    const char text[]= "OK\n";
	int n = sendto(fd, text, strlen(text)+1, 0, from, fromlen);
    if (n < 0) {
        perror("ERROR writing to socket");
        return n;
    }
    if(comment){
        n = sendto(fd,comment, strlen(comment)+1, 0, from, fromlen);
    }
    return n;
}
int ResponseFailed(int fd, const char* reason, struct sockaddr * from, socklen_t fromlen)
{
    char buffer[256];
    sprintf(buffer, "FAILED\n%s\n", reason);
    int n = sendto(fd, buffer, strlen(buffer)+1, 0, from, fromlen);
    if (n < 0) {
        perror("ERROR writing to socket");
        return n;
    }
    return 0;
}


int HandleCommandText(int fd, char* arg, struct sockaddr * from, socklen_t fromlen)
{
    printf("Client says: %s\n", arg);

    ResponseOK(fd, arg, from, fromlen);
    return 0;
}

int usage_counts[4][5] = {0};

int HandleCommandWriterIn(int fd, int id, int role, struct sockaddr * from, socklen_t fromlen)
{
    char buffer[256];
    if(id < 4 && role ==0) {
    	usage_counts[id][0] ++;
    	sprintf(buffer, "ASRB %d - %d writers.", id, usage_counts[id][0]);
    	printf("%s\n", buffer);
    	ResponseOK(fd, buffer, from, fromlen);
    } else {
    	sprintf(buffer, "Invalid ID numbers %d - %d", id, role);
    	ResponseFailed(fd, buffer, from, fromlen);
    }
    return 0;
}
int HandleCommandWriterOut(int fd, int id, int role, struct sockaddr * from, socklen_t fromlen)
{
    char buffer[256];
    if(id < 4 && role ==0) {
    	usage_counts[id][0] --;
    	sprintf(buffer, "ASRB %d : %d writers.", id, usage_counts[id][0]);
    	printf("%s\n", buffer);
    	ResponseOK(fd, buffer, from, fromlen);
    } else {
    	sprintf(buffer, "Invalid ID numbers %d - %d", id, role);
    	ResponseFailed(fd, buffer, from, fromlen);
    }
    return 0;
}
int HandleCommandReaderIn(int fd, int id, int role, struct sockaddr * from, socklen_t fromlen)
{
    char buffer[256];
    if(id < 4 && role <5) {
    	usage_counts[id][role] ++;
    	sprintf(buffer, "ASRB %d - %d reader: %d.", id, role, usage_counts[id][role]);
    	printf("%s\n", buffer);
    	ResponseOK(fd, buffer, from, fromlen);
    } else {
    	sprintf(buffer, "Invalid ID numbers %d - %d", id, role);
    	ResponseFailed(fd, buffer, from, fromlen);
    }
}
int HandleCommandReaderOut(int fd, int id, int role, struct sockaddr * from, socklen_t fromlen)
{
    char buffer[256];
    if(id < 4 && role <5) {
    	usage_counts[id][role] --;
    	sprintf(buffer, "ASRB %d - %d reader: %d.", id, role, usage_counts[id][role]);
    	printf("%s\n", buffer);
    	ResponseOK(fd, buffer, from, fromlen);
    } else {
    	sprintf(buffer, "Invalid ID numbers %d - %d", id, role);
    	ResponseFailed(fd, buffer, from, fromlen);
    }
}

int ProcessClientCommand(int fd)
{

    int n;
    char buffer[256];
    struct sockaddr_un from;
    socklen_t fromlen = sizeof(from);

    memset(buffer, 0, 256);
    n = recvfrom(fd,buffer,255, 0, (struct sockaddr *)&from, &fromlen);
    if (n<0) {
        printf("Read client message error!\n");
        return  -1;
    }
    buffer[n] = 0;

    char* pCmd;
    char* pArg = NULL;
    char* pArg2 = NULL;

    pCmd = strtok(buffer, "\n");
    if (!pCmd){
        printf("Unrecognized client message %s\n", buffer);
        return -2;
    }

    pArg = strtok(NULL, "\n");
    if (pArg) {
        pArg2 = pArg + strlen(pArg) + 1;
    }
    printf("Client: %s", pCmd);
    if (pArg) {
        printf("- %s\n", pArg);
    } else {
        printf("\n");
    }

    if (strcmp(pCmd, SEND_TEXT) ==0 ){
        HandleCommandText(fd, pArg, (struct sockaddr *)&from, fromlen);
    } else if(strcmp(pCmd, WRITER_IN) ==0 ) {
        HandleCommandWriterIn(fd, atoi(pArg), atoi(pArg2), (struct sockaddr *)&from, fromlen);
    } else if(strcmp(pCmd, WRITER_OUT) ==0 ) {
        HandleCommandWriterOut(fd, atoi(pArg), atoi(pArg2), (struct sockaddr *)&from, fromlen);
    } else if(strcmp(pCmd, READER_IN) ==0 ) {
        HandleCommandReaderIn(fd, atoi(pArg), atoi(pArg2), (struct sockaddr *)&from, fromlen);
    } else if(strcmp(pCmd, READER_OUT) ==0 ) {
        HandleCommandReaderOut(fd, atoi(pArg), atoi(pArg2), (struct sockaddr *)&from, fromlen);
    }

    return  0;
}

int CreateServer(char* name)
{
    struct sockaddr_un serv_addr;
    int sockfd = socket(PF_UNIX, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        return -1;
    }
    /* Initialize socket structure */
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, name);
    unlink(name);

    printf("Server bind to %s\n", name);
    /* Now bind the host address using bind() call.*/
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        return -1;
    }

    return sockfd;
}

int StartServer(char* serverName)
{

    g_kill_threads = 0;
    g_live_threads = 0;


	g_ServerFd = CreateServer(serverName);
    if (g_ServerFd <0){
    	fprintf(stderr, "Failed to create server %s.\n", serverName);
    	return ASRB_ERR_CreateIpcError;
    }

    //set current folder as global, since cwd works for child thread only
	g_live_threads =1;

	while(g_kill_threads ==0) {
		if (0 != ProcessClientCommand(g_ServerFd))
			break;
	}
	if(g_ServerFd)
		close(g_ServerFd);
    g_live_threads = 0; /* force all other threads to stop */
    printf("Server terminated.\n");
    return ASRB_OK;

}
void KillServer()
{
	g_kill_threads = 1;
	if (g_ServerFd >=0){
		shutdown(g_ServerFd, SHUT_RDWR);
		close(g_ServerFd);
	}
	g_ServerFd = -1;

}
