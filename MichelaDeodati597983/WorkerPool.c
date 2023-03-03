#define _POSIX_C_SOURCE 2001112L
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
#include <WorkerPool.h> 
#include <stdbool.h>
#include <Util.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/un.h>



/**
 * @function: wpoolWorker
 * @brief funzione eseguita dal worker che appartiene a wpool
 * 
 * @param pool workerpool_t
 */
static void * wpoolWorker(void* pool){
    //cast della void*pool in una workerpool
    workerpool_t * wpool =(workerpool_t*)pool;
    //task generica
    workertask_t task;
    workerthread_t self;
    self.wid=pthread_self();
    self.wpoolIndex=0;
    while(!pthread_equal(wpool->workers[self.wpoolIndex].wid,self.wid)){
        self.wpoolIndex++;
    }
    //acquisisco la lock
    if(pthread_mutex_lock(&(wpool->lock))!=0){
        fprintf(stderr, "errore acqusizione della lock da parte del thread:%d\n", self.wpoolIndex);
        return NULL;
    }
    while(true){
        while((wpool->pendingQueueCount==0)&&(!wpool->exiting)){
            pthread_cond_wait(&(wpool->cond), &(wpool->lock));
        }
        
        if(wpool->exiting && !wpool->waitEndingTask){
            //esco ignorando le task pendenti
            break;
        }
        if(wpool->exiting && wpool->waitEndingTask){
            //esco ma ci sono task pendenti da terminare
            break;
        }
        //nuovo task
        task.funPtr=wpool->pendingQueue[wpool->queueHead].funPtr;
        task.arg=wpool->pendingQueue[wpool->queueHead].arg;

        wpool->queueHead++;
        wpool->pendingQueueCount--;
        if(wpool->queueHead==abs(wpool->queueSize)){
            wpool->queueHead=0;
        }
        wpool->activeTask++;
        if(pthread_mutex_unlock(&(wpool->lock))!=0){
            fprintf(stderr, "errore nella unlock da parte del thread:%d\n", self.wpoolIndex);
            return NULL;
        }
        //eseguo la task
        (*(task.funPtr))(task.arg);
        //acquisisco la lock per decrementare le activeTask
        if(pthread_mutex_lock(&(wpool->lock))!=0){
            fprintf(stderr, "errore acqusizione della lock per decrementare le activeTask da parte del thread:%d\n", self.wpoolIndex);
            return NULL;
        }
        wpool->activeTask--;
    }    
    if(pthread_mutex_unlock(&(wpool->lock))!=0){
        fprintf(stderr, "errore nella unlock da parte del thread:%d\n", self.wpoolIndex);
        return NULL; 
    }
    fprintf(stderr, "Worker %d in uscita.\n", self.wpoolIndex);
    return NULL;
}



