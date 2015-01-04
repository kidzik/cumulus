#include "transfer.h"

off_t fsize(char *file) {
    struct stat;
    if (stat(file, &stat) == 0) {
        return stat.st_size;
    }
    return 0;
}

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int connect(char *ip, int portno)
{
    int sockfd, n;

    struct sockaddr_in serv_addr;
    struct hostent *server;

    printf("Connecting to %s at %d", ip, portno);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    server = gethostbyname(ip);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");
}

int disconnect(int socket){
    close(socket);
}

int send_message(int socket, CUM_MSG* msg){
  if (send(socket, (char*)msg, sizeof(CUM_MSG), 0) < 0)
    error("ERROR: Failed to send message");
}

int recieve_message(int socket, CUM_MSG *msg)
{
  int f_block_sz = 0;
  int success = 0;
  while(success == 0)
    {
      while(f_block_sz = recv(socket, msg, sizeof(CUM_MSG), 0))
	if(f_block_sz < 0)
	  error("Receive msg error.\n");

      printf("ok!\n");
      success = 1;
    }
  return 0;
}

int send_content(int socket, char* path){
    // Open file
    char sdbuf[BUFSIZE]; // Send buffer
    printf("Send %s... ", path);
    FILE *fp = fopen(path, "r");
    if(fp == NULL)
      {
	printf("ERROR: File %s not found.\n", path);
	exit(1);
      }
    bzero(sdbuf, BUFSIZE);

    // Read file
    int f_block_sz;
    while((f_block_sz = fread(sdbuf, sizeof(char), BUFSIZE, fp))>0)
      {
	// Send file
	if(send(socket, sdbuf, f_block_sz, 0) < 0)
	  {
	    printf("ERROR: Failed to send file %s.\n", path);
	    break;
	  }
	bzero(sdbuf, BUFSIZE);
      }

    // Done
    printf("ok!\n");

    return 0;
}

int send_file(int sockfd, char* path){
  CUM_MSG cmsg;
  cmsg.id = MSG_FILE;
  send_message(sockfd, &cmsg, sizeof(CUM_MSG));

  CUM_FILE cfile;
  cfile.timestamp;
  cfile.checksum;
  cfile.flags;
  cfile.size = fsize(path);
  strcpy(path,cfile.path)
  send_message(sockfd, &cmsg, sizeof(CUM_FILE));

  // send new file msg
  // send file descriptor
  send_content(sockfd, path);
}

