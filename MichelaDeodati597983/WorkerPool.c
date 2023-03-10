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
long getFileSize(FILE *file) {
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
    printf("----DEBUG WorkerPool:  sono nella funzione wpoolWorker\n");
    printf("----DEBUG WorkerPool: casto la threadpool\n");
    workerpool_t * wpool =(workerpool_t*)pool;//cast della void*pool in una workerpool
    workertask_t task;//task generica
    //acquisisco la lock
    if(pthread_mutex_lock(&(wpool->lock))!=0){
        fprintf(stderr, "WORKERPOOL: in funzione wpoolWorker: errore acqusizione lock\n");
        pthread_exit(NULL);
    }
    printf("----DEBUG WorkerPool: dopo aver acquisito la lock entro nel while\n");fflush(stdout);
    while(true){
        printf("----DEBUG WorkerPool: sono nel while(true)\n");fflush(stdout);

        //rimango in attesa di un segnale, controllo dei wakeups spuri
        while((wpool->pendingQueueCount==0)&&(!wpool->exiting)){
            printf("----DEBUG WorkerPool: ASPETTO LA SIGNAL\n");fflush(stdout);
            pthread_cond_wait(&(wpool->cond), &(wpool->lock));
            printf("----DEBUG WorkerPool: HO RICEVUTO LA SIGNAL\n");fflush(stdout);  
        }
        //la pool è in fase di uscita e non ci sono più task pendenti, il thread esce dal while true
        if(wpool->exiting && (wpool->pendingQueueCount==0)){
            printf("----DEBUG WorkerPool: USCITA sono finite le task in coda\n");
            //eseguo la unlock
            if(pthread_mutex_unlock(&(wpool->lock))!=0){
                fprintf(stderr, "WORKERPOOL: in wpoolWorker errore unlock della coda in fase di uscita\n");
                pthread_exit(NULL);
            }
            break;
        }else{
            //il thread prende una nuova task dalla testa della threadpool
            printf("----DEBUG WorkerPool: sot prendendo una funzione dalla testa\n");fflush(stdout); 
            task.fun = wpool->pendingQueue[wpool->queueHead].fun;
            task.arg = wpool->pendingQueue[wpool->queueHead].arg;
            //diminuisco il numero di task in coda
            --wpool->pendingQueueCount;
            //sposto in avanti la testa di 1 
            ++wpool->queueHead;

            //se la testa è arrivata in fondo alla queue la riporto in cima;
            if(wpool->queueHead == wpool->queueSize){
                wpool->queueHead = 0;
            }
            //incremento il numero di task attive eseguite dai thread
            ++wpool->activeTask;
            printf("----DEBUG WorkerPool: incremento il numero delle active task e sveglio un'altro worker\n");fflush(stdout); 
            //sveglio uno dei thread in attesa per eseguire la prossima task
            pthread_cond_signal(&(wpool->cond));

            //eseguo la unlock sulla coda
            if(pthread_mutex_unlock(&(wpool->lock))!=0){
                fprintf(stderr, "WORKERPOOL: in wpoolWorker errore unlock della coda\n");
                pthread_exit(NULL);
            }
            printf("----DEBUG WorkerPool: dopo aver fatto la unlock eseguo la funzione\n");fflush(stdout); 
            //eseguo la funzione passata al thread:
            (*(task.fun))(task.arg);
            //dopo aver eseguito la task rieseguo la lock sulla coda per poter modificare il valore delle task attive
            if(pthread_mutex_lock(&(wpool->lock))!=0){
                fprintf(stderr, "WORKERPOOL: wpoolWorker, errore losck sulla mutex della coda delle task\n");
                pthread_exit(NULL);
            }
            printf("----DEBUG WorkerPool: funzione eseguita,  task attive diminuite\n");fflush(stdout); 
            --wpool->activeTask;
        }
    }//end while 
    //non serve esguire la unlock perchè l'ho eseguita prima nel while
    printf("----DEBUG WorkerPool: thread in uscita\n");fflush(stdout); 
    pthread_exit(NULL);
}



