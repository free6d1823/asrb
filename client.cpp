#include <stdio.h>
#include <unistd.h>
#include <memory.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <netinet/in.h>
#include <time.h>

#include <errno.h>
#include <arpa/inet.h>

#include "common.h"
#include "asrb_int.h"
int ConnectServer(char* name, int id1, int id2)
{
    printf("Connecting server %s...\n", name);
	struct sockaddr_un addr;
	int ret;
	char clientName[256];
	struct sockaddr_un from;

    int fd = socket(PF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) {
       perror("ERROR opening socket");
       return fd;
    }

    sprintf(clientName, "%s-%d.%d", name, id1, id2);
    memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, clientName);
	unlink(clientName);

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("ERROR bind");
		close(fd);
		return -1;
	}

    /* Now connect to the server */
	memset(&from, 0, sizeof(from));
	from.sun_family = AF_UNIX;
	strcpy(from.sun_path, name);

    if (connect(fd, (struct sockaddr*)&from, sizeof(from)) < 0) {
         perror("ERROR connecting\n");
         close(fd);
        return -1;
    }

    return fd;
}
/*****************************************************************
 * Process client request commands
 * ***************************************************************/
int WriteText(int fd, char* text)
{
    int len = strlen(text);
    int n = send(fd, text, len+1, 0);
    if (n < 0)
        perror("Error write socket!\n");
    return n;
}

int SendMessage(int fd, char* message)
{
    int n;
    char buffer[256];

    sprintf(buffer, SEND_TEXT"\n%s\n", message);
    if (WriteText(fd, buffer)<0) {
       return -1;
    }
    n = recv(fd, buffer, 255, 0);
    if (n < 0) {
       perror("ERROR reading server\n");
       return -2;
    }
    if (strstr(buffer, "OK")==NULL) {
        fprintf(stderr, "Server response %s\n", buffer);
        return -3;
    }

    printf("Server response:%s\n", buffer);
    return 0;
}

int SendCommand(int fd, const char* cmd, int id, int value)
{
    int n;
    char buffer[256];

    sprintf(buffer, "%s\n%d\n%d\n", cmd, id, value);
    if (WriteText(fd, buffer)<0) {
       return -1;
    }
    n = recv(fd, buffer, 255, 0);
    if (n < 0) {
       perror("ERROR reading server\n");
       return -2;
    }
    if (strstr(buffer, "OK")==NULL) {
        fprintf(stderr, "Server response %s\n", buffer);
        return -3;
    }

    printf("Server response:%s\n", buffer);
    return 0;
}
/*****************************************************************
 * Client start here
 * ***************************************************************/
void RunClient(char* serverName, int idAsrb, int idRole, int command, void* argument)
{
    int fd = ConnectServer(serverName, idAsrb, idRole);
    if (fd <0)
        return;
    //tdwrgl
    switch(command) {
    case IPCCMD_SEND_TEXT:
        SendMessage(fd, (char*) argument);
        break;
    case IPCCMD_WRITER_JOIN:
    	SendCommand(fd, WRITER_IN, idAsrb, idRole);
        break;
    case IPCCMD_WRITER_LEAVE:
    	SendCommand(fd, WRITER_OUT, idAsrb, idRole);
        break;
    case IPCCMD_READER_JOIN:
    	SendCommand(fd, READER_IN, idAsrb, idRole);
        break;
    case IPCCMD_READER_LEAVE:
    	SendCommand(fd, READER_OUT, idAsrb, idRole);
        break;

    default:
        break;
    }
   close(fd);
     return;
}
