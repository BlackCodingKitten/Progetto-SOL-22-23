#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 2001112L
#endif
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
 * @brief ricavare la dimensione in byte del file 
 * 
 * @param file nome del file
 * @return long 
 */
long getFileSize(FILE *file){
    long size;

    fseek(file, 0, SEEK_END);  // si posiziona alla fine del file
    size = ftell(file);        // legge la posizione attuale nel file, che corrisponde alla sua dimensione in byte
    fseek(file, 0, SEEK_SET);  // si riporta all'inizio del file

    return size;
}

/**
 * @function: wpoolWorker
 * @brief funzione eseguita dal worker che appartiene a wpool
 * 
 * @param pool workerpool_t
 */
static void * wpoolWorker(void* pool){
 workerpool_t * wpool =(workerpool_t*)pool;//cast della void*pool in una workerpool
    workertask_t task;//task generica
    //acquisisco la lock
    if(pthread_mutex_lock(&(wpool->lock))!=0){
        fprintf(stderr, "WORKERPOOL: in funzione wpoolWorker: errore acqusizione lock\n");
        pthread_exit(NULL);
    }
    
    while(true){
        //rimango in attesa di un segnale, controllo dei wakeups spuri
        if(!(wpool->exiting) && (wpool->pendingQueueCount==0)){
            pthread_cond_wait(&(wpool->cond), &(wpool->lock));
        }
        //la pool è in fase di uscita e non ci sono più task pendenti, il thread esce dal while true
        if(wpool->exiting && (wpool->pendingQueueCount==0)){
            //eseguo la unlock
            if(pthread_mutex_unlock(&(wpool->lock))!=0){
                fprintf(stderr, "WORKERPOOL: in wpoolWorker errore unlock della coda in fase di uscita\n");
                pthread_exit(NULL);
            }
            break;
        }else{
            //diminuisco il numero di task in coda
            --wpool->pendingQueueCount;
            //il thread prende una nuova task dalla testa della threadpool
            task.fun = wpool->pendingQueue[wpool->queueHead].fun;
            task.arg = wpool->pendingQueue[wpool->queueHead].arg;
            //sposto in avanti la testa di 1 
            ++wpool->queueHead;
            //se la testa è arrivata in fondo alla queue la riporto in cima;
            if(wpool->queueHead == wpool->queueSize){
                wpool->queueHead = 0;
            }
            //incremento il numero di task attive eseguite dai thread
            ++wpool->activeTask;
            //eseguo la funzione passata al thread:
            (*(task.fun))(task.arg);

            //eseguo la unlock sulla coda
            if(pthread_mutex_unlock(&(wpool->lock))!=0){
                fprintf(stderr, "WORKERPOOL: in wpoolWorker errore unlock della coda\n");
                pthread_exit(NULL);
            }

            //dopo aver eseguito la task rieseguo la lock sulla coda per poter modificare il valore delle task attive
            if(pthread_mutex_lock(&(wpool->lock))!=0){
                fprintf(stderr, "WORKERPOOL: wpoolWorker, errore losck sulla mutex della coda delle task\n");
                pthread_exit(NULL);
            }
            //ho eseguito la task, decremento il numero di quelle attive
            --wpool->activeTask;
        }
    }//end while 
    //non serve esguire la unlock perchè l'ho eseguita prima di uscire dal while, concludo la task del thread con status NULL
    pthread_exit(NULL);
}



