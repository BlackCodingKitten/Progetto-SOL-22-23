/**
 * @file WorkerPool.c
 * @brief File di implementazione dell'interfaccia WorkerPool 
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <WorkerPool.h> 
#include <stdbool.h>
#include <stdarg.h>


/**
 * @function: wpoolWorker
 * @brief funzione eseguita dal workerche appartiene a wpool
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
        fprintf(stderr, "-36-pthread_mutex_lock(&(wpool->lock)): errore\n");
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
            fprintf(stderr, "-63-pthread_mutex_unlock(&(wpool->lock)):errore\n");
            return NULL;
        }
        //eseguo la task
        (*(task.funPtr))(task.arg);
        //acquisisco la lock per decrementare le activeTask
        if(pthread_mutex_lock(&(wpool->lock))!=0){
            fprintf(stderr, "-70-pthread_mutex_lock(&(wpool->lock)): errore\n");
            return NULL;
        }
        wpool->activeTask--;
    }    
    if(pthread_mutex_unlock(&(wpool->lock))!=0){
        fprintf(stderr, "-76-pthread_mutex_unlock(&(wpool->lock)):errore\n");
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
        perror("wpool=(workerpool_t*)malloc(sizeof(workerpool_t)");
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
        perror("wpool->workers=(pthread_t*)malloc(sizeof(pthread_t)*numWorkers)");
        //libero la wpool
        free(wpool);
        return NULL;
    }
    //alloco la queue di workertask
    if((wpool->pendingQueue=(workertask_t*)malloc(sizeof(workertask_t)*pendingSize))){
        //se fallisce la malloc
        perror("wpool->pendingQueue=(workertask_t*)malloc(sizeof(workertask_t)*pendingSize)");
        //libero la memoria
        free(wpool->workers);
        free(wpool);
        return NULL;
    }
    //inizializzo la mutex della threadpool
    if((pthread_mutex_init(&(wpool->lock), NULL))!=0){
        perror("pthread_mutex_init(&(wpool->lock), NULL)");
        free(wpool->workers);
        //adesso devo liberare anche la coda
        free(wpool->pendingQueue);
        free(wpool);
        return NULL;
    }
    //inizializzo la cond della threadpool
    if((pthread_cond_init(&(wpool->cond),NULL))!=0){
        perror("pthread_cond_init(&(wpool->cond),NULL)");
        free(wpool->workers);
        free(wpool->pendingQueue);
        free(wpool);
    }
    for(int i=0; i<numWorkers;i++){
        if(pthread_create(&(wpool->workers[i].wid),NULL,&wpoolWorker, (void*)wpool)!=0){
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
        if(pthread_join(wpool->workers[i].wid,NULL)!=0){
            errno=EFAULT;
            if(pthread_mutex_unlock(&(wpool->lock))!=0){
                fprintf(stderr,"Errore Fatale unlock\n");
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

bool spawnDetachedThread (void(*task)(void*),void*arg){
    //task=NULL è invalido
    if(task==NULL){
        errno=EINVAL;
        return false;
    }
    workertask_t*wtask;
    if((wtask=malloc(sizeof(workertask_t)))==NULL){
        perror("wtask=malloc(sizeof(workertask_t)");
        return false;
    }
    wtask->funPtr=task;
    wtask->arg=arg;

    pthread_t dThread;
    pthread_attr_t attr;
    //fa lo spawn di un thread in modalità detached
    if(pthread_attr_init(&attr)!=0){
        perror("pthread_attr_init(&attr)");
        return false;
    }
    if(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)!=0){
        perror("pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)");
        return false;
    }
    if(pthread_create(&dThread,&attr,&dThreadTask,(void*)wtask)!=0){
        perror("pthread_create(&dThread,&attr,&dThreadTask,(void*)wtask)");
        errno=EFAULT;
        free(wtask);
        return(false);
    }
    return true;
}

/**
 * @function dThreadTask
 * @brief task eseguita da un thread non appartenete al pool
 * 
 * @param arg worker_t che ha puntatore a funzione e argomenti della funzione a cui punta;
 */
static void* dThreadTask(void*arg){
    workertask_t*f=(workertask_t*)arg;
    //esegue la funzione
    (*(f->funPtr))(f->arg);
    free(f);
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
        fprintf(stderr, "pthread_mutex_lock(&(wpool->lock)): errore");
        return -1;
    }

    int qSize=abs(wpool->queueSize);
    //bool : dobbiamo gestire le task pendenti?
    bool noPending = (wpool->pendingQueue)==-1;

    //controllo se la coda è piena o se si è in fase di uscita:
    if(wpool->pendingQueueCount >= qSize || wpool->exiting){
        //unlock
        if(pthread_mutex_unlock(&(wpool->lock))!=0){
            fprintf(stderr, "-222-pthread_mutex_unlock(&(wpool->lock)): errore");
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
                fprintf(stderr, "-233-pthread_mutex_unlock(&(wpool->lock)): errore");
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
            fprintf(stderr,"-251-pthread_mutex_unlock(&(wpool->lock)): errore\n");
        }
        //ritorno -1 in ogni caso
        return -1;
    }

    //se ho aggiunto correttamente il thread rilascio la lock e return 0
    if(pthread_unlock_mutex(&(wpool->lock))!=0){
        fprintf(stderr, "-259-pthread_mutex_unlock(&(wpool->lock)): errore\n");
        return -1;
    }
    return 0;
}