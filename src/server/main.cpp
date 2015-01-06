/* A simple server in the internet domain using TCP
   The port number is passed as an argument */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include "../common/transfer.h"

int loop_messages(int socket){
  printf("[server] waiting for messages...\n");
  CUM_MSG cmsg;
  while (!recieve_message(socket, (char*)&cmsg, sizeof(CUM_MSG))){
    printf("id    = %d\nflags = %d\n", cmsg.id, cmsg.flags);
    if (cmsg.id == MSG_FILE){
      recieve_file(socket);
    }
  }
}

int cum_listen(int portno){
     int sockfd, newsockfd;
     socklen_t clilen;
     char revbuf[256];
     struct sockaddr_in serv_addr, cli_addr;
     int n;
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");

     listen(sockfd,5);

     int result = 0;

     printf("[server] waiting for connections...\n");
     while (result == 0){
       clilen = sizeof(cli_addr);
       int newsockfd = accept(sockfd, 
			  (struct sockaddr *) &cli_addr, 
			  &clilen);
       if (newsockfd < 0) 
          error("ERROR on accept");

       pid_t pid = fork();
       printf("[server] connection opened...\n");
       if (pid < 0)
	 result = -1;
       else if (pid == 0) { /* client code */
	 close(sockfd); //the client doesn't need to listen
	 loop_messages(newsockfd);
	 return 0; //have the child leave the while loop -- probably not what I want
       }
       else {
	 close(newsockfd);
       }             
     }
     close(sockfd);
}

int main()
{
    cum_listen(PORT);

    printf("[server] connection lost.\n");

    return 0; 
}
