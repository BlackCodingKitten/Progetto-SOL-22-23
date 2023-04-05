/**
 * @file WorkerPool.c
 * @author Michela Deodati 597983
 * @brief implementazionde dell'interfaccia Workerpool
 * @date 15-03-2023
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/un.h>

#include "./WorkerPool.h"
#include "./Util.h"

workerpool_t* createWorkerPool(int nthread, int qsize, int fd_conn){
    //inizializzo la workerpool
    workerpool_t * wpool;
    if((wpool=(workerpool_t*)malloc(sizeof(workerpool_t)))==NULL){
        fprintf(stderr, "Ha fallito la malloc\n");
        return NULL;
    }

    //l'allocazione è andta a buon fine, setto le condizioni iniziali:
    wpool->numWorkers=0;
    wpool->qsize=qsize;
    wpool->head=0;
    wpool->tail=0;
    wpool->fd_socket=fd_conn;
    wpool->exiting=false;
    wpool->queue_remain=0;
    wpool->can_write=1;

    //alloco l'array di workers
    if((wpool->workers_array=(pthread_t*)malloc(sizeof(pthread_t)*nthread))==NULL){
        fprintf(stderr, "Ha fallito la malloc\n");
        free(wpool);
        return NULL;
    }

    //alloco la coda delle task
    if((wpool->queue=(worker_arg*)malloc(sizeof(worker_arg)*qsize))==NULL){
        fprintf(stderr, "Ha fallito la malloc\n");
        free(wpool->workers_array);
        free(wpool);
        return NULL;
    }

    //inizializzo mutex e condizioni della threadpool
    if((pthread_mutex_init(&(wpool->conn_mutex),NULL)==-1)){
        perror("fallita l'inizilizzazione della mutex sulla connsessione");
        free(wpool->workers_array);
        free(wpool->queue);
        free(wpool);
        return NULL;
    }
    if((pthread_mutex_init(&(wpool->queue_mutex),NULL))==-1){
        perror("fallita l'inizilizzazione della mutex sulla coda");
        free(wpool->workers_array);
        free(wpool->queue);
        free(wpool);
        return NULL;
    }
    if((pthread_cond_init(&(wpool->conn_cond),NULL)==-1)){
        perror("fallita l'inizilizzazione della cond sulla connessione");
        free(wpool->workers_array);
        free(wpool->queue);
        free(wpool);
        return NULL;

    }
    if((pthread_cond_init(&(wpool->queue_cond),NULL))==-1){
        perror("fallita l'inizilizzazione della con sulla coda");
        free(wpool->workers_array);
        free(wpool->queue);
        free(wpool);
        return NULL;

    }

    //creo i thread della wpool
    for(int i=0; i <nthread; i++){
        if(pthread_create(&(wpool->workers_array[i]), NULL, &wpoolWork, (void*)wpool)!=0){
            destroyWorkerPool(wpool,false);
            return NULL;
        }
        wpool->numWorkers++;
    }


    //tutto è andato abuon fine
    return wpool;
}