workerpool_t * createWorkerpool (int numWorkers, int queueSize){

    //questo controllo è quasi superfluo ma la sicurezza non è mai troppa
    if(numWorkers<=0 || queueSize <0){
        errno=EINVAL;
        return NULL;
    }
    //inizializzo la workerpool
    workerpool_t * wpool;
    if((wpool=(workerpool_t*)malloc(sizeof(workerpool_t)))==NULL){
        //se la malloc ritorna NULL stampo l'errore e ritorno NULL
        perror("Fallisce l'allocazione della workerpool");
        return NULL;
    }

    //setto le condizioni iniziali
    wpool->numWorkers=0;
    wpool->activeTask=0;
    wpool->queueSize = queueSize;
    wpool->queueHead = 0;
    wpool->queueTail = 0;
    wpool->pendingQueueCount=0;
    wpool->exiting=false;
    
    //alloco l'array di workers
    if((wpool->workers=(workerthread_t*)malloc(sizeof(workerthread_t)*numWorkers))==NULL){
        //se fallisce la malloc
        perror("WORKERPOOL-nella creazione della threadpool fallisce l'allocazione dell'array di theread");
        //libero la wpool
        free(wpool);
        return NULL;
    }
    
    //alloco la queue di workertask
    if((wpool->pendingQueue=(workertask_t*)malloc(sizeof(workertask_t)*queueSize))==NULL){
        //se fallisce la malloc
        perror("Fallisce l'allocazione della pendingQueue nella workerpool");
        //libero la memoria
        free(wpool->workers);
        free(wpool);
        return NULL;
    }
    //inizializzo tutte le funzioni della queue a leggieSomma()
    for(int i=0; i<queueSize; i++){
        (wpool->pendingQueue)[i].fun = leggieSomma;
    }
    
    //inizializzo la mutex della threadpool
    if((pthread_mutex_init(&(wpool->lock), NULL))==-1){
        perror("fallisce l'inizializzazione della mutex per la coda nella workerpool");
        free(wpool->workers);
        //adesso devo liberare anche la coda
        free(wpool->pendingQueue);
        free(wpool);
        return NULL;
    }
    //inizializzo la cond della threadpool
    if((pthread_cond_init(&(wpool->cond),NULL))==-1){
        perror("Fallisce l'inizializzazione della condition nella workerpool");
        free(wpool->workers);
        free(wpool->pendingQueue);
        free(wpool);
        return NULL;
    }
    for(int i=0; i<numWorkers;i++){
        if(pthread_create(&((wpool->workers[i]).wid), NULL, wpoolWorker, (void*)wpool)!=0){
            fprintf(stderr, "Fallisce la creazione dei thread della pool\n");
            //se anche solo 1 create non v a buon fine distruggo la pool e setto errno
            destroyWorkerpool(wpool,false);
            errno = EFAULT;
            return NULL;
        }
        //se la pthread_create() va a buon fine incremento numWorkers, altrimenti no.
        wpool->numWorkers++;
    }
    return wpool;
}

/**
 * @function freeWorkPool
 * @brief libera le risorse della workerpool
 * 
 * @param wpool: threadpool di worker
 */
static void freeWorkPool(workerpool_t* wpool){
    if(wpool->workers!=NULL){
        free(wpool->workers);
        free(wpool->pendingQueue);
        pthread_mutex_destroy(&(wpool->lock));
        pthread_cond_destroy(&(wpool->cond));
    }
    free(wpool);
}

bool destroyWorkerpool (workerpool_t* wpool, bool waitTask){
    if(wpool==NULL){
        errno = EINVAL;
        return false;
    }
    //setto lo stato di uscita della threadpool
    wpool->exiting=true;
    
    if(pthread_mutex_lock(&(wpool->lock))!=0){
        fprintf(stderr, "Errore fatale lock\n");
        return false;
    }
    //se sono in fase di uscita perchè ho finito le task da inviare alla queue  waitTsak=true altrimenti è un uscita forzata di emergenza.
    if(waitTask){
        //mando un segnale broadcast in modo da svegliare tutti i thread per terminare le task
        if(pthread_cond_broadcast(&(wpool->cond))!=0){
            if(pthread_mutex_unlock(&(wpool->lock))!=0){
                fprintf(stderr,"Errore Fatale unlock\n");
                return false;
            }
        }
        //unlock della queue
        if(pthread_mutex_unlock(&(wpool->lock))!=0){
            fprintf(stderr,"Errore Fatale unlock\n");
            return false;
        }
        for(int i=0; i<wpool->numWorkers;i++){
            //faccio la join con tutti i thread per attendere la terminazione delle task già inserite in coda
            if(pthread_join(wpool->workers[i].wid,NULL)!=0){
                errno=EFAULT;
                if(pthread_mutex_unlock(&(wpool->lock))!=0){
                    fprintf(stderr,"Errore Fatale unlock riga 204 Workerpool.c\n");
                }
                return false;
            }
        }
    }

    freeWorkPool(wpool);
    //ho liberato la memoria della wpool
    return true;
}

