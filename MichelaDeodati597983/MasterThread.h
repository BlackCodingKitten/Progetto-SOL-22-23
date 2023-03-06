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
#include <Collector.h>

#define NTHREAD_DEFAULT 4
#define QUEUE_SIZE_DEFAULT 8
#define DELAY_DEFAULT 0

typedef struct s{
    int * stop;         //condizione di terminazione del while del masterthread;
    sigset_t set;     // set dei segnali da gestire mascherati
    int signal_pipe;    //descrittore di scrittura di una pipe senza nome usato per comunicare al collector che è terminata l'esecuzione
}sigHarg;

/**
 * @brief funzione principale che svolge tutte le funzioni del MasterThread
 * 
 * @param argc numero di argomenti passati in input
 * @param argv char*[] degli argomenti
 */
void runMasterThread(int argc,string argv[]);