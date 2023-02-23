/**
 * @file main.c
 * @author Michela Deodati 597983
 * @brief file main
 * @date 15-03-2023
 * 
 */

#define _POSIX_C_SOURCE 200112L
#define _OPEN_SYS_ITOA_EXT
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

#include <Util.h>
#include <WorkerPool.h>
#include <Collector.h>
#include <MasterThread.h>

typedef char* string;


int main (int argc, string argv[]){
    //contenuta in Util.h esegue il controllo del numero di argomenti passati al main
    INPUT_CHECK(argc,argv);
    return EXIT_SUCCESS;
}