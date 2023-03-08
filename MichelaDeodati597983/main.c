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

/**
 * @brief esegue il controllo dell'input in maniera quantitativa perchè poi il controllo vero e proprio
 * e l'assegnazione dei valori viene lasciata al master thread
 * 
 * @param argc dimensione di argv
 * @param argv parametri passati al main da riga di comando
 */
void InputCheck(int argc, string argv[]){
    if(argc<2){
         fprintf(stderr, "****FATAL ERROR****\nUsage %s: 'filename.....'\nOptions:\n\t-n\t<nthreads>\n\t-q\t<qsize>\n\t-d\t<directory name>\n\t-t\t<delay>\n", argv[0]);
         exit(EXIT_FAILURE);
    }
}

int main (int argc, string argv[]){
    //contenuta in Util.h esegue il controllo del numero di argomenti passati al main
    InputCheck(argc, argv);
    //avvio il masterthread che è il core del progetto
    runMasterThread(argc,argv);

    return 0;
}