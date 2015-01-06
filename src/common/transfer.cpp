#include "transfer.h"
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <openssl/md5.h>

#define MIN(a,b) (((a)<(b))?(a):(b))

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

int send_content(int socket, FILE* fp)
{
    char sdbuf[BUFSIZE]; // Send buffer
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
	    printf("ERROR: Failed to send file.\n");
	    break;
	  }
	total_sent += sent;
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
      }
    printf("ok!\n");
    fclose(fp); 

    return 0;
}

int md5file(char* path, unsigned char* sum)
{
  MD5_CTX c;
  char buf[512];
  ssize_t bytes;

  MD5_Init(&c);
  bytes=read(STDIN_FILENO, buf, 512);
  while(bytes > 0)
    {
      MD5_Update(&c, buf, bytes);
      bytes=read(STDIN_FILENO, buf, 512);
    }

  MD5_Final(sum, &c);
  return 0;
}

int send_file(int sockfd, char* path)
{
  FILE *fp = 0;

  CUM_MSG cmsg;
  cmsg.id = MSG_FILE;

  CUM_FILE cfile;
  struct stat st;
  stat(path, &st);

  cfile.timestamp = st.st_atim.tv_sec;
  cfile.mode = st.st_mode;
  cfile.size = st.st_size;
  md5file(path, cfile.checksum);

  strcpy(cfile.path, path);

  if (S_ISDIR(cfile.mode))
  {
    // Directory
    cfile.size = 0;
  }
  else 
  {
    // Open file
    printf("Send %s... ", path);
    fp = fopen(path, "rb");
    if(fp == NULL)
      {
	printf("ERROR: File %s not found.\n", path);
	return -1;
      }
  }
  printf("Sending %d bytes from %s...\n", cfile.size, cfile.path);

  // send new file msg
  send_message(sockfd, (char*)&cmsg, sizeof(CUM_MSG));

  // send file descriptor
  send_message(sockfd, (char*)&cfile, sizeof(CUM_FILE));

  // does he want it?
  recieve_message(sockfd, (char*)&cmsg, sizeof(CUM_MSG));

  // send contents
  if (fp && cmsg.id != MSG_REFUSE){
    send_content(sockfd, fp);
    fclose(fp);
  }
}

int recieve_file(int sockfd)
{
  CUM_FILE cfile;
  char path[BUFSIZE];

  recieve_message(sockfd, (char*)&cfile, sizeof(CUM_FILE));
  printf("Downloading %d bytes to %s...\n", cfile.size, cfile.path);
  sprintf(path, "local/%s", cfile.path);

  // do we have a newer file?
  CUM_MSG cmsg;
  cmsg.id = MSG_OK;
  send_message(sockfd, (char*)&cmsg, sizeof(CUM_MSG));

  if (cmsg.id == MSG_REFUSE)
    return 0;

  if (S_ISDIR(cfile.mode)){
    struct stat st = {0};
  
    if (stat(path, &st) == -1) {
      mkdir(path, 0700);
      printf("Directory %s created\n", cfile.path);
    }

  }
  else
    recieve_content(sockfd, path, cfile.size);
}

int send_dirs(int socket, const char* path)
{
  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir (path)) != NULL) {
    /* add all subdirectories */
    while ((ent = readdir (dir)) != NULL) {
      char fullpath[MAX_PATH];
      sprintf(fullpath, "%s/%s", path, ent->d_name);

      printf ("%s %d\n", fullpath, ent->d_type);
      if (!strcmp(".",ent->d_name) || !strcmp("..",ent->d_name))
	continue;
      if(DT_REG == ent->d_type || DT_DIR == ent->d_type)
	send_file(socket, fullpath);
      if(DT_DIR == ent->d_type)
	send_dirs(socket, fullpath);
    }
    closedir (dir);
  } else {
    /* could not open directory */
    perror ("");
    return EXIT_FAILURE;
  }
  return 0;
}
