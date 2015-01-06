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

int rstr(char *str, const int length)
{
  char alphabeth[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  for (int i = 0; i < length; ++i)
    str[i] = alphabeth[rand() % (sizeof(alphabeth) - 1)];
  str[length] = 0;
  return 0;
}

int synchronise(int socket)
{
  printf("[client] synchronise...\n");
  CUM_MSG cmsg;
  while (!recieve_message(socket, (char*)&cmsg, sizeof(CUM_MSG))){
    printf("id    = %d\nflags = %d\n", cmsg.id, cmsg.flags);
    if (cmsg.id == MSG_FILE)
      recieve_file(socket);
    if (cmsg.id == MSG_OK)
      break;
  }
}

int scan_dirs(const char* path, int fd)
{
  int wd;
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

void handle_event ( struct inotify_event *event, int fd ) {
  if ( event->mask & IN_CREATE ) {
    if ( event->mask & IN_ISDIR ) {
      printf( "New directory %s created.\n", event->name );
      scan_dirs(event->name, fd);
      send_file(server_socket, event->name);
    }
    else {
      printf( "New file %s created.\n", event->name );
      send_file(server_socket, event->name);
    }
  }
  else if ( event->mask & IN_DELETE ) {
    if ( event->mask & IN_ISDIR ) {
      printf( "Directory %s deleted.\n", event->name );
      wds.erase(wdpaths[event->wd]);
      wdpaths.erase(event->wd);

    }
    else {
      printf( "File %s deleted.\n", event->name );
    }
  }
}

int identify(char* cid){
  FILE *fp = fopen(".cumulus","r");
  if (!fp){
    FILE *fp = fopen(".cumulus","w");
    rstr(cid, CLIENTID_LEN);
    write(fp, cid, CLIENTID_LEN);
    fclose(fp);
    return 0;
  }

  read(fp, cid, CLIENTID_LEN);
  fclose(fp);
  return 0;
  
}

int main( )
{
  char clientid[CLIENTID_LEN];
  identify(clientid);
  printf("[client] Starting client %s... \n", clietnid);

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
