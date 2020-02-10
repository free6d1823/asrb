#include <stdio.h>
#include <unistd.h>
#include <memory.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <time.h>

#include <errno.h>
#include <arpa/inet.h>

#include "common.h"
int ConnectServer(char* ip, int port)
{
    printf("Connecting %s:%d...\n", ip, port);
    struct sockaddr_in serv_addr;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
       perror("ERROR opening socket");
       return sockfd;
    }

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if(inet_pton(AF_INET, ip, &serv_addr.sin_addr)<=0)
    {
        printf("\n inet_pton error occured\n");
        return 1;
    }

    /* Now connect to the server */
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
         perror("ERROR connecting\n");
        return -1;
    }

    return sockfd;
}
/*****************************************************************
 * Process client request commands
 * ***************************************************************/
int WriteCommand(int fd, char* text)
{
    int len = strlen(text);
    int n = write(fd, text, len);
    if (n < 0)
        perror("Error write socket!\n");
    return n;
}

int GetDir(int fd)
{
    char buffer[256];
    int n;
    sprintf(buffer, GET_DIR"\n");
    if (WriteCommand(fd, buffer)<0) {
       return -1;
    }
    n = read(fd, buffer, 255);
    if (n < 0) {
       perror("ERROR reading server\n");
       return -2;
    }
    buffer[n] = 0;
    printf("Current Folder:%s\n", buffer);
    return 0;
}
int PutFile(int fd, char* name)
{
    char buffer[256];
    int n;
    int len;
    FILE* fp;
    fp = fopen(name, "rb");
    if(!fp){
        fprintf(stderr, "Failed to open file %s.\n", name);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    sprintf(buffer, PUT_FILE"\n%s\n%d\n", name, len);
    if (WriteCommand(fd, buffer)<0) {
       return -1;
    }
    n = read(fd, buffer, 255);
    if (n < 0) {
       perror("ERROR reading server\n");
       fclose(fp);
       return -2;
    }
    buffer[n] = 0;
    if (strstr(buffer, "OK") == NULL)
    {
        perror("Put file server responses failed.\n");
        puts(buffer);
        fclose (fp);
        return -3;
    }
    int remain = len;
   while (remain >0) {
       int n = fread(buffer, 1, min(remain, 256), fp);
       if (n <= 0){
           perror("Failed to read file!\n");
           break;
       }

       int m = write(fd, buffer, n);
       if (m != n){
           perror("Faile to write socket\n");
           break;
       }
       remain -= n;
   }
   fclose(fp);
   n = read(fd, buffer, 255);
   if (n < 0) {
      perror("ERROR reading server\n");
      return -2;
   }
   buffer[n] = 0;
   printf("%s\n", buffer);
   return 0;
}

int RemoveFile(int fd, char* name)
{
    char buffer[256];
    int n;


    sprintf(buffer, RM_FILE"\n%s\n", name);
    if (WriteCommand(fd, buffer)<0) {
       return -1;
    }
    n = read(fd, buffer, 256);
    if (n <0) {
        perror("ERROR reading server\n");
        return -2;
    }
    buffer[n] = 0;
    puts(buffer);
    return 0;
}

int GetFile(int fd, char* name)
{
    char buffer[256];
    int n;
    int len;

    sprintf(buffer, GET_FILE"\n%s\n", name);
    if (WriteCommand(fd, buffer)<0) {
       return -1;
    }
    n = read(fd, buffer, 32);
    if (n < 0) {
       perror("ERROR reading server\n");
       return -2;
    }
    buffer[n]=0;

    char* pCmd = strtok(buffer, "\n");
    if (strcmp(pCmd, "OK") != 0)
    {
        puts(buffer);
        return -3;
    }
    char* p = strtok(NULL,"\n");
    if (!p){
        perror("Wrong protocol.\n");
        return -3;
    }
    len = atoi(p) ;
    FILE* fp;
    fp = fopen(name, "wb");
    if (!p) {
        fprintf(stderr, "Failed to open file %s\n", name);
        return -4;
    }
    int remain = len;

    //save remained payload to file
    char* p2 = p + strlen(p) + 1;

    int m = n - (p2 - buffer);
    if (m>0) {
        n = fwrite (p2, 1, m, fp);
        if (n < m) {
            perror("Failed to write file!!\n");
            fclose(fp);
            return -5;
        }
        remain -= m;
    }
    while (remain > 0){
        n = read(fd, buffer,  min(remain, 256));
        if (n < 0) {
            perror("read socket error!\n");
            break;
        }
        m = fwrite(buffer, 1, n, fp);
        if (m != n) {
            perror("Write file error!\n");
            break;
        }
        remain -= n;
    }
    printf("Get file %s OK %d bytes.\n", name, len);
    fclose(fp);
    return 0;
}

int GetChannel(int fd, int channel)
{
    char buffer[256];
    int n;
    int len;
    FILE* fp;
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    sprintf(buffer, "channel%d_%02d%02d%02d_%02d%02d%02d.dat", channel, tm.tm_year-100, tm.tm_mon+1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);
    fp = fopen(buffer, "wb");
    if (!fp) {
        fprintf(stderr, "Failed to open file %s\n", buffer);
        return -4;
    }

    sprintf(buffer, GET_CHANNEL"\n%d\n", channel);
    if (WriteCommand(fd, buffer)<0) {
        fclose(fp);
       return -1;
    }

    n = read(fd, buffer, 32);
    if (n < 0) {
       perror("ERROR reading server\n");
       fclose(fp);
       return -2;
    }
    char* pCmd = strtok(buffer, "\n");
    char* p = strtok(NULL,"\n");
    if (!p){
        perror("Wrong protocol.\n");
        fclose(fp);
        return -3;
    }
    if (strcmp(pCmd, "OK") != 0)
    {
        printf("%s: %s\n", pCmd, p);
        fclose(fp);
        return -3;
    }

    len = atoi(p) ;
printf("try to get %d bytes ..\n", len);

    int remain = len;

    //save remained payload to file
    char* p2 = p + strlen(p) + 1;
    int m = n - (p2 - buffer);
    if (m>0) {
        n = fwrite (p2, 1, m, fp);
        if (n < m) {
            perror("Failed to write file!!\n");
            fclose(fp);
            return -6;
        }
        remain -= m;
    }
    while (remain > 0){
        n = read(fd, buffer, min(remain, 256));
        if (n < 0) {
            perror("read socket error!\n");
            break;
        }
        m = fwrite(buffer, 1, n, fp);
        if (m != n) {
            perror("Write file error!\n");
            break;
        }
        remain -= n;
    }
    printf("Save channel %d OK %d bytes.\n", channel, len);
    fclose(fp);
    return 0;
}

int SetDir(int fd, char* folder)
{
    char buffer[256];
    int n;
    printf("SetCurrentFolder %s\n", folder);
    sprintf(buffer, SET_DIR"\n%s\n", folder);
    if (WriteCommand(fd, buffer)<0) {
       return -1;
    }
    n = read(fd, buffer, 255);
    if (n < 0) {
       perror("ERROR reading server\n");
       return -2;
    }
    buffer[n] = 0;
    printf("Result: %s\n", buffer);
    return 0;
}

int SendMessage(int fd, char* message)
{
    int n;
    char buffer[256];
    int len = strlen(message)+1;
    printf("SendMessage %s\n", message);
    sprintf(buffer, PUT_TEXT"\n%d\n", len);
    if (WriteCommand(fd, buffer)<0) {
       return -1;
    }
    n = read(fd, buffer, 255);
    if (n < 0) {
       perror("ERROR reading server\n");
       return -2;
    }
    if (strstr(buffer, "OK")==NULL) {
        fprintf(stderr, "Server response %s\n", buffer);
        return -3;
    }

    if (WriteCommand(fd, message)<0) {
       return -1;
    }
    n = read(fd, buffer, 255);
    if (n < 0) {
       perror("ERROR reading server 2\n");
       return -5;
    }
    if (strstr(buffer, "OK")==NULL) {
        fprintf(stderr, "Server response %s\n", buffer);
        return -6;
    }
    printf("Task done successfully.\n");
    return 0;
}

int ListDir(int fd)
{
    int n;
    char buffer[256];
    printf("ListFiles\n");
    sprintf(buffer, LIST_DIR"\n");
    if (WriteCommand(fd, buffer)<0) {
       return -1;
    }
    memset(buffer, 0, sizeof(buffer));
    //read response
    n = read(fd, buffer, 32);
    if (n<0)
    {
       perror("ERROR reading socket\n");
       return -1;
    }
    buffer[n] = 0;
    char* pResponse = strtok(buffer, "\n");
    if (strcmp (pResponse, "OK") !=0 ){
        printf("Server response: %s\n", buffer);
        return -1;
    }
    pResponse = strtok(NULL, "\n");
    if(!pResponse){
        printf("Wrong protocol!!\n");
        return -2;
    }
    int nLen = atoi(pResponse);
    printf("response %d bytes\n", nLen);
    strcpy(buffer, pResponse + strlen(pResponse)+1);
    printf("%s", buffer);
    do {
        n = read(fd, buffer, 255);
        if (n <=0)
            break;
        buffer[n]=0;
        printf("%s", buffer);
    }while (n == 255);
    printf("\nListFiles done\n");
    return 0;
}

/*****************************************************************
 * Client start here
 * ***************************************************************/
void RunClient(char* serverIp, int serverPort, int command, char* argument)
{
    int fd = ConnectServer(serverIp, serverPort);
    if (fd <0)
        return;
    //tdwrgl
    switch(command) {
        case 't':
        SendMessage(fd, argument);
        break;
    case 'l':
        ListDir(fd);
        break;
    case 'd':
        SetDir(fd, argument);
        break;
    case 'f':
        GetDir(fd);
        break;
    case 'w':
        PutFile(fd, argument);
        break;
    case 'r':
        GetFile(fd, argument);
        break;
        break;
    case 'm':
        RemoveFile(fd, argument);
        break;
    case 'g':
        GetChannel(fd, atoi(argument));
        break;
    default:
        break;
    }
   close(fd);
     return;
}
