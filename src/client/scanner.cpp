#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <dirent.h>

#include <map>
#include <string>

#include "../common/transfer.h"

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

using namespace std;

map<string, int> wds;
map<int, string> wdpaths;
int server_socket;

const string cumdir = "local";

//! Generates a random string of given length
/*!
\param str string to write the result to
\param length length of the random string
\return Returns 0 if succeeded
*/
int random_string(char *str, const int length)
{
  char alphabeth[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  for (int i = 0; i < length; ++i)
    str[i] = alphabeth[rand() % (sizeof(alphabeth) - 1)];
  str[length] = 0;
  return 0;
}

//! Authenticates the client on given socket
/*!
\param socket server socket
\param clientid clientid
\return Returns 0 if succeeded

TODO: will be changed to id, login, pass
*/
int authenticate(int socket, char* clientid)
{
  printf("[client] authenticate...\n");
  CUM_AUTH cauth;
  strcpy(cauth.clientid, clientid);
  CUM_MSG cmsg;
  send_message(socket, (char*)&cauth, sizeof(CUM_AUTH));
  recieve_message(socket, (char*)&cmsg, sizeof(CUM_MSG));
  if (cmsg.id != MSG_OK){
    perror("Could not log in");
    exit(0);
  }
  return 0;
}

//! Synchronises data with the server
/*!
\param socket server socket
\return Returns 0 if succeeded

TODO:
1) Should recieve previous checksum to see if the file was not changed in the meanwhile
*/
int synchronise(int socket)
{
  printf("[client] synchronise...\n");
  CUM_MSG cmsg;
  while (!recieve_message(socket, (char*)&cmsg, sizeof(CUM_MSG))){
    printf("id    = %d\nflags = %d\n", cmsg.id, cmsg.flags);
    if (cmsg.id == MSG_FILE){
      CUM_FILE cfile;
      recieve_message(socket, (char*)&cfile, sizeof(CUM_FILE));
      CUM_MSG cresp;
      cresp.id = MSG_OK;
      send_message(socket, (char*)&cresp, sizeof(CUM_MSG));
      
      recieve_file(socket, &cfile);

    }
    if (cmsg.id == MSG_OK)
      break;
  }
  printf("[client] synchronised...\n");
}

//! Adds inotify to the directory and subdirectories and sends all the files in the tree
/*!
\param path directory to be scanned
\param file descriptor of the inotify
\return Returns 0 if succeeded
*/
int scan_dirs(const char* path, int fd)
{
  int wd;
  printf("[client] iwatch added");
  wd = inotify_add_watch( fd, path, IN_CREATE | IN_DELETE );
  wds[path] = wd;
  wdpaths[wd] = path;

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
	send_file(server_socket, fullpath);
      if(DT_DIR == ent->d_type)
	scan_dirs(fullpath, fd);
    }
    closedir (dir);
  } else {
    /* could not open directory */
    perror ("");
    return EXIT_FAILURE;
  }
  return 0;
}

//! Handles inotify event and sends data to the server
/*!
\param event inotify event to handle
\param fd file descriptor of inotify
*/
void handle_event ( struct inotify_event *event, int fd ) {
  char fullpath[MAX_PATH]; 
  sprintf(fullpath, "%s/%s", wdpaths[event->wd].c_str(), event->name);

  if ( event->mask & IN_CREATE ) {
    if ( event->mask & IN_ISDIR ) {
      printf( "New directory %s created.\n", fullpath );
      scan_dirs(fullpath, fd);
      send_file(server_socket, fullpath);
    }
    else {
      printf( "New file %s created.\n", fullpath );
      send_file(server_socket, fullpath);
    }
  }
  else if ( event->mask & IN_DELETE ) {
    if ( event->mask & IN_ISDIR ) {
      printf( "Directory %s deleted.\n", fullpath );
      wds.erase(fullpath);
      wdpaths.erase(event->wd);
    }
    else {
      printf( "File %s deleted.\n", fullpath );
    }
  }
}

//! Reads the clients unique id. If does not exist, creates one
/*!
\param cid points where to write the id
\return Returns 0 if succeeded
*/
int identify_client(char* cid){
  FILE *fp = fopen(".cumulus","r");
  if (!fp){
    FILE *fp = fopen(".cumulus","w");
    random_string(cid, CLIENTID_LEN);
    fwrite(cid, 1, CLIENTID_LEN, fp);
    fclose(fp);
    return 0;
  }

  fread(cid, 1, CLIENTID_LEN, fp);
  fclose(fp);
  return 0;
  
}

//! The main loop, recieves events from inotify
/*!
\return Returns 0 if succeeded
*/
int main( )
{
  char clientid[CLIENTID_LEN];
  identify_client(clientid);
  printf("[client] Starting client %s... \n", clientid);

  server_socket = connect((char*)"127.0.0.1", PORT); 

  int length, i = 0;
  int fd;
  int wd;
  char buffer[EVENT_BUF_LEN];

  /*creating the INOTIFY instance*/
  fd = inotify_init();

  /*checking for error*/
  if ( fd < 0 ) {
    perror( "inotify_init" );
  }

  char path[MAX_PATH];

  authenticate(server_socket, clientid);
  synchronise(server_socket);

  scan_dirs(cumdir.c_str(),fd);

  while(length = read( fd, buffer, EVENT_BUF_LEN )){
    /*checking for error*/
    if ( length < 0 ) {
      perror( "read" );
    }  

    /*actually read return the list of change events happens. Here, read the change event one by one and process it accordingly.*/
    while ( i < length ) { 
      struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];
      if ( event->len ) {
	handle_event(event, fd);
      }
      i += EVENT_SIZE + event->len;
    }
    i = 0;
  }

  /*removing the “/tmp” directory from the watch list.*/
   inotify_rm_watch( fd, wd );

  /*closing the INOTIFY instance*/
   close( fd );

}
