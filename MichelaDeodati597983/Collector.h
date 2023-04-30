#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 2001112L
#endif
/**
 * @file Collector.h
 * @author Michela Deodati 597983
 * @brief interfaccia collector
 * 
 * @date 15-03-22
 * 
 */

#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <assert.h>
#include <signal.h>

#include "./Util.h"

/**
 * @brief socket su cui il Collector si mette in ascolto
 * 
 */
int listenSocket;


void runCollector(int numFile, int * signal_flag);



