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
    wArg task; //task generica
    //acquisisco la lock
    if(pthread_mutex_lock(&(wpool->lock))!=0){
        fprintf(stderr, "WORKERPOOL: in funzione wpoolWorker: errore acqusizione lock\n");
        pthread_exit(NULL);
    }else{
        for(;;){
            //in attesa di un signal, evito wakes up spuri
            while((wpool->pendingQueueCount == 0) && (wpool->exiting == false)){
                pthread_cond_wait(&(wpool->cond), &(wpool->lock));
            }

            //controllo se sono in fase di uscita
            if(wpool->exiting == true) {
                if(wpool->pendingQueueCount == 0){
                    if(pthread_mutex_unlock(&(wpool->lock))!=0){
                        fprintf(stderr, "Errore unlock coda delle task ");
                        return NULL;
                    }
                    pthread_exit(NULL);
                }
            }

            //nuovo task
            if(wpool->pendingQueue[wpool->queueHead]==NULL){
                continue;
            }else{
                puts("Problema?");
                task.path=(string) malloc(sizeof(char) * (1+strlen(wpool->pendingQueue[wpool->queueHead])));
                memset(task.path,'\0', (1+strlen(wpool->pendingQueue[wpool->queueHead])));
                strcpy(task.path, wpool->pendingQueue[wpool->queueHead]);
                wpool->pendingQueue[wpool->queueHead]=NULL;
                fprintf(stdout, "ESTRAGGO DALLA CODA %s\n", task.path);
                task.pool=wpool;           

                wpool->queueHead++;
                if(wpool->queueHead >= wpool->queueSize){
                    wpool->queueHead=0;
                }

                wpool->activeTask--;
                if(pthread_mutex_unlock(&(wpool->lock))!=0){
                    fprintf(stderr, "Errore unlock coda in wpoolworker\n");
                    exit(EXIT_FAILURE);
                }
                leggieSomma((void*)&task);
                free(task.path);
                if(pthread_mutex_lock(&(wpool->lock))!=0){
                    fprintf(stderr, "impossibile acquisire la lock della coda\n");
                    pthread_exit(NULL);
                }
            }

        }//fine for (while true)
    }//fine else lock

    //nonn ci dovrebbe mai arrivare
    exit(EXIT_FAILURE);
        
}


/**
 * @brief crea e inizializza la theradpool
 * 
 * @param numWorkers numero di thread sempre attivi
 * @param queueSize dimensione della coda delle task
 * @param fd_socket socket con cui i thead comunicano col collecor
 * @return workerpool_t*  o NULL in caso di errore
 */
