#define _POSIX_C_SOURCE 200112L
#define _OPEN_SYS_ITOA_EXT
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <WorkerPool.h>
#include <Util.h>

typedef struct sighandler_t{
    sigset_t * set;         //set dei segnali da gestire
    int signalPipe;          //descrittore di una pipe senza nome 
}sighandler_t;

static void* sigHandlerTask (void*arg);

bool isRegularFile(string path);

bool checkNthread(const int nthread);
bool checkqSize (const int qsize);
bool checkDelay (const int time);
bool isDirectory (const string dir);

void runMasterThread(int argc,string argv[]);