workerpool_t * createWorkerpool (int numWorkers, int pendingSize){
    if(numWorkers<=0 || pendingSize <0){
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
    if(pendingSize==0){
        //non ci sono task pendenti
        wpool->queueSize = -1;
    }else{
        wpool->queueSize = pendingSize;
    }
    wpool->queueHead = wpool->queueTail = 0;
    wpool->pendingQueueCount=0;
    wpool->exiting=false;
    wpool->waitEndingTask=false;

    //alloco l'array di workers
    if((wpool->workers=(workerthread_t*)malloc(sizeof(workerthread_t)*numWorkers))==NULL){
        //se fallisce la malloc
        perror("Fallisce l'allocazione dell'array di theread nella workerpool");
        //libero la wpool
        free(wpool);
        return NULL;
    }
    //alloco la queue di workertask
    if((wpool->pendingQueue=(workertask_t*)malloc(sizeof(workertask_t)*pendingSize))){
        //se fallisce la malloc
        perror("Fallisce l'allocazione della pendingQueue nella workerpool");
        //libero la memoria
        free(wpool->workers);
        free(wpool);
        return NULL;
    }
    //inizializzo la mutex della threadpool
    if((pthread_mutex_init(&(wpool->lock), NULL))!=0){
        perror("fallisce l'inizializzazione della mutex per la coda nella workerpool");
        free(wpool->workers);
        //adesso devo liberare anche la coda
        free(wpool->pendingQueue);
        free(wpool);
        return NULL;
    }
    //inizializzo la cond della threadpool
    if((pthread_cond_init(&(wpool->cond),NULL))!=0){
        perror("Fallisce l'inizializzazione della condition nella workerpool");
        free(wpool->workers);
        free(wpool->pendingQueue);
        free(wpool);
    }
    for(int i=0; i<numWorkers;i++){
        if(pthread_create(&(wpool->workers[i].wid),NULL,&wpoolWorker, (void*)wpool)!=0){
            fprintf(stderr, "Fallisce la creazione dei thread della pool\n");
            //se la pthread_create() non va a buon fine,
            //libero la memoria e forzo l'uscita dei thread
            destroyWorkerpool(wpool,true);
            //setto errno
            errno = EFAULT;
            return NULL;
        }
        //se la pthread_create() va a buon fine incremento numWorkers;
        wpool->numWorkers++;
        wpool->workers[i].wpoolIndex=i;
    }
    return wpool;
}

bool destroyWorkerpool (workerpool_t* wpool, bool forcedExit){
    if(wpool==NULL){
        errno = EINVAL;
        return false;
    }
    if(pthread_mutex_lock(&(wpool->lock))!=0){
        fprintf(stderr, "Errore fatale lock\n");
        return false;
    }
    wpool->exiting=true;
    wpool->waitEndingTask=!forcedExit;

    if(pthread_cond_broadcast(&(wpool->cond))!=0){
        if(pthread_mutex_unlock(&(wpool->lock))!=0){
            fprintf(stderr,"Errore Fatale unlock\n");
        }
        errno=EFAULT;
        return false;
    }
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
    freeWorkPool(wpool);
    return true;
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


int addToWorkerpool (workerpool_t* wpool, void(*task)(void*), void*arg){
    //controllo che la task non sia NULL e che la wpool non sia NULL
    if(wpool==NULL || task==NULL){
        //setto errno
        errno=EINVAL;
        return -1;
    }

    //acquisico la lock
    if(pthread_mutex_lock(&(wpool->lock))!=0){
        fprintf(stderr, "Errore fatale lock riga 241 Workerpool.c");
        return -1;
    }

    int qSize=abs(wpool->queueSize);
    //bool : dobbiamo gestire le task pendenti? se è == -1 non vanno gestite quindi no pending diventa false
    bool noPending = (wpool->pendingQueue)==-1;

    //controllo se la coda è piena o se si è in fase di uscita:
    if(wpool->pendingQueueCount >= qSize || wpool->exiting){
        //unlock
        if(pthread_mutex_unlock(&(wpool->lock))!=0){
            fprintf(stderr, "Errore unlock riga 252 Workerpool.c");
            return -1;
        }
        //uscita col valore che indica queue piena
        return 1;
    }
    //se le task attive sono maggiori/uguali del numero di workers ritorno queue piena
    if(wpool->activeTask >= wpool->numWorkers){
        if(noPending){
            //le task pendenti non vengono gestite perchè i workers sono tutti occupati
            assert(wpool->pendingQueueCount == 0);
            if(pthread_mutex_unlock(&(wpool->lock))!=0){
                fprintf(stderr, "Errore unlock riga 264 Workerpool.c");
                return -1;
            }
            return 1;
        }
    }

    //inserico in coda la nuova task
    wpool->pendingQueue[wpool->queueTail].funPtr=task;
    wpool->pendingQueue[wpool->queueTail].arg=arg;
    wpool->activeTask++;
    wpool->queueTail++;
    if(wpool->queueTail >= qSize){
        wpool->queueTail=0;
    }
    int err;
    if((err=pthread_cond_signal(&(wpool->cond)))!=0){
        //setto errno
        errno=err;
        //rilascio la lock
        if(pthread_mutex_unlock(&(wpool->lock))!=0){
            fprintf(stderr,"Errore unlock riga 284 Workerpool.c\n");
        }
        //ritorno -1 in ogni caso
        return -1;
    }

    //se ho aggiunto correttamente il thread rilascio la lock e return 0
    if(pthread_unlock_mutex(&(wpool->lock))!=0){
        fprintf(stderr, "Errore unlock riga 292 Workerpool.c\n");
        return -1;
    }
    return 0;
}


void leggieSomma (void*arg){
    //creo la connessione tra il thread e il Collector
    struct sockaddr_un addr;
    strncpy(addr.sun_path,SOCKET_NAME,UNIX_PATH_MAX);
    addr.sun_family=AF_UNIX;
    int sock =socket(AF_UNIX,SOCK_STREAM,0);
    if(sock==-1){
        perror("socket()");
        exit(EXIT_FAILURE);
    }
    string filePath = *(string)arg;
    long somma = 0;
    //apro il file in modalità lettura binaria
    FILE*file=fopen(filePath,'rb');
    if(file==NULL){
        perror(file);
        fprintf(stderr, "Errore apertura file:%s\n",filePath);
        //----------------------------------------------------------
    }
    long fileDim=getFileSize(file)/ sizeof(long); //ricavo quanti numeri ci sono nel file
    //alloco un array in cui inserisco tutti i valori
    long* fileArray =(long*)malloc(sizeof(long)*fileDim);
    //leggo tutti i valori del file 
    fread(fileArray,sizeof(long),fileDim,file);

    //sommo il prodotto del valore per l'indice
    for(int i=0; i<fileDim; i++){
        somma=somma+(fileArray[i]*i);
    }
    //alloco il buffer per scrivere
    string buffer = malloc(sizeof(char)*FILE_BUFFER_SIZE);
    memeset(buffer,'\0',FILE_BUFFER_SIZE);
    sprintf(buffer, "%ld", somma); //converto il valore della somma in stringa
    //attendo che il collector faccia l'accept
    while(connect(sock, (struct sockaddr*)&addr,sizeof(addr))==-1){
        if(errno=ENOENT){
            sleep(1);
        }else{
            perror("connect()");
            CLOSE_SOCKET(sock);
            exit(EXIT_FAILURE);
        }
    }
    //la somma è da spedire al Collector con la write
    if(writen(sock,buffer,strlen(buffer)+1)==-1){
        perror("write()");
        CLOSE_SOCKET(sock);
        REMOVE_SOCKET();
        exit(EXIT_FAILURE);
    }
    if(buffer!=NULL){
        free(buffer);
    }
    //spedisco anche la path del file
    if(writen(sock, filePath,strlen(filePath)+1)==-1){
        perror("write()");
        CLOSE_SOCKET(sock);
        REMOVE_SOCKET();
        exit(EXIT_FAILURE);
    }
    //al termine dell'invio al collector chiudo il client
    CLOSE_SOCKET(sock);
}

/**
 * @brief ricavare la dimensione in byte del file 
 * 
 * @param file nome del file
 * @return long 
 */
long getFileSize(FILE *file) {
    long size;

    fseek(file, 0, SEEK_END);  // si posiziona alla fine del file
    size = ftell(file);        // legge la posizione attuale nel file, che corrisponde alla sua dimensione in byte
    fseek(file, 0, SEEK_SET);  // si riporta all'inizio del file

    return size;
}