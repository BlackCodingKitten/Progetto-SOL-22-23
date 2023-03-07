#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 2001112L
#endif
/**
 * @file WorkerPool.h
 * @author Michela Deodati 597983
 * @brief implementazionde dell'interfaccia Workerpool
 * @date 15-03-2023
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef WORKER_POOL_H_
#define WORKER_POOL_H_

#include <pthread.h>
#include <stdbool.h>

#include "./Util.h"



/**
 * @struct workerthread_t
 * @brief generico thread worker che appartiene alla pool
 * 
 * @var wid: pthread_t id del worker
 * @var wpoolIndex: indice del thread nella threadpool
 */
typedef struct workerthread_t{
    pthread_t wid;
    int wpoolIndex;
}workerthread_t;

/**
 * @struct workertask_t
 * @brief task che devono eseguire i thread worker
 * 
 * @var funPtr: Puntetore alla funzione da eseguire
 * @var arg: argomento della funzione 
*/
typedef struct workertask_t{
    void(*funPtr)(void*);
    void*arg;
}workertask_t;

/**
 * @struct workerpool
 * @brief rappresentazionde della threadpool di worker
*/
typedef struct workerpool_t{
    pthread_mutex_t lock;                   //mutext nell'accesso al'oggetto
    pthread_cond_t  cond;                   //usata per notificare un worker
    workerthread_t*      workers;           //array di worker id
    int             numWorkers;             //size dell'array workers
    workertask_t*   pendingQueue;           //coda interna usata per le task pendenti
    int             queueSize;              //massima size della coda, se settata a -1 non ccetta task pendenti
    int             activeTask;             //numero di task che sono attualmente in esecuzione
    int             queueHead;              //testa della coda
    int             queueTail;              //fine della coda
    int             pendingQueueCount;      //quantità di task pendenti presenti
    bool            exiting;                //true segnala se viene iniziato il protocollo di uscita
    bool            waitEndingTask;         //true quando si vuole aspettare che non ci siano più lavori in coda
}workerpool_t;

//FUNZIONI DI GESTIONE DELLA THREADPOOL DI WORKER

/**
 * @function createWorkerPool
 * @brief crea un oggetto workerpool
 * @param numWorkers è il numero di worker della pool
 * @param pendingSize è la size delle richieste che possono essere pendenti, -1 se non ci possono essere richieste pendenti
 * 
 * @return se tutto va bene ritorna una threadpool di workers, altrimenti ritorna NULL e setta opportunatamente errno
*/
workerpool_t* createWorkerpool (int numWorkers, int pendingSize);

/**
 *@function destroyWorkerPool
 * @brief stoppa tutti i thread in esecuzione e distrugge l'oggetto wpool 
 * 
 * @param wpool 
 * @param forcedExit 
 * @return true in caso di successo
 * @return false in caso di fallimento, setta errno
 */
bool destroyWorkerpool (workerpool_t* wpool, bool forcedExit);

/**
 * @function addToWorkerPool
 * @brief aggiunge una task al wpool, se ci sono thread liberi o se la coda dei pendenti non è piena, altrimenti fallisce
 * 
 * @param wpool oggetto workerpool
 * @param task task del worker
 * @param arg argomento della task del worker
 * @return int 0 se va tutto bene, 1 non ci sono thread liberi o la coda è piena,-1 fallisce la chimata, setta errno
 */
int addToWorkerpool (workerpool_t* wpool, void(*task)(void*), void*arg);

/**
 * @brief task del thread worker che fa i calcoli sul file
 * 
 * @param arg viene passato il nome del file;
 */
void leggieSomma (void*arg);

#endif /* WORKER_POOL_H_ */