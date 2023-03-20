/**
 * @file Masterthread.h
 * @author MICHELA DEODATI 597983
 * @brief 
 * @version 2.0
 * @date 20-03-2023
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "./Util.h"

/**
 * @brief funzione che controlla la thread pool, lancia il signal handler, e esegue la fork del processo collector, 
 * 
 * @param nthread numero di thread della threadpool
 * @param qsize  dimensione della coda delle task
 * @param tdelay tempo di inserimento tra una task e la prossima nella coda
 * @param fileindex indice dove si trovano i file dopo aver eseguito la getopt
 * @param argv array di stringhe passato al main
 * @param mask maschera dei segnali settata 
 * @param fd_conn socket su cui i thread comunicano col collector
 * @return int 0 se è andato tutto bene, 1 se ci sono stati errori
 */
int runMasterthread(int nthread, int qsize, int tdelay, int fileindex, string * argv, sigset_t * mask, int fd_conn);
