#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 2001112L
#endif
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


#include "./Util.h"


/**
 * @brief funzione principale che svolge tutte le funzioni del MasterThread
 * 
 * @param argc numero di argomenti passati in input
 * @param argv char*[] degli argomenti
 */
void runMasterThread(int argc,string argv[]);