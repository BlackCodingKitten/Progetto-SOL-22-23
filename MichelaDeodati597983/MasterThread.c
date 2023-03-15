/**
 * @file MasterThread.c
 * @author Michela Deodati 597983
 * @brief implementazione dell'intefraccia MasterThread
 * @version 0.1
 * @date 15-03-2023
 * 
 * 
 */
#define _GNU_SOURCE
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 2001112L
#endif
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <getopt.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <signal.h>

#include <Collector.h>
#include <WorkerPool.h>
#include "./MasterThread.h"

/**
 * @brief @struct struct di argomenti da passare al signal handler
 */
typedef struct s{
    int * stop;                     //condizione di terminazione del while del masterthread;
    sigset_t*set;                   // set dei segnali da gestire mascherati
    int signal_socket;              //descrittore di scrittura di una pipe senza nome usato per comunicare al collector che è terminata l'esecuzione
    pthread_mutex_t * socketMutex;    //mutex per poter scrivere sulla socket
    workerpool_t *wpool;
}sigHarg;

/**
 * @brief task del thread signal handler che gestisce i segnali di terminazione e i segnali di stampa 
 * invia un messaggio sulla pipe passata come argomento per comunicare al collector la terminazione o 
 * che deve stampare
 * 
 * @param arg viene passata al struct sigHarg in cui sono settati i valori 
 * @return void* 
 */
static void* sigHandlerTask (void*arg){
    //casto arg
    sigHarg sArg = *((sigHarg*)arg);
   
    int sig;
    while (true){
        //controllo che sigwait non ritorni un errore
        if(sigwait((*((sigHarg*)arg)).set, &sig)==-1){
            errno=EINVAL;
            perror("SIGNAL_HANDLER-Masterthread.c-123: errore sigwait");
            close(sArg.signal_socket);
            exit(EXIT_FAILURE);
        }else{
            //attendo che il collector faccia l'accept
            //puts("ho ricevuto un segnale");
            switch (sig){
                case SIGUSR2:
                    return NULL;
                case SIGINT:
                case SIGHUP:
                case SIGTERM:                  
                    //scrivo al collector che è stato inviato un segnale di terminazione
                    destroyWorkerpool(sArg.wpool,true);
                    if(pthread_mutex_lock(sArg.socketMutex) != 0){
                        fprintf(stderr, "Errore lock socket\n");
                        return NULL;
                    }else{
                        //lock eseguita correttamente
                        //spedisco al Collector con la writen "exit"
                        int sc;
                        if((sc=writen(sArg.signal_socket, "exit" ,strlen("exit")+1))!=0){
                            perror("write()");
                            return NULL;
                        }
                        puts("SIGNAL HANDLER SCRITTURA AVVENUTA");
                        //aspetto di ricevere K dal collector prima di rilasciare la lock
                        char k[2];
                        if(readn(sArg.signal_socket,k,2)==-1){
                            fprintf(stderr, "il collector non ha ricevuto correttamente il file\n");
                        }
                        printf("SIGNAL HANDLER LEGGE: %s\n",k);
                        if(pthread_mutex_unlock(sArg.socketMutex)!=0){
                            fprintf(stderr, "impossibile fare la unlock della socket\n");
                            return NULL;
                        }
                    }
                    sArg.wpool=NULL;
                    (*sArg.stop)=1;
                    return NULL;        
                case SIGUSR1:
                    if(pthread_mutex_lock(sArg.socketMutex) != 0){
                        fprintf(stderr, "Errore lock socket\n");
                        return NULL;
                    }else{
                        //lock eseguita correttamente
                        //spedisco al Collector con la writen "stampa"
                        int sc;
                        if((sc=writen(sArg.signal_socket, "stampa" ,strlen("stampa")+1))!=0){
                            perror("write()");
                            return NULL;
                        }
                        
                        //aspetto di ricevere K dal collector prima di rilasciare la lock
                        char k[2];
                        if(readn(sArg.signal_socket,k,2)==-1){
                            fprintf(stderr, "il collector non ha ricevuto correttamente il file\n");
                        }
                        printf("SIGNAL HANDLER LEGGE: %s dopo aver inviato stampa\n",k);
                        if(pthread_mutex_unlock((*(wArg*)arg).socketMutex)!=0){
                            fprintf(stderr, "impossibile fare la unlock della socket\n");
                            return NULL;
                        }
                    }
                    break;
                default:
                    break;
            }
        }
        
    }//end while(true)
    return NULL; 
}

