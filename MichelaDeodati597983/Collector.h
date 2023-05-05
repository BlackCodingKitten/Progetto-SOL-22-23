/**
 * @file Collector.h
 * @author Michela Deodati 597983
 * @brief interfaccia collector
 * 
 * @date 15-03-22
 * 
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 2001112L
#endif

#ifndef COLLECTOR_H_
#define COLLECTOR_H_

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
 * @brief implementa tutte le funzionalità del collector, apre una socket  e si connette al server (Masterthread), legge i risultati calcolati dai worker, e stampa ordinatamente i file 
 * 
 * @param numFile numero di file che vengono passati 
 * 
 * @return esce con codice  :EXIT_SUCCESS se va tutto bene, EXIT_FAILURE se ci sono stati degli errori
 */
void runCollector(int numFile);

#endif  /*COLLECTOR_H_*/