workerpool_t * createWorkerpool (int numWorkers, int queueSize){

    //questo controllo è quasi superfluo ma la sicurezza non è mai troppa
    if(numWorkers<=0 || queueSize <0){
        errno=EINVAL;
        return NULL;
    }
    printf("DEBUG WorkerPool: Inizializzo la workerpool con i valori n:%d q:%d\n", numWorkers, queueSize);
    //inizializzo la workerpool
    workerpool_t * wpool;
    if((wpool=(workerpool_t*)malloc(sizeof(workerpool_t)))==NULL){
        //se la malloc ritorna NULL stampo l'errore e ritorno NULL
        perror("Fallisce l'allocazione della workerpool");
        return NULL;
    }
    printf("DEBUG WorkerPool:setto le condizioni iniziali della workerpool\n");
   
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
    }
    for(int i=0; i<numWorkers;i++){
        if(pthread_create(&((wpool->workers[i]).wid), NULL, wpoolWorker, (void*)wpool)!=0){
            fprintf(stderr, "Fallisce la creazione dei thread della pool\n");
            //se anche solo 1 create non v a buon fine distruggo la pool e setto errno
            destroyWorkerpool(wpool);
            errno = EFAULT;
            return NULL;
        }
        //se la pthread_create() va a buon fine incremento numWorkers;
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

bool destroyWorkerpool (workerpool_t* wpool){
    if(wpool==NULL){
        errno = EINVAL;
        return false;
    }
    if(pthread_mutex_lock(&(wpool->lock))!=0){
        fprintf(stderr, "Errore fatale lock\n");
        return false;
    }
    wpool->exiting=true;


    //continuo ad inviare segnali fino a che la coda non si svuota
    while(wpool->pendingQueueCount!=0){
        if(pthread_cond_signal(&(wpool->cond))!=0){
            if(pthread_mutex_unlock(&(wpool->lock))!=0){
                fprintf(stderr,"Errore Fatale unlock\n");
            }
            errno=EFAULT;
            return false;
        }
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

int addTask (workerpool_t* wpool, void* voidFile){
    //controllo che la task non sia NULL e che la wpool non sia NULL
    if(wpool==NULL || voidFile == NULL){
        //setto errno
        errno=EINVAL;
        return -1;
    }
    printf("DEBUG Workerpool->addTask: Inizio fase di aggiunta di una nuova task\n"); fflush(stdout);
    //acquisico la lock
    if(pthread_mutex_lock(&(wpool->lock))!=0){
        fprintf(stderr, "Workerpool.c errore fatale lock");
        return -1;
    }
    printf("DEBUG Workerpool->addTask: eseguita la lock\n"); fflush(stdout);
    int qSize = wpool->queueSize;
    //controllo se la coda è piena o se si è in fase di uscita non accetto più alcuna task
    if((wpool->pendingQueueCount >= qSize || wpool->exiting) ){
        //unlock
        if(pthread_mutex_unlock(&(wpool->lock))!=0){
            fprintf(stderr, "Workerpool.c errore unlock");
            return -1;
        }
        //uscita col valore che indica queue piena in  modo da non far aggiungere altre task
        printf("DEBUG Workerpool->addTask: la coda è piena, unlock ed esco con value 1 \n"); fflush(stdout);
        return 1;
    }
    printf("DEBUG Workerpool->addTask: Passo gli argomenti della task: %s\n", *(string*)voidFile); fflush(stdout);
    //inserico in coda la nuova task
    wpool->pendingQueue[wpool->queueTail].arg=voidFile;
    //incremento il numero delle task in coda
    ++wpool->pendingQueueCount;
    //incremento il vlaore del puntatore alla fine della coda
    ++wpool->queueTail;
    //se il puntatore alla fine della coda supera la dimensione della coda, sovreascrivo le task di testa 
    if(wpool->queueTail >= qSize){
        printf("DEBUG Workerpool->addTask: il puntatore alla fine della coda ha raggiunto il limite dela coda lo rimetto in testa\n"); fflush(stdout);
        wpool->queueTail=0;
    }

    printf("DEBUG Workerpool->addTask: Mando una signal per svegliare i thread\n"); fflush(stdout);
    if(pthread_cond_signal(&(wpool->cond))== -1){
        //rilascio la lock
        if(pthread_mutex_unlock(&(wpool->lock))!=0){
            fprintf(stderr," fallimento unlock di emergenza dopo il fallimneot della signal in add task\n");
        }
        //ritorno -1 in ogni caso
        return -1;
    }
    printf("DEBUG Workerpool->addTask: SIGNAL mandata correttamente\n"); fflush(stdout);
    //se ho mandato correttamente il segnale rilascio la lock e return 0
    if(pthread_mutex_unlock(&(wpool->lock))!=0){
        fprintf(stderr, "Errore unlock riga 292 Workerpool.c\n");
        return -1;
    }
    printf("DEBUG Workerpool->addTask: uscita con valore 0 dalla funzione\n"); fflush(stdout);
    return 0;
}


void leggieSomma (void*arg){
    //creo la connessione tra il thread e il Collector
    printf("DEBUG WorkerPool: Entro nella funzione leggieSomma\n");

    struct sockaddr_un addr;
    strncpy(addr.sun_path,SOCKET_NAME,UNIX_PATH_MAX);
    addr.sun_family=AF_UNIX;
    int sock = socket(AF_UNIX,SOCK_STREAM,0);
    if(sock == -1){
        perror("Errore in leggie somma: socket()");
        exit(EXIT_FAILURE);
    }
    printf("DEBUG WorkerPool: Leggo la path del file\n");

    string filePath = *(string*)arg;

    printf("DEBUG WorkerPool:%s\n", filePath);
    long somma = 0;
    //apro il file in modalità lettura binaria
    FILE*file=fopen(filePath,"rb");
    if(file==NULL){
        perror(filePath);
        fprintf(stderr, "Errore apertura file:%s\n",filePath);
        pthread_exit(NULL);
    }
    printf("DEBUG WorkerPool: file aperto con successso\n");
    //ricavo quanti numeri ci sono nel file
    long fileDim=getFileSize(file)/ sizeof(long); 
    
    printf("DEBUG WorkerPool:nel file ci sono %ld numeri\n", fileDim);
    //alloco un array in cui inserisco tutti i valori dopo averli letti
    long* fileArray =(long*)malloc(sizeof(long)*fileDim);

    printf("DEBUG WorkerPool:ho allocato il file e leggo i valori\n");
    //leggo tutti i valori del file e li salvo in fileArray
    if(fread(fileArray,sizeof(long),fileDim,file)<=0){
        fprintf(stderr,"leggieSomma: errore lettura valori nel file %s", filePath);
        fclose(file);
        free(fileArray);
        return;
    }
    fclose(file);
    printf("DEBUG WorkerPool: ho letto i valori ora calcolo la somma\n");
    //sommo il prodotto del valore per l'indice
    for(int i=0; i<fileDim; i++){
        somma=somma+(fileArray[i]*i);
    }
    //dopo aver calcolato la somma libero la memoria
    free(fileArray);

    printf("DEBUG WorkerPool:la somma è venuta %ld \n", somma);
    //alloco il buffer per scrivere della dimensione 21 cioè 20= numero massimo di caratteri di maxlong +1 per il terminatore
    string buffer = malloc(sizeof(char)*FILE_BUFFER_SIZE+1);
    memset(buffer,'\0',FILE_BUFFER_SIZE+1);
    //converto il valore della somma in stringa
    sprintf(buffer, "%ld", somma);
    printf("DEBUG WORKERPOOL: valore della somma:%ld, convertito in stringa:%s\nDEBUG WorkerPool: Attendo la accept dal Collector\n", somma,buffer);
    
    //attendo che il collector faccia l'accept
    while(connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1){
        if(errno=ENOENT){
            sleep(1);
        }else{
            perror("connect()");
            CLOSE_SOCKET(sock);
            free(buffer);
            exit(EXIT_FAILURE);
        }
    }

    //la somma è da spedire al Collector con la write
    if(writen(sock,buffer,strlen(buffer)+1)==-1){
        perror("write()");
        CLOSE_SOCKET(sock);
        free(buffer);
        REMOVE_SOCKET();
        exit(EXIT_FAILURE);
    }
    //spedisco anche la path del file
    if(writen(sock, filePath,strlen(filePath)+1) ==- 1){
        perror("write()");
        CLOSE_SOCKET(sock);
        free(buffer);
        REMOVE_SOCKET();
        exit(EXIT_FAILURE);
    }

    //libero la memoria
    if(buffer!=NULL){
        free(buffer);
    }
    if(filePath!=NULL){
        free(filePath);
    }
    //al termine dell'invio al collector chiudo il client
    printf("DEBUG WorkerPool: spedisco somma e path e chiuso la socket\n");
    CLOSE_SOCKET(sock);
}

