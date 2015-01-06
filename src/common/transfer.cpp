#include "transfer.h"
#include <sys/stat.h>

#define MIN(a,b) (((a)<(b))?(a):(b))

off_t fsize(char *path) {
  struct stat st;
  stat(path, &st);
  return st.st_size;
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

    printf("Connecting to %s at %d\n", ip, portno);

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
    printf("Connected\n");
    return sockfd;
}

int disconnect(int socket){
    close(socket);
}

int send_message(int socket, char* msg, int length){
  int f_block_sz = 0;
  f_block_sz = send(socket, (char*)msg, length, 0);
  if (f_block_sz < 0)
    error("ERROR: Failed to send message\n");
  printf("%d bytes sent\n", f_block_sz);
}

int recieve_message(int socket, char* msg, int length)
{
  int f_block_sz = 0;
  while(1){
    f_block_sz = recv(socket, msg, length, 0);
    if(f_block_sz < 0)
      error("Receive msg error.\n");
    if(f_block_sz == length)
      break;
    if(f_block_sz > 0)
      error("Receive msg error (too little bytes).\n");
  }

  printf("%d bytes recieved\n", f_block_sz);
  return 0;
}

int send_content(int socket, char* path){
    // Open file
    char sdbuf[BUFSIZE]; // Send buffer
    printf("Send %s... ", path);
    FILE *fp = fopen(path, "rb");
    if(fp == NULL)
      {
	printf("ERROR: File %s not found.\n", path);
	exit(1);
      }
    bzero(sdbuf, BUFSIZE);

    // Read file
    int f_block_sz;
    int total_sent = 0;
    while((f_block_sz = fread(sdbuf, sizeof(char), BUFSIZE, fp))>0)
      {
	int sent = send(socket, sdbuf, f_block_sz, 0);
	// Send file
	if(sent < 0)
	  {
	    printf("ERROR: Failed to send file %s.\n", path);
	    break;
	  }
	total_sent += sent;
	printf("%d ", total_sent);
	bzero(sdbuf, BUFSIZE);
      }

    // Done
    printf("ok!\n");

    return 0;
}

int recieve_content(int socket, char* path, int size){
    // Open file
    char revbuf[BUFSIZE]; // Recieve buffer
    
    printf("Recieve %s... ", path);
    FILE *fp = fopen(path, "wb");
    if(fp == NULL)
      {
	printf("ERROR: Could not open file %s\n", path);
	exit(1);
      }
    bzero(revbuf, BUFSIZE);

    int f_block_sz;
    int recieved = 0;
    while(recieved < size)
      {
	f_block_sz = recv(socket, revbuf, MIN(BUFSIZE, size - recieved), 0);
	if(f_block_sz < 0)
	  {
	    printf("Receive file error.\n");
	    break;
	  }
	int write_sz = fwrite(revbuf, sizeof(char), f_block_sz, fp);
	if(write_sz < f_block_sz)
	  {
	    printf("File write failed.\n");
	    break;
	  }
	bzero(revbuf, BUFSIZE);
	recieved += f_block_sz;
	printf("%d ", recieved);	
      }
    printf("ok!\n");
    fclose(fp); 

    return 0;
}

int send_file(int sockfd, char* path){
  CUM_MSG cmsg;
  cmsg.id = MSG_FILE;
  send_message(sockfd, (char*)&cmsg, sizeof(CUM_MSG));

  CUM_FILE cfile;
  cfile.timestamp;
  cfile.checksum;
  cfile.flags;
  cfile.size = fsize(path);
  strcpy(cfile.path, path);
  printf("Sending %d bytes from %s...\n", cfile.size, cfile.path);
  send_message(sockfd, (char*)&cfile, sizeof(CUM_FILE));

  // send new file msg
  // send file descriptor
  send_content(sockfd, path);
}

int recieve_file(int sockfd){
  CUM_FILE cfile;
  char path[BUFSIZE];

  recieve_message(sockfd, (char*)&cfile, sizeof(CUM_FILE));
  printf("Downloading %d bytes to %s...\n", cfile.size, cfile.path);
  sprintf(path, "local/%s", cfile.path);

  recieve_content(sockfd, path, cfile.size);
}

