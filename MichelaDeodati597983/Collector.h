#define _POSIX_C_SOURCE 2001112L
/**
 * @file Collector.h
 * @author Michela Deodati 597983
 * @brief interfaccia collector
 * 
 * @date 15-03-22
 * 
 */
#define _POSIX_C_SOURCE 200112L
#define _OPEN_SYS_ITOA_EXT
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

#include <Util.h>
/**
 * @brief socket su cui il Collector si mette in ascolto
 * 
 */
int listenSocket;
/**
 * @brief struct che contiene la path del file e il valore calcolato
 * 
 */
typedef struct data{
    long fileValue;     //valore della sommatoria delle righe del file per il loro indice
    string filePath;    //path del file
}Data;

void runCollector(int numFile);

void stampaRisultati (Data * a);