int addTask (workerpool_t* wpool, void* voidFile){
   //controllo che la task non sia NULL e che la wpool non sia NULL
    if(wpool==NULL || voidFile == NULL){
        //setto errno
        errno=EINVAL;
        return -1;
    }

    if(wpool->exiting){
        //la pool è in uscita non accetto nuove task
        return 1;
    }
    
    //acquisico la lock
    if(pthread_mutex_lock(&(wpool->lock))!=0){
        fprintf(stderr, "Workerpool.c errore fatale lock");
        return -1;
    }
    int qSize = wpool->queueSize;
    //controllo se la coda è piena o se si è in fase di uscita non accetto più alcuna task
    if((wpool->pendingQueueCount >= qSize || wpool->exiting) ){
        //unlock
        if(pthread_mutex_unlock(&(wpool->lock))!=0){
            fprintf(stderr, "Workerpool.c errore unlock");
            return -1;
        }
        //uscita col valore che indica queue piena in  modo da non far aggiungere altre task
        
        return 1;
    }
    //inserico in coda la nuova task
    wpool->pendingQueue[wpool->queueTail].arg=voidFile;
    //incremento il numero delle task in coda
    ++wpool->pendingQueueCount;
    //incremento il vlaore del puntatore alla fine della coda
    ++wpool->queueTail;
    //se il puntatore alla fine della coda supera la dimensione della coda, sovreascrivo le task di testa 
    if(wpool->queueTail >= qSize){
        
        wpool->queueTail=0;
    }

    
    if(pthread_cond_signal(&(wpool->cond))== -1){
        //rilascio la lock
        if(pthread_mutex_unlock(&(wpool->lock))!=0){
            fprintf(stderr," fallimento unlock di emergenza dopo il fallimneot della signal in add task\n");
        }
        //ritorno -1 in ogni caso
        return -1;
    }
    
    //se ho mandato correttamente il segnale rilascio la lock e return 0
    if(pthread_mutex_unlock(&(wpool->lock))!=0){
        fprintf(stderr, "Errore unlock riga 292 Workerpool.c\n");
        return -1;
    }
    
    return 0;
}



void leggieSomma (void*arg){
    //casto arg del thread nella path del file
    if(arg==NULL){
        return;
    }
    string filePath = (*(wArg*)arg).path;
    
    //inizializzo la variabile somma
    long somma = 0;

    //apro il file in modalità lettura binaria
    FILE*file = fopen(filePath,"rb");
    //controllo che sia stato aperto correttamente
    if(file==NULL){
        perror(filePath);
        fprintf(stderr, "Errore apertura file:%s\n",filePath);
        pthread_exit(NULL);
    }

    //ricavo quanti numeri ci sono nel file
    long fileDim=getFileSize(file)/ sizeof(long); 
    
    //alloco un array in cui inserisco tutti i valori dopo averli letti
    long* longInFile =(long*)malloc(sizeof(long)*fileDim);

    //leggo tutti i valori del file e li salvo in fileArray
    if(fread(longInFile,sizeof(long),fileDim,file)<=0){
        fprintf(stderr,"leggieSomma: errore lettura valori nel file %s", filePath);
        fclose(file);
        free(longInFile);
        return;
    }
    //chiudo il file dopo che ho finito di leggere
    fclose(file);

    //sommo il prodotto del valore per l'indice
    for(int i=0; i<fileDim; i++){
        somma=somma+(longInFile[i]*i);
    }
    //dopo aver calcolato la somma libero la memoria
    free(longInFile);
    
    //alloco il buffer per scrivere della dimensione (numero massimo di caratteri di maxlong + la lunghezza massima della path del file)
    string buffer = malloc(sizeof(char)*FILE_BUFFER_SIZE+PATH_LEN);
    memset(buffer,'\0',FILE_BUFFER_SIZE+PATH_LEN);
    //converto il valore della somma in stringa e poi gli accodouno spazio e la path del file per poi spedirlo al Collector
    sprintf(buffer, "%ld", somma);
    strcat(buffer, " ");
    strcat(buffer, filePath);
    
    if(pthread_mutex_lock((*(wArg*)arg).socketMutex) != 0){
        fprintf(stderr, "Errore lock socket\n");
        free(buffer);
        return;
    }else{
        //lock eseguita correttamente
        //spedisco al Collector con la writen i risultati
        int sc;
        if((sc=writen((*(wArg*)arg).sock, buffer ,strlen(buffer)+1))!=0){
            perror("write()");
            free(buffer);
            return;
        }
        
        //aspetto di ricevere OK dal collector prima di rilasciare la lock
        memset(buffer,'\0',FILE_BUFFER_SIZE+PATH_LEN);
        if(readn((*(wArg*)arg).sock,buffer,3)==-1){
            fprintf(stderr, "il collector non ha ricevuto correttamente il file\n");
        }
        if(pthread_mutex_unlock((*(wArg*)arg).socketMutex)!=0){
            fprintf(stderr, "impossibile fare la unlock della socket\n");
            free(buffer);
            return;
        }
        
    }
    //libero la memoria
    free(buffer);
    
}

