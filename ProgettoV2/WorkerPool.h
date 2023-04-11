/**
 * @file Workerpool.h
 * @author MICHELA DEODATI 597983
 * @brief implementazione dell'interfaccia workerpool
 * @version 2.0
 * @date 20-03-2023
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef WORKER_POOL_H_
#define WORKER_POOL_H_

#include <pthread.h>
#include <stdbool.h>

#include "./Util.h"

typedef struct {
    void * arg;
}workertask_t;

typedef struct {
    pthread_mutex_t queue_mutex; 
    pthread_mutex_t conn_mutex;
    pthread_cond_t queue_cond;
    pthread_cond_t conn_cond;
    int can_write;
    pthread_t * workers_array;
    int fd_socket;
    int numWorkers;
    workertask_t * queue;
    int qsize;
    int head;
    int tail;
    int queue_remain;
    bool exiting;
}workerpool_t;

typedef struct {
    string file;
    workerpool_t * wpool;
}worker_arg;



workerpool_t* createWorkerPool(int nthread, int qsize, int fd_conn);


/**
 * @function addToWorkerPool
 * @brief aggiunge una task al wpool, se ci sono thread liberi o se la coda dei pendenti non è piena, altrimenti fallisce
 * 
 * @param wpool oggetto workerpool
 * @param arg argomento della task del worker

 * @return int 0 se va tutto bene, 1 non ci sono thread liberi o la coda è piena,-1 fallisce la chimata, setta errno
 */
int addTask(workerpool_t * wpool, void * arg);

static void * wpoolWork (void * wpool);

bool destroyWorkerPool(workerpool_t * wpool, bool wait_task );

static void *  calcolaFile (void * arg);

#endif /* WORKER_POOL_H_ */