#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>
#include "../common/transfer.h"

#include <map>
#include <string>

using namespace std;

const string cumdir = "local";

typedef std::pair<string,string> spair;
typedef map<spair, string> mspairs;
typedef map<int, string> mis;
typedef mspairs::iterator imspairs;
typedef mis::iterator imis;

mspairs c_file_ver; 
mis clients;

//! Saves '<client, file> -> version' map
/*!
\param mspairs the map to serialise
\return Returns 0 if succeeded
*/
int save_map(mspairs* m)
{
  FILE* fp = fopen("map.txt","w");
  fprintf(fp,"%d\n",(int)m->size());

  for(imspairs iterator = m->begin(); iterator != m->end(); iterator++) {
    fprintf(fp,"%s %s %s\n", iterator->first.first.c_str(),
	    iterator->first.second.c_str(),
	    iterator->second.c_str());
  }
  fclose(fp);
  return 0;
}

//! Loads '<client, file> -> version' map
/*!
\param mspairs the map to load to
\return Returns 0 if succeeded
*/
int load_map(mspairs* m)
{
  FILE* fp = fopen("map.txt","r");
  if (fp == 0)
    return 0;

  char a[BUFSIZE],b[BUFSIZE],c[BUFSIZE];
  int n;

  fscanf(fp, "%d", &n);

  for (int i = 0; i<n; i++)
  {
    fscanf(fp,"%s %s %s\n", a, b, c);
    (*m)[spair(string(a),string(b))] = string(c);
  }
  fclose(fp);
}

//! Authenticates the client on given socket
/*!
\param socket the socket of the cilent
\return Returns 0 if succeeded
*/
int authenticate(int socket)
{
  printf("[server] authenticate...\n");
  CUM_AUTH cauth;
  CUM_MSG cmsg;
  cmsg.id = MSG_OK;
  recieve_message(socket, (char*)&cauth, sizeof(CUM_AUTH));
  clients[socket] = string(cauth.clientid);
  send_message(socket, (char*)&cmsg, sizeof(CUM_MSG));
  printf("[server] cilent %s authenticated...\n",cauth.clientid);
  return 0;

}

//! Updates versions of files in the map of server and client
/*!
\param socket the socket of the cilent
\param fullpath path of the file which was just updated
\return Returns 0 if succeeded

TODO: The save at each update is very very very inefficient,
but I am too lazy for this sort of coding for the moment
*/
int upade_versions(int socket, char* fullpath){
  CUM_FILE cfile;
  get_file_desc(fullpath, &cfile);

  c_file_ver[spair("server", string(fullpath))] = string((const char*)cfile.checksum);
  c_file_ver[spair(clients[socket], string(fullpath))] = string((const char*)cfile.checksum);
  save_map(&c_file_ver);
  return 0;
}

//! Sends the directories and files from given path to socket
/*!
\param socket the socket of the cilent
\param path path of the directory
\return Returns 0 if succeeded

TODO: Should be done only if the client does not have given file
*/
int send_dirs(int socket, const char* path)
{
  int res;
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
      if(DT_REG == ent->d_type || DT_DIR == ent->d_type){
	res = send_file(socket, fullpath);
	if (!res)
	  upade_versions(socket, fullpath);
      }
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

//! Synchronise the client
/*!
\param socket the socket of the cilent
\return Returns 0 if succeeded
*/
int synchronise(int socket)
{
  printf("[server] synchronise...\n");
  send_dirs(socket, cumdir.c_str());
  CUM_MSG cmsg;
  cmsg.id = MSG_OK;
  send_message(socket, (char*)&cmsg, sizeof(CUM_MSG));
}

//! Sends the file to other clients
/*!
\param mcls map socket -> clientid
\param mwecn map <clientid,path> -> version
\param fullpath file to send
\return Returns 0 if succeeded
*/
int send_to_others(mis* mcls, mspairs* mvers, char* fullpath)
{
  int res;
  for(imis iterator = mcls->begin(); iterator != mcls->end(); iterator++) {
    if (mvers->count(spair(iterator->second,fullpath)) == 0 ||
	(*mvers)[spair(iterator->second,fullpath)].compare((*mvers)[spair("server",fullpath)]) == 0)
      continue;
    res = send_file(iterator->first, fullpath);
    if (!res)
      upade_versions(iterator->first, fullpath);    
  }

  return 0;
}

//! Wait for messages from given process (run in a different thread)
/*!
\param socket socket to wait on
\return Returns 0 if succeeded
*/
int loop_messages(int socket)
{
  printf("[server] waiting for messages...\n");
  CUM_MSG cmsg;
  while (!recieve_message(socket, (char*)&cmsg, sizeof(CUM_MSG))){
    printf("id    = %d\nflags = %d\n", cmsg.id, cmsg.flags);
    if (cmsg.id == MSG_FILE){
      CUM_FILE cfile;

      recieve_message(socket, (char*)&cfile, sizeof(CUM_FILE));

      // if client changed the newest version or an old one 
      if (c_file_ver.count(spair(clients[socket], string(cfile.path))) > 0 &&
	  c_file_ver[spair(clients[socket], string(cfile.path))].compare(c_file_ver[spair("server", string(cfile.path))]) != 0){
	printf("Synchronisation error\n");
      }

      recieve_file(socket, &cfile);

      // remember that client has this version of the file
      c_file_ver[spair(clients[socket], string(cfile.path))] = string((const char*)cfile.checksum);

      // rememebr that server has this version of the file
      c_file_ver[spair("server", string(cfile.path))] = string((const char*)cfile.checksum);

      pid_t pid = fork();      

      if (pid < 0)
	break;
      else if (pid == 0) { /* send to others */
	send_to_others(&clients, &c_file_ver, cfile.path);
	return 0; 
      }
      else {
	continue;
      }             
      
      // TODO: Send to other clients
    }
  }
}

//! Listen for connections
/*!
\param sockfd socket to listen on
\return Returns 0 if ended without errors (child process ended)
*/
int cum_listen(int sockfd)
{
  socklen_t clilen;
  int result = 0;
  struct sockaddr_in cli_addr;

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
      close(sockfd); 
      authenticate(newsockfd);
      synchronise(newsockfd);
      loop_messages(newsockfd);
      break;
    }
    else {
      close(newsockfd);
    }             
  }
  return 0;
}

//! Initialise the server on the given port
/*!
\param portno port to listen on
\return Returns 0 if no errors
*/
int cum_init(int portno)
{
  // Setup the server
  int sockfd, newsockfd;
  char revbuf[256];
  struct sockaddr_in serv_addr;
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

  return(sockfd);
}

//! Initialise the server and wait for connections
/*!
\return Returns 0 if no errors
*/
int main()
{
  int sockfd = cum_init(PORT);

  load_map(&c_file_ver);

  cum_listen(sockfd);
  close(sockfd);

  printf("[server] connection lost.\n");

  return 0; 
}
