#include <stdint.h>

#define MAX_PATH   256
#define BUFSIZE    256
#define PORT       10026

#define MSG_OK     1
#define MSG_READY  2
#define MSG_FILE   3

struct CUM_MSG {
  uint32_t id;
  uint32_t flags;
};

struct CUM_FILE {
  uint32_t timestamp;
  uint32_t checksum;
  uint32_t flags;
  uint32_t size;
  char path[256];
};