workerpool_t * createWorkerpool (int numWorkers, int queueSize, int fd_socket){

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
    wpool->fd_socket=fd_socket;
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
    if((wpool->pendingQueue=(string*)malloc(sizeof(string)*queueSize))==NULL){
        //se fallisce la malloc
        perror("Fallisce l'allocazione della pendingQueue nella workerpool");
        //libero la memoria
        free(wpool->workers);
        free(wpool);
        return NULL;
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

    //inizializzo la mutex della socket
    if((pthread_mutex_init(&(wpool->conn_lock), NULL))==-1){
        perror("fallisce l'inizializzazione della mutex per la coda nella workerpool");
        free(wpool->workers);
        //adesso devo liberare anche la coda
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
        //libero ciascun elemento della coda e poi la coda stessa
        for(int i=0; i<wpool->queueSize; i++){
            free(wpool->pendingQueue[i]);
        }
        free(wpool->pendingQueue);
        pthread_mutex_destroy(&(wpool->lock));
        pthread_cond_destroy(&(wpool->cond));
        pthread_mutex_destroy(&(wpool->conn_lock));
        close(wpool->fd_socket);
        free(wpool);
    }
}

/**
 * @brief elimina la threadpool
 * 
 * @param wpool threadpool da eliminare
 * @param waitTask bool che decide se vanno prima finite di elaborare le task in coda
 * @return true pool elimnata correttamente
 * @return false pool eliminata in maiera non corretta fallimento
 */
bool destroyWorkerpool (workerpool_t* wpool, bool waitTask){
    puts("Inizio distruzione pool");
    if(wpool==NULL){
        errno = EINVAL;
        return false;
    }
    
    if(pthread_mutex_lock(&(wpool->lock))!=0){
        fprintf(stderr, "Errore fatale lock\n");
        return false;
    }else{
        //setto lo stato di uscita della threadpool
        wpool->exiting=true;
        //se sono in fase di uscita perchè ho finito le task da inviare alla queue  waitTsak=true altrimenti è un uscita forzata di emergenza.
        if(waitTask){
            //mando un segnale broadcast in modo da svegliare tutti i thread per terminare le task
            if(pthread_cond_broadcast(&(wpool->cond))!=0){
                if(pthread_mutex_unlock(&(wpool->lock))!=0){
                    fprintf(stderr,"Errore Fatale unlock\n");
                    return false;
                }
                return false;
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
        }else{
            if(pthread_mutex_unlock(&(wpool->lock))!=0){
                fprintf(stderr, "Errore unlock della coda delle task in destroy workerpool\n");
                return false;
            }
        }
    }//fine else lock

    freeWorkPool(wpool);
    //ho liberato la memoria della wpool
    return true;
}

int addTask (workerpool_t* wpool, string file){
   //controllo  che la wpool non sia NULL
    if(wpool==NULL){
        //setto errno
        errno=EINVAL;
        return -1;
    }

    //acquisico la lock
    if(pthread_mutex_lock(&(wpool->lock))!=0){
        fprintf(stderr, "Workerpool.c errore fatale lock");
        return -1;
    }else{
        if(wpool->exiting || wpool->queueSize<=wpool->pendingQueueCount || wpool->activeTask==wpool->numWorkers){
            //la pool è in uscita non accetto nuove task
            //la coda è piena non si accettano nuove task
            //tutti i thread sono occupati non sia ccettano nuove task
            if(pthread_mutex_unlock(&(wpool->lock))!=0){
                fprintf(stderr,"Errore unlock della coda delle task in add Task\n");
                return -1;
            }
            return 1;
        }
        
        
        wpool->pendingQueue[wpool->queueTail]=(string)malloc(sizeof(char)*(1+strlen(file)));
        
        strcpy(wpool->pendingQueue[wpool->queueTail], file);
        
        //incremento il numero delle task in coda
        ++wpool->pendingQueueCount;
        //incremento il valore del puntatore alla fine della coda
        
        ++wpool->queueTail;
        
        //se il puntatore alla fine della coda supera la dimensione della coda, sovrescrivo le task di testa 
        if(wpool->queueTail >= wpool->queueSize){
            wpool->queueTail=0;
            puts("Riporto la coda a 0");
        }
        puts("sveglio un thread");
        if(pthread_cond_signal(&(wpool->cond))!=0){
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

    }
    //tutto è andato a buon fine
    return 0;
}


/**
 * @brief funzione che calcola la formula assegnata sul file che viene passato come argomento
 * 
 * @param arg nome del file + puntatore alla wpool
 */
void leggieSomma (void*arg){

    string filePath = (*(wArg*)arg).path;
    workerpool_t* wpool = (*(wArg*)arg).pool;
    
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
    //converto il valore della somma in stringa e scrivo anche il nome del file
    sprintf(buffer, "%ld %s", somma, filePath);
    

    //eseguo la lock sulla socket per scrivere
    if(pthread_mutex_lock(&(wpool->conn_lock)) != 0){
        fprintf(stderr, "Errore lock socket\n");
        free(buffer);
        return;
    }else{
        //lock eseguita correttamente
        int sc;
        if((sc=writen(wpool->fd_socket, buffer ,strlen(buffer)+1))!=0){
            perror("write()");
            free(buffer);
            if(pthread_mutex_unlock(&(wpool->conn_lock))!=0){
               fprintf(stderr, "errore unlock socet\n");
               exit(EXIT_FAILURE);
            }
            return;
        }
        memset(buffer,'\0',FILE_BUFFER_SIZE+PATH_LEN);
        //scrittura avvenuta aspetto di ricevere OK dal collector prima di rilasciare la lock
        if(readn(wpool->fd_socket,buffer,3)==-1){
            fprintf(stderr, "il collector non ha ricevuto correttamente il file\n");
             if(pthread_mutex_unlock(&(wpool->conn_lock))!=0){
               fprintf(stderr, "errore unlock socet\n");
               exit(EXIT_FAILURE);
            }
            return;
        }
        if(pthread_mutex_unlock(&(wpool->conn_lock))!=0){
            fprintf(stderr, "impossibile fare la unlock della socket\n");
            free(buffer);
            return;
        }  
    }
    
    //libero la memoria
    free(buffer);
    return;
}

