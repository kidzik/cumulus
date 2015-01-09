#include <stdint.h>
#include <openssl/md5.h>

#define MAX_PATH     512
#define BUFSIZE      256
#define CHECKSUM_LEN 33
#define PORT         10033
#define CLIENTID_LEN 64

#define MSG_ERROR  0
#define MSG_OK     1
#define MSG_READY  2
#define MSG_FILE   3
#define MSG_REFUSE 4
#define MSG_AUTH   5

#define CUM_DIR    1

struct CUM_MSG {
  uint32_t id;
  uint32_t flags;
};

struct CUM_FILE {
  uint32_t timestamp;
  char checksum[CHECKSUM_LEN];
  uint32_t mode;
  uint32_t size;
  char path[MAX_PATH];
};

struct CUM_AUTH {
  char clientid[CLIENTID_LEN];
  char login[CLIENTID_LEN];
  char passhash[CLIENTID_LEN];
};

