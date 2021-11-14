#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <malloc.h>
#include <time.h>
#include <inttypes.h>
#include "../errlib.h"
#include "../sockwrap.h"
#define TIMEOUT 15
#define MAXQUEUE 15
#define BUFLEN 32
#define RECBUFLEN 100
#define MAXS 30
#define MSG_ERR "-ERR\r\n"
#define MSG_OK "+OK\r\n"
#define MSG_GET "GET"
#define MTU 2048
#define SCNu16 "hu"
#define SCNu32 "u"
#define MAX_ATTEMPTS 5

char *prog_name;

int main (int argc, char *argv[]){
	
  int s, n, i, fid, bufDim = MTU, attempt, flag = 1;
  struct in_addr server_address_IP;
  struct sockaddr_in server_address;
  uint16_t tport_h, tport_n;
  uint32_t fileSize_h, fileSize_n, lastMod_h, lastMod_n, read, tbread, timeS, timeE, timeDiff;
  char buf[BUFLEN], rbuf[RECBUFLEN], recbuf[MTU]/* *recbuf = malloc(sizeof(char)*MTU)*/, date[MAXS], c, *filename;	
  struct timeval tval;
  struct tm ts;
  time_t secs;
  fd_set cset;
  FILE* fp;
									/* saving prog_name */			
  prog_name = argv[0];
									/* arguments check */
  if(argc < 4)
    err_quit("usage: %s <dst host> <dst port> <filename1> ... <filenameN>\n", prog_name);
    									/* ip address conversion */
  if(!inet_aton(argv[1], &server_address_IP))
    err_quit("<%s> invalid IP address\n", prog_name);
									/* network byte order port conversion */
  if(sscanf(argv[2], "%" SCNu16, &tport_h) != 1)
    err_quit("<%s> invalid port\n", prog_name);
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
  server_address.sin_addr = server_address_IP;
									/* connect with error check */
  printf("<%s> trying to reach address %s:%" SCNu32 "...", prog_name, inet_ntoa(server_address.sin_addr), ntohs(server_address.sin_port));

  if((n = connect(s, (struct sockaddr*) &server_address, sizeof(server_address))) == -1){
    close(s);
    err_quit("failed\n");
  }
  printf("connected\n");
									/* initializing timeout */
  FD_ZERO(&cset);
  FD_SET(s, &cset);
  tval.tv_sec = TIMEOUT;
  tval.tv_usec = 0;
									/* files get requests loop */
  for(fid = 3; fid < argc; fid++){
    filename = argv[fid];
    memset(buf, 0, sizeof(buf));
    memset(rbuf, 0, sizeof(rbuf));
    sprintf(buf, "GET %s\r\n", filename);
    printf("\n<%s> sending GET request for file <%s>...", prog_name, filename);
    Send(s, buf, strlen(buf), 0);
    printf("sent\n");
									/* waiting for data to come (t.o.) */
    if((Select(FD_SETSIZE, &cset, 0, 0, &tval)) > 0){
      n = 0;
									/* saving initial msg sent by the server */
      do{
        Recv(s, &c, sizeof(char), 0);
        rbuf[n++] = c;
      } while( (n < (RECBUFLEN-1)) && (c != '\n') );
      buf[n] = '\0';
									/* if stored in rbuf, replacing '\n' and '\r' with '\0' */
      for(i = 0; i<n; i++){
        if(rbuf[i] == '\r' || rbuf[i] == '\n')
          rbuf[i] = '\0';
      }
      printf("<%s> server replied %s: ", prog_name, rbuf);
									/* case 1: server replied with OK_MSG */
      if(strncmp(rbuf, MSG_OK, strlen(MSG_OK)-2) == 0){
        printf("file %s is available and ready to be downloaded\n", filename);
									/* reading file size received */
        Recv(s, rbuf, 4, 0);
        fileSize_n = (*(uint32_t *) rbuf); 				
        fileSize_h = ntohl(fileSize_n);
        printf("<%s> size: [%" SCNu32 " bytes]\n", prog_name, fileSize_h);
									/* opening file */
        printf("<%s> creating file in work directory...", prog_name);
        if((fp = fopen(filename, "wb")) == NULL){
          close(s);
          err_quit("could not create file\n");
        }
        printf("done\n");
									/* writing file */
        tbread = fileSize_h;
        read = 0;
        timeS = time(NULL);
        while(read < fileSize_h){
          tbread = fileSize_h - read;
          if(tbread < bufDim){
            bufDim = bufDim/2;
            /*recbuf = realloc(recbuf, sizeof(char) * bufDim);*/
          }
          else{
            n = readn(s, recbuf, bufDim);
            if(n == 0){printf("\n<%s> client lost connection with server\n", prog_name); close(s); return -1;}
            read += n;
            fwrite(recbuf, sizeof(char), bufDim, fp);
            printf("<%s> %d/%d bytes downloaded\r", prog_name, read, fileSize_h);
          }
        }
        bufDim = MTU;
									/* computing download time */
        printf("\n");
        timeE = time(NULL);
        fclose(fp);
        timeDiff = timeE - timeS;
        printf("<%s> file has been successfully downloaded in ", prog_name);
        if(timeDiff < 1) printf("less than 1 second\n");
        else if(timeDiff == 1) printf("about 1 second\n");
        else printf("about %" SCNu32 " seconds\n", timeDiff);
									/* reading last modification received */
        Recv(s, rbuf, 4, 0);
        lastMod_n = (*(uint32_t *) rbuf);
        lastMod_h = ntohl(lastMod_n);
        secs = (time_t) lastMod_h;
        ts = *localtime(&secs);
        strftime(date, sizeof(date), "%a %Y-%m-%d %H:%M:%S %Z", &ts);
        printf("<%s> last modification: %s\n", prog_name, date);
      }
									/* case 2: server replied with ERR_MSG */
      else if(strncmp(rbuf, MSG_ERR, strlen(MSG_ERR)-2) == 0){
        printf("an error occurred\n");
        close(s);
      }
    }
    else{
      close(s);
      err_quit("timeout expired with no response received");
    }
  }  
  close(s);

  return 0;
}
