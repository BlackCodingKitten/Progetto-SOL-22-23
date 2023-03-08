/**
 * @file main.c
 * @author Michela Deodati 597983
 * @brief file main
 * @date 15-03-2023
 * 
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 2001112L
#endif
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
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
#include <signal.h>

#include "./WorkerPool.h"
#include "./Collector.h"
#include "./MasterThread.h"

int main (int argc, string argv[]){
    //contenuta in Util.h esegue il controllo del numero di argomenti passati al main
    INPUT_CHECK(argc,argv);
    //avvio il masterthread che è il core del progetto
    runMasterThread(argc,argv);

    return 0;
}