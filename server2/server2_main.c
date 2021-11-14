#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <malloc.h>
#include <inttypes.h>
#include "../errlib.h"
#include "../sockwrap.h"
#define TIMEOUT 15
#define MAXQUEUE 15
#define BUFLEN 32
#define MAXS 30
#define MSG_ERR "-ERR\r\n"
#define MSG_OK "+OK\r\n"
#define MSG_GET "GET"
#define MTU 2048
#define SCNu16 "hu"
#define SCNu32 "u"

int serve_client(int s, char* ipaddr, uint16_t port);
int format_ok(char* string);
char *prog_name;

int main (int argc, char *argv[]){
  
  uint16_t tport_h, tport_n;
  int s, sConn, pid;
  struct sockaddr_in server_address, from;
  socklen_t addrlen = sizeof(addrlen);
									/* saving prog_name */
  prog_name = argv[0];
  									/* arguments check */
  if(argc != 2)
    err_quit("usage: %s <port> (with <port> decimal integer)\n", prog_name);
									/* network byte order port conversion */
  if(sscanf(argv[1], "%" SCNu16, &tport_h) != 1)
    err_quit("<%s> <port> parameter not valid (error code: %d)\n", prog_name, errno);
  tport_n = htons(tport_h);
									/* socket creation with error check */
  printf("<%s> creating socket...", prog_name);
  if((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
    err_quit("failed (error code = %d)\n", errno);
  printf("done. Socket fd = %d\n", s);
									/* sockaddr_in structure building */
  bzero(&server_address, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_port = tport_n;
  server_address.sin_addr.s_addr = INADDR_ANY;
									/* address binding with error check */
  printf("<%s> binding address to socket fd = %d...", prog_name, s);
  if(bind(s, (struct sockaddr*) &server_address, sizeof(server_address)) == -1){
    close(s);
    err_quit("failed (error code = %d)\n", errno);
  }
  printf("done.\n");
  									/* listen with error check */
  printf("<%s> preparig server for receiving connections...", prog_name);
  if(listen(s, MAXQUEUE) == -1){
    close(s);
    err_quit("failed (error code = %d)\n", errno);
  }
  printf("done.\n");
									/* main server loop */
  for(;;){
									/* 3 way handshake */
    printf("\n<%s> server listening on port %" SCNu16 "...", prog_name, tport_h);
    if((sConn = accept(s, (struct sockaddr*) &from, &addrlen)) == -1){
      close(sConn);
      err_quit("server failed in accepting new pending connection (error code: %d)\n", errno);
    }
    printf("new connection established with %s, connected fd = %d\n", inet_ntoa(from.sin_addr), s);
									/* creating new process */
    pid = fork();
    if(pid == -1) 							/* case 1 : fork failed */
      err_quit("<%s> server unable to create new process\n");
    else if(pid > 0) 							/* case 2: parent process */
      close(sConn);
    else{  								/* case 3: child process */
      printf("<%s> waiting for files requests from client...\n", prog_name);
      while(serve_client(sConn, inet_ntoa(from.sin_addr), ntohs(from.sin_port)) != -1){}
    }
  }
  close(s);
  return 0;
}

int serve_client(int s, char* ipaddr, uint16_t port){
  char rbuf[BUFLEN], filename[MAXS], sbuf[MTU] = "", c;
  int n, i, filesAvailable = 1, count = 0, bufDim = MTU;
  uint32_t fileSize_h, fileSize_n, lastMod_h, lastMod_n, sent = 0, tbsent;
  FILE* fp;
  struct stat fileInfo;
  									/* recv and control on recv */
  n = Recv(s, rbuf, BUFLEN-1, 0);
  if(n == -1){
    close(s);
    err_quit("function recv() failed (error code = %d)\n", errno);
  }	
  else if(n == 0){
    printf("<%s> (%s : %" SCNu16 ") client has performed an orderly shutdown\n", prog_name, ipaddr, port);
    close(s);
    return -1;
  }
    									/* recv and control on recv_CASE: data available */
  else{					
    rbuf[n] = '\0';
    printf("<%s> (%s : %" SCNu16 ") checking format of command received...", prog_name, ipaddr, port);
									/* format received control */
    if(format_ok(rbuf) == 0){
      Send(s, MSG_ERR, strlen(MSG_ERR), 0);
      close(s);
      err_quit("invalid data format received", prog_name);
    }
    printf("format is correct");
									/* replacing \r and \n with \0 to handle filename */
    for(i = 0; i<strlen(rbuf); i++){
      if(rbuf[i] == '\r' || rbuf[i] == '\n')
        rbuf[i] = '\0';
    }
    sscanf(rbuf, "GET %s", filename);
    printf("\n<%s> (%s : %" SCNu16 ") client asked for <%s>\n", prog_name, ipaddr, port, filename);
    printf("<%s> (%s : %" SCNu16 ") verifying wether the file exists or not...", prog_name, ipaddr, port);
									/* existing file control */
    filename[strlen(filename)] = '\0';
    if((fp = fopen(filename, "r")) == NULL){
      Send(s, MSG_ERR, strlen(MSG_ERR), 0);
      close(s);
      err_quit("file specified by client not existing (error code = %d)\n", errno);
    }
    printf("file <%s> found in work directory\n", filename);
      									/* preparing stats */
    stat(filename, &fileInfo);
    fileSize_h = fileInfo.st_size;
    fileSize_n = htonl(fileSize_h);
    lastMod_h = fileInfo.st_mtime;
    lastMod_n = htonl(lastMod_h);
                                                                        /* sending OK and control */
    printf("<%s> (%s : %" SCNu16 ") sending OK...", prog_name, ipaddr, port);
    if(send(s, MSG_OK, strlen(MSG_OK), 0) == -1){
      close(s);
      err_quit("function send() failed (error code = %d)\n", errno);
    }
    printf("sent\n");
									/* sending fileSize and control */
    printf("<%s> (%s : %" SCNu16 ") sending file size [%" SCNu32 " bytes] ...", prog_name, ipaddr, port, fileSize_h);
    if(send(s, &fileSize_n, sizeof(fileSize_n), 0) == -1){
      close(s);
      err_quit("function send() failed (error code = %d)\n", errno);
    }
    printf("sent\n");
									/* sending file in binary */
    printf("<%s> (%s : %" SCNu16 ") sending file...\n", prog_name, ipaddr, port);
    while(sent < fileSize_h){
      tbsent = fileSize_h - sent;
      if( tbsent < bufDim )
        bufDim = bufDim/2;
      else{
        fread(sbuf, sizeof(char), bufDim, fp);
        sent += send(s, sbuf, bufDim, 0);
      }
    }
    bufDim = MTU;
    fclose(fp);
    sent = 0;
    memset(sbuf, 0, sizeof(sbuf)); 
									/* sending lastMod and control */
    printf("<%s> (%s : %" SCNu16 ") sending last modification...", prog_name, ipaddr, port);
    if(send(s, &lastMod_n, sizeof(lastMod_n), 0) == -1){
      close(s);
      err_quit("function send() failed (error code = %d)\n", errno);
    }
    printf("sent\n\n");
    return 1;
  }
}

int format_ok(char* string){
  int i;
  if(strncmp(string, MSG_GET, strlen(MSG_GET)) != 0) return 0;
  if(string[3] != ' ') return 0;
  for(i = 4; (string[i] != '\r') && (i < strlen(string)); i++){
    if(i == 4 && (string[i] == '/' || string[i] == '.')) return 0;
    if(string[i] == '/') return 0;
  }
  if(i == strlen(string)) return 0;
  if(string[++i] != '\n') return 0;

  return 1;
}