/**
 * @brief core del masterthread che esegue tutte le funzioni richieste: creazione della workerpool
 * gestione dei segnali, fork per creare il processo figlio collector, terminazione della threadpool
 */
int runMasterThread(int n, int q, int t, int numFilePassati,sigset_t mask, string * files){
    //creo la struct timespec a cui passo parametro t come tv_sec
    struct timespec delay = {t,0};

    struct sockaddr_un addr;
    strncpy(addr.sun_path,SOCKET_NAME,UNIX_PATH_MAX);
    addr.sun_family=AF_UNIX;
    //creo la socket che mi permetterà di cominicare tra i worker e il collector, ne aggiungo una in più per il signal handler.
    int MasterSocket = socket(AF_UNIX,SOCK_STREAM,0);
    if(MasterSocket == -1){
        perror("socket() MasterThread");
        return EXIT_FAILURE;
    }
    //bind della MasterSocket che si metterà in ascolto del collector
    if(bind(MasterSocket,(struct sockaddr*)&addr, sizeof(addr))!=0){
        perror("bind(farm.sck)");
            close(MasterSocket);
            REMOVE_SOCKET();
            return (EXIT_FAILURE);
    }
    //metto la listen socket in ascolto
    if(listen(MasterSocket,SOMAXCONN)==-1){
        perror("listen()");
        close(MasterSocket);
        REMOVE_SOCKET();
        return EXIT_FAILURE;
    }


    //creo e inizializzo la mutex sulla socket per comunicare col collector
    pthread_mutex_t socketMutex;
    if((pthread_mutex_init(&socketMutex, NULL))==-1){
        perror("fallisce l'inizializzazione della mutex per la socket di comunicazione col collector");
        close(MasterSocket);
        return EXIT_FAILURE;
    }

    //creo la condizione del while per terminare in caso di ricezione dei segnali SIGHUP,SIGINT,SIGTERM
    int* stop=(int*)malloc(sizeof(int));
    *stop=0;

    //creo la threadpool con la funzione createWorkerpool e controllo che la creazione della threadpool si concluda con successo
    workerpool_t *wpool=NULL;    
    if((wpool =createWorkerpool(n, q))==NULL){
        fprintf(stderr, "MASTERTHREAD: Errore fatale nella creazione della workerpool\n");
        pthread_mutex_destroy(&socketMutex);
        close(MasterSocket);
        free(stop);
        return EXIT_FAILURE;
    }
   
    //Eseguo la fork, il processo padre  continuerà ad essere il MasterThread,  mentre il processo figlio invocherà la funzione che gestisce il collector
    pid_t process_id=fork();
    if(process_id<0){
        //id <0 la fork ha dato errore
        fprintf(stderr, "MASTERTHREAD: Errore la fork ha ritornato un id negativo process_id=%d\n", process_id);
        destroyWorkerpool(wpool,false);
        pthread_mutex_destroy(&socketMutex);
        close(MasterSocket);
        free(stop);
        return EXIT_FAILURE;
    }else{
        if(process_id==0){
            //sono nel processo figlio: avvio il processo collector, gli passo quanti file ci sono da stampare
            runCollector(numFilePassati);
        }else{
            //accetto la connessione col collector controllando che avvenga in maiera corretta
            int collectorSocket=accept(MasterSocket,NULL,0);
            if(collectorSocket==-1){
                destroyWorkerpool(wpool,false);
                pthread_mutex_destroy(&socketMutex);
                close(MasterSocket);
                REMOVE_SOCKET();
                free(stop);
                return EXIT_FAILURE;
            }
            //setto gli argomenti da passare al thread signal handler
            sigHarg argSH ={stop, &mask, collectorSocket, &socketMutex,wpool};
            //creo il thread signal handler e gli passo gli argomenti appena settati
           
            pthread_t sigHandler;
            if(pthread_create(&sigHandler, NULL, &sigHandlerTask, (void *)&argSH) != 0){
                fprintf(stderr, "errore nella creazione del sighandler\n");
                destroyWorkerpool(wpool,false);
                pthread_mutex_destroy(&socketMutex);
                close(MasterSocket);
                REMOVE_SOCKET();
                free(stop);
                return EXIT_FAILURE;
            }
            int index=0; //indice per scorrere files

            //itero fino a che stop!=1, fino a che non ho mandato tutti i file o se il Collector è terminato per un qualunque motivo inaspettato
            int collectorTerminato=0;
            
            while(!(*stop) && ((collectorTerminato=waitpid(process_id,NULL,WNOHANG))!=process_id)){
                //se non è stato passato alcun -t dorme 0
                //creo setto gli argomenti da mettere in coda
                wArg workerArg = {files[index], collectorSocket, &socketMutex};
                //printf("\nMASTERTHREAD:");puts(files[index]);puts("\n");
                int check = addTask(wpool, (void*)&workerArg);
                
                if(check==0){
                    //incremento l'indice solo se riesco ad assegnare correttamente la task alla threadpool
                    ++index;
                    if(index==numFilePassati){
                        *stop=1;
                    }
                    nanosleep(&delay,NULL);
                    continue;
                }else{
                    if(check==1){
                        //ritorna 1 qundo la coda è piena
                        continue;
                    }else{
                        //check==-1, c'è stato un errore grave nella addTask notifico al collector che deve termianre sulla socket del signal handler ed esco 
                        pthread_kill(sigHandler,SIGUSR2);
                        pthread_join(sigHandler,NULL);
                        destroyWorkerpool(wpool,false);
                        close(MasterSocket);
                        free(stop);
                        pthread_mutex_destroy(&socketMutex);                        
                        REMOVE_SOCKET();
                        return EXIT_FAILURE;
                    }
                }
            }
           
            //il collector è terminato prima del masterthread, c'è un errore, libero le risorse e ritorno EXIT_FAILURE
            if(collectorTerminato==process_id){
                destroyWorkerpool(wpool,false);
                *(stop)=1;
                pthread_kill(sigHandler,SIGUSR2);
                pthread_join(sigHandler,NULL);
                close(MasterSocket);
                REMOVE_SOCKET();
                free(stop);
                pthread_mutex_destroy(&socketMutex);
                return  EXIT_FAILURE;
            }

            //distruggo la threadpool ma aspetto che siano completate le task pendenti
            if(!destroyWorkerpool(wpool,true)){
                pthread_kill(sigHandler, SIGUSR2);
                pthread_join(sigHandler,NULL);
                close(MasterSocket);
                pthread_mutex_destroy(&socketMutex);
                REMOVE_SOCKET();
                free(stop);
                return EXIT_FAILURE;
            }
            //chiudo la MasterSocket e aspetto che termini il collector
            close(MasterSocket);            
            waitpid(process_id,NULL,0);
            pthread_kill(sigHandler, SIGUSR2);
            pthread_join(sigHandler,NULL);
            pthread_mutex_destroy(&socketMutex);
            free(stop);
            REMOVE_SOCKET();
           
            return EXIT_SUCCESS;
        }
    }
    //non ci dovrebbe arrivare mai
    return EXIT_FAILURE;
}