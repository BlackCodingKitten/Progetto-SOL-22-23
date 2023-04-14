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

/**
 * @brief cacola per ciascun file quanti long ci sono al suo interno
 *        è una funzione interna usata da leggi e somma
 * @param filename nome del file (che corrisponde alla path)
 * @return long qunatità di numeri di tipo long ci sono nel file 
 */
long getLongsAmount(FILE*filename){
    long size = 0;
    //Sposto il puntatore alla fine del file 
    fseek(filename, 0, SEEK_END);
    //leggo l'indice che corrisponde al byte 
    size=ftell(filename);
    //riporto il untatore in testa al file 
    fseek(filename, 0, SEEK_SET);
    //calcolo quanti long ci sono nel file 
    size=size / sizeof(long);
    return size;
}

/**
 * @brief Create a Worker Pool object
 * 
 * @param nthread numero dei tread nella threadpool
 * @param qsize dimensione della coda delle task passate alla pool
 * @param fd_conn cnalae di comunicazione con cui ciascun thread parla al collector
 * @return workerpool_t* puntatore alla threapool, NULL se ci sono stati errori nel processo di creazione
 */
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
    if((wpool->queue=(workertask_t*)malloc(sizeof(workertask_t)*qsize))==NULL){
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
    for(int i=0; i < nthread; i++){
        if(pthread_create(&(wpool->workers_array[i]), NULL, &wpoolWork, (void*)wpool)!=0){
            destroyWorkerPool(wpool,false);
            return NULL;
        }
        wpool->numWorkers++;
    }
    //tutto è andato abuon fine
    return wpool;
}


/**
 * @brief libera le risorse allocate dalla threadpool, non viene mai chimata direttamente,
 *        viene chimata tramite destroyworkerpool
 * 
 * @param wpool threadpool di cui vanno liberate le risorse.
 */
static void freeWorkerPool(workerpool_t* wpool){
    if(wpool->workers_array != NULL){
        free(wpool->workers_array);
        free(wpool->queue);
    }
    //elimino le muetx
    pthread_mutex_destroy(&(wpool->conn_mutex));
    pthread_mutex_destroy(&(wpool->queue_mutex));
    //elimino le condizioni
    pthread_cond_destroy(&(wpool->queue_cond));
    pthread_cond_destroy(&(wpool->conn_cond));

    free(wpool);
    //fine 
}

/**
 * @brief 
 * 
 * @param wpool threadpool
 * @param wait_task attendere la fine della task
 * @return true distruzione della pool andata a buon fine
 * @return false errore nella distruzione della pool
 */
bool destroyWorkerPool(workerpool_t * wpool, bool wait_task ){
    if (wpool==NULL){
        errno=EINVAL;
        return false;
    }
    //setto lo stato di uscita della threadpool
    wpool->exiting=true;

    if(pthread_mutex_lock(&(wpool->queue_mutex))){
        fprintf(stderr, "chiusura della pool Errore lock\n");
        return false;
    } 

    //ontrollo se devo attendere l'attesa delle task pendenti
    if(wait_task){
        if(pthread_cond_broadcast(&(wpool->queue_cond))!=0){
            if(pthread_mutex_unlock(&(wpool->queue_mutex))!=0){
                fprintf(stderr,"Errore fatale nella chiusura della pool, impossibile svegliare altri thread\n");
            }
            return false;
        }

        for(int i=0; i<wpool->numWorkers; i++){
            //faccio la join su ogni thread
            pthread_join(wpool->workers_array[i],NULL);
        }
    }

    if(pthread_mutex_unlock(&wpool->queue_cond)!=0){
        fprinf(stderr, "errore unlock durante la chiusura della pool\n");
        return false;
    }

    //libero le risorse della pool
    freeWorkerPool(wpool);
    return true;

}

/**
 * @brief task di ciscuno dei thread nella threadpool che permette di estrarre un elemento della coda concorrente 
 *          ed eseguire la task;
 * 
 * @param wpool threadpool
 */
static void * wpoolWork(void * wpool){
    //per prima cosa casto la threadpool
    workerpool_t * pool = (workerpool_t *)wpool;
    workertask_t toPass; //generico argomento da passare alla funzione calcolaFile
    //acquisisco la losck sulla coda
    if(pthread_mutex_lock(&(pool->queue_mutex))!=0){
        fprintf(stderr, "Errore acquisizione lock sulla coda delle taskn\n");
        pthread_exit(NULL);
    }

    while (true){
        //rimango in attesa di un segnale, controllo dei wakeup spuri
        //attendo se la pool NON è in fase di uscita e non ci sono task
        while((!(pool->exiting) && (pool->queue_remain == 0))){
            pthread_cond_wait(&(pool->queue_cond), &(pool->queue_mutex));
        }
        //controllo se il thread si è svgliato perchè la pool è in fase di uscita
        if(pool->exiting && (pool->queue_remain == 0)){
            //threadpool in uscita, sono finite le task in coda 
            if(pthread_mutex_unlock(&(pool->queue_mutex))!=0){
                fprintf(stderr, "Errore unlock della mutex della coda\n");
                pthread_exit(NULL);
            }
            pthread_exit(NULL);
        }else{
            //il thread prende una nuova task dalla testa della coda
            toPass.arg= pool->queue[pool->head].arg;
            //sposto la testa
            ++pool->head;
            if(pool->head >= pool->qsize){
                //riporto la testa in cima 
                pool->head=0;
            }
            //diminuisco il numero di task in coda
            --pool->queue_remain;
            
            if(pthread_mutex_unlock(&(pool->queue_mutex))!=0){
                fprintf(stderr, "Errore unlock della coda delle task \n");
                pthread_exit(NULL);
            }
            //eseguo la funzione
            calcolaFile(toPass.arg);

            //rieseguo la lock
            if(pthread_mutex_lock(&(pool->queue_mutex))!=0){
                fprintf(stderr, "Errore lock della coda delle task\n");
                pthread_exit(NULL);
            }

        }
    }
    //non ci arriva mai 
    exit(EXIT_FAILURE);
}

