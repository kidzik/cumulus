#include "transfer.h"
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <openssl/md5.h>

#define MIN(a,b) (((a)<(b))?(a):(b))

//! Outputs the error message and stops the app
/*!
\param msg the error message
*/
void error(const char *msg)
{
    perror(msg);
    exit(0);
}

//! Connects to the server at given ip and port
/*!
\param ip ip or hostname of the server
\param portno number of port
\return Returns 0 if succeeded
*/
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

//! Sends message of given length
/*!
\param socket socket
\param msg message
\param length message length
\return Returns 0 if succeeded
*/
int send_message(int socket, char* msg, int length){
  int f_block_sz = 0;
  f_block_sz = send(socket, (char*)msg, length, 0);
  if (f_block_sz < 0)
    error("ERROR: Failed to send message\n");
  printf("%d bytes sent\n", f_block_sz);
}

//! Recieves message of a given length
/*!
\param socket socket
\param msg message
\param length message length
\return Returns 0 if succeeded
*/
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

//! Sends file contents
/*!
\param socket socket
\param fp pointer of an opened file to send
\return Returns 0 if succeeded
*/
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

//! Recieves file contents
/*!
\param socket socket
\param path to save the file
\return Returns 0 if succeeded

TODO: Recieve full file before saving
*/
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

//! Computes chacksum of a given file
/*!
\param path file's path
\param sum points where to write the checksum
\return Returns 0 if succeeded

TODO: Recieve full file before saving
*/
int md5file(char* path, char* sum)
{
  FILE* fp = fopen(path,"rb");
  if(!fp)
    perror("No such file");
  MD5_CTX c;
  unsigned char digest[16];
  char buf[512];
  ssize_t bytes;

  MD5_Init(&c);
  bytes=fread(buf, 1, 512, fp);
  while(bytes > 0)
    {
      MD5_Update(&c, buf, bytes);
      bytes = fread(buf, 1, 512, fp);
    }

  MD5_Final(digest, &c);
  fclose(fp);

  char bb[CHECKSUM_LEN];
  for(int i = 0; i < CHECKSUM_LEN/2; ++i)
    sprintf(bb+i*2, "%02x", (unsigned int)digest[i]);
  strcpy(sum, bb);
  printf("%s\n",sum);
  return 0;
}

//! Reads file stats and saves to CUM_FILE structure
/*!
\param path file's path
\param cfile structure to write
\return Returns 0 if succeeded
*/
int get_file_desc(char* path, CUM_FILE* cfile)
{
  struct stat st;
  if (stat(path, &st ) == -1){
    printf("get_file_desc: file '%s' does not exist\n", path);
    return -1;
  }

  cfile->timestamp = st.st_atim.tv_sec;
  cfile->mode = st.st_mode;
  cfile->size = st.st_size;
  md5file(path, cfile->checksum);

  strcpy(cfile->path, path);
  return 0;
}

//! Sends file msg, header, recieves the accep and sends the contents
/*!
\param sockfd socket for sending
\param path file's path
\return Returns 0 if succeeded
*/
int send_file(int sockfd, char* path)
{
  int notafile = 0;
  FILE *fp = 0;
  fp = fopen(path, "rb");  
  if (fp == 0)
    notafile = 1;

  CUM_MSG cmsg;
  cmsg.id = MSG_FILE;
  cmsg.flags = 0;

  CUM_FILE cfile;
  if (get_file_desc(path, &cfile) == -1)
    return -1;

  if (S_ISDIR(cfile.mode))
  {
    printf("send_file: %s is a directory\n", path);
    // Directory
    cfile.size = 0;
  }
  else if (notafile){
    printf("ERROR: Not a dir, not a file.\n");
    return -1;
  }
  else if (S_ISREG(cfile.mode))
  {
    // Open file
    printf("Send %s...\n", path);

    if(fp == NULL)
      {
	printf("ERROR: File %s not found.\n", path);
	return -1;
      }
  }
  else
    return 0;
  
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

//! Recieves file. Run AFTER recieving the msg and file descriptor first
/*!
\param sockfd socket for sending
\param cfile file description
\return Returns 0 if succeeded
*/
int recieve_file(int sockfd, CUM_FILE *cfile)
{
  char path[BUFSIZE];

  sprintf(path, "%s", cfile->path);

  // do we have a newer file?
  CUM_MSG cmsg;
  cmsg.id = MSG_OK;
  send_message(sockfd, (char*)&cmsg, sizeof(CUM_MSG));

  if (cmsg.id == MSG_REFUSE)
    return 0;

  if (S_ISDIR(cfile->mode)){
    struct stat st = {0};
  
    if (stat(path, &st) == -1) {
      mkdir(path, 0700);
      printf("Directory %s created\n", cfile->path);
    }

  }
  else
    recieve_content(sockfd, path, cfile->size);
}

