#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

#include "messages.h"

int send_file(int sockfd, char *path);
int recieve_message(int socket, char *msg, int length);
int send_message(int socket, char *msg, int length);
void error(const char *msg);
int connect(char *ip, int portno);
int recieve_file(int sockfd);