/**
 * @brief aggiunge una tsk alla coda 
 * 
 * 
 * @return -1 se c'è stato un errore,  1 se la coda è piena o se si è in fse di uscita, 0 quando la task è stata aggiunta alla coda con successo
 */
int addTask(workerpool_t * wpool, void * arg){
    //controllo che la pool non si ain uscita altrimenti esco direttamente 
    //in fase di uscita non si accettano nuove task
    if(wpool->exiting){
        return 1;
    }


    //acquisisco la lock
    if(pthread_mutex_lock(&(wpool->queue_mutex))!=0){
        fprintf(stderr,  "Errore lock sulla coda delle task durante l'aggiunta\n");
        return -1;
    }else{
        int size= wpool->qsize;
        
        if((wpool->queue_remain)>=size){
            if(pthread_mutex_unlock(&(wpool->queue_mutex))!=0){
                return -1;
            }
            return 1;
        }

        wpool->queue[wpool->tail].arg=arg;
        //incremento tail controllando che non sia arrivata al limite della dimensione della coda
        ++wpool->tail;
        if(wpool->tail >= size){
            wpool->tail=0;
        }
        //incremento il numero delle task pendenti
        ++wpool->queue_remain;

        //ho inserito una nuova task, sveglio un thread
        if(pthread_cond_signal(&(wpool->queue_cond))==-1){
            if(pthread_mutex_unlock(&(wpool->queue_mutex))!=0){
                fprintf(stderr, "Errore aggiunta task in coda\n");
            }
            //ritorno -1 in ogni caso
            return -1;
        }
        //se ho mandato correttamente il segnale rilascio la lock e return 0
        if(pthread_mutex_unlock(&(wpool->queue_mutex))!=0){
            fprintf(stderr, "Errore unlock coda delle task \n");
            return -1;
        }

        return 0;
    }

    //non ci deve mai arrivare qui 
    return -1;
}

/**
 * @brief legge i valori dal file, li calcola e comunica col collector
 * 
 */
static void *  calcolaFile (void * arg){
    //casto l'argomento
    worker_arg toDo = *((worker_arg*)arg);
    //inizializzo il valore somma 
    long somma=0;
    //apri il file in "lettura binaria"
    FILE*file=fopen(toDo.file, "rb");
    //controllo che sia stato aperto correttamente
    if(file == NULL){
        perror(toDo.file);
        return NULL;
    }

    //ricavo quanti numeri ci sono nel file
    long fileDim = getLongsAmount(file);

    //alloco un array in cui inserire turri i valori
    long* longInFile =(long*)malloc(sizeof(long)*fileDim);

    //leggo tutti i valori dal file e li salvo in un array
    if(fread(longInFile,sizeof(long),fileDim,file)<=0){
        fprintf(stderr,"leggieSomma: errore lettura valori nel file %s", toDo.file);
        fclose(file);
        free(longInFile);
        return NULL;
    }
    //ho finito di leggere posso chiudere il file 
    fclose(file);

    //eseguo la somma
    for (int i=0;  i< fileDim; i++){
        somma=somma + (longInFile[i] + i);
    }
    //finito di calcolare la somma libero la memoria
    free(longInFile);

    string buffer = malloc(sizeof(char)*BUFFER_SIZE);
    memset(buffer,'\0',BUFFER_SIZE);
    //converto il valore della somma in stringa e poi gli accodouno spazio e la path del file per poi spedirlo al Collector
    sprintf(buffer, "%ld", somma);
    strcat(buffer, " ");
    strcat(buffer, toDo.file);

    //sono pronta per scrivere 
    if(pthread_mutex_lock(&(toDo.wpool->conn_mutex))!=0){
        fprintf(stderr, "Errore lock socket\n");
        free(buffer);
        return NULL;
    }else{
        //aspetto che il socket sia disponibile per la scrittura
        while(toDo.wpool->can_write == 0){
            pthread_cond_wait(&(toDo.wpool->conn_cond),&(toDo.wpool->conn_mutex));
        }
        toDo.wpool->can_write=0;
        if(writen(toDo.wpool->fd_socket, buffer,  strlen(buffer)+1)<0){
            fprintf(stderr, "Errore fatale scrittura ");
            free(buffer);
            exit(EXIT_FAILURE);
        }
        char ok[3];
        if(readn(toDo.wpool->fd_socket, ok, 3) == -1){
            fprintf(stderr, "messaggio non ricevuto dal collector\n");
            free(buffer);
            return NULL;
        }
        toDo.wpool->can_write=1;
        pthread_cond_signal(&(toDo.wpool->conn_cond));
        if(pthread_mutex_unlock(&(toDo.wpool->conn_mutex))!=0){
            fprintf(stderr, "errore rilascio lock della socket");
            free(buffer);
            exit(EXIT_FAILURE);
        }
    }
    //libero la memoria 
    free(buffer);
    return NULL;
}