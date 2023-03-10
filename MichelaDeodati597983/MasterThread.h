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
 * @brief è la funzione che gestisce la threadpool, lacia il signal handler, 
 * e si forka nel processso collector, sostanzialmente il core del progetto
 * 
 * @param n numero thread
 * @param q dimensione coda
 * @param t delay assegamento task
 * @param numFilePassati totale file passati al main esclusi quelli nella cartella
 * @param argarray array di stringhe dei file passati
 */
int runMasterThread(int n, int q, int t, int numFilePassati, string * files);