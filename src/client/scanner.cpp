#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <dirent.h>

#include "../common/transfer.h"

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

int server_socket;

int scan_dirs(char* path, int fd)
{
  int wd;
  wd = inotify_add_watch( fd, path, IN_CREATE | IN_DELETE );
  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir (path)) != NULL) {
    /* add all subdirectories */
    while ((ent = readdir (dir)) != NULL) {
      ent->d_name;
      printf ("%s %d\n", ent->d_name, ent->d_type);
      if((DT_DIR & ent->d_type) && strcmp(".",ent->d_name) && strcmp("..",ent->d_name))
	scan_dirs(ent->d_name, fd);
      if((DT_REG & ent->d_type) && strlen(ent->d_name)>0)
	send_file(server_socket, ent->d_name);
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
    }
    else {
      printf( "New file %s created.\n", event->name );
    }
  }
  else if ( event->mask & IN_DELETE ) {
    if ( event->mask & IN_ISDIR ) {
      printf( "Directory %s deleted.\n", event->name );
    }
    else {
      printf( "File %s deleted.\n", event->name );
    }
  }
}

int main( )
{

  printf("[client] Starting... \n");

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

  scan_dirs((char*)".",fd);

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
