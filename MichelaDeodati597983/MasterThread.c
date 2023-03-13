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
    int * stop;         //condizione di terminazione del while del masterthread;
    sigset_t*set;    // set dei segnali da gestire mascherati
    int signal_pipe;    //descrittore di scrittura di una pipe senza nome usato per comunicare al collector che è terminata l'esecuzione
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
    /*pthread_detach contrassegna un thread come come detach, in questo modo quando termina 
    le sue risorse vengono automaticamente restituite al sistema senza
    senza che sia necessario che un altro thread esegua una join*/
    if(pthread_detach(pthread_self())==-1){
        perror("pthread_detach(pthread_self) signal handeler");
        pthread_exit(NULL);
    }
    sigHarg*sArg = (sigHarg*)arg;
    sigset_t*set= sArg->set;
    printf("%p = indirizzo del set nel signal handler\n", sArg->set);
    int sig;
    while (!(*(sArg->stop))){
        //controllo che sigwait non ritorni un errore
        if(sigwait(set, &sig)==-1){
            errno=EINVAL;
            perror("SIGNAL_HANDLER-Masterthread.c-123: errore sigwait");
            exit(EXIT_FAILURE);
        }else{
            puts("ho pricevuto un segnale");
            switch (sig){
                case SIGINT:
                case SIGHUP:
                case SIGTERM:
                    //cambio il valore della guardia del while in masterthread
                    *(sArg->stop)=1;
                    
                    //notifico la ricezione del segnale al Collector e poi chiudo la pipe
                    if(writen(sArg->signal_pipe,"t", 2)==-1){
                        fprintf(stderr, "SIGNAL_HANDLER-Masterthread.c-131: errore writen sulla pipe");
                    }
                    close(sArg->signal_pipe); 
                    pthread_exit(NULL);          
                case SIGUSR1:
                    //invio al collector il segnale di stampa
                    if(writen(sArg->signal_pipe, "s", 2)==-1){
                        fprintf(stderr, "SIGNAL_HANDLER-Masterthread.c-137: errore writen sulla pipe");
                    }
                default:
                    continue;
                    break;
            }
        }
        
    }//end while(true)
    pthread_exit(NULL); 
}

/**
 * @brief core del masterthread che esegue tutte le funzioni richieste: creazione della workerpool
 * gestione dei segnali, fork per creare il processo figlio collector, terminazione della threadpool
 */
int runMasterThread(int n, int q, int t, int numFilePassati, string * files){
    //creo la struct timespec a cui passo parametro t come tv_sec
    struct timespec delay = {t,0};
    //creo la condizione del while per terminare in caso di ricezione dei segnali SIGHUP,SIGINT,SIGTERM
    int stop=0;
    //creo la threadpool con la funzione createWorkerpool e controllo che la creazione della threadpool si concluda con successo
    workerpool_t *wpool=NULL;    
    if((wpool =createWorkerpool(n, q))==NULL){
        fprintf(stderr, "MASTERTHREAD: Errore fatale nella creazione della workerpool\n");
        return EXIT_FAILURE;
    }
    //ignoro il segnale sigpipe per evitare di essere terminato da una scrittura su socket
    struct sigaction s;
    memset(&s,0,sizeof(s));
    s.sa_handler=SIG_IGN;
    if((sigaction(SIGPIPE,&s,NULL))==-1){
        perror("sigaction()");
        destroyWorkerpool(wpool,false);
        return EXIT_FAILURE;
    }

    //creo la pipe che mi permette di segnalare al collector tramite il signalHandler che sto terminando o per dirgli di stamapare
    int s_pipe[2];
    if(pipe(s_pipe)==-1){
        perror("Masterthread: errore esecuzione pipe()");
        destroyWorkerpool(wpool,false);
        return EXIT_FAILURE;
    }
    //Eseguo la fork, il processo padre(stesso proceso del main) continuerà ad essere il MasterThread,  mentre il processo figlio invocherà la funzione che gestisce il collector
    pid_t process_id=fork();
    if(process_id<0){
        fprintf(stderr, "MASTERTHREAD: Errore la fork ha ritornato un id negativo process_id=%d\n", process_id);
        destroyWorkerpool(wpool,false);
        REMOVE_SOCKET();
        return EXIT_FAILURE;
    }else{
        // inizializzo il signal handler nel processo padre dopo aver fatto la fork
        sigset_t mask;
        if(sigemptyset(&mask)==-1){
            perror("sigemptset()");
            destroyWorkerpool(wpool,false);
            return  EXIT_FAILURE;
        }
        sigaddset(&mask, SIGINT);
        sigaddset(&mask, SIGTERM);
        sigaddset(&mask, SIGHUP);
        sigaddset(&mask, SIGUSR1);
        if(process_id==0){
            //sono nel processo figlio: avvio il processo collector, gli passo signal_pipe[0] per leggere quello che scrive il signal hander su signal_pipe[1]
            runCollector(numFilePassati,s_pipe[0]);
        }else{
            //blocco i segnali per poterli gestire in  maniera custom
            if (pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0){
                //se da errore non setta errno
                fprintf(stderr, "fatal error pthread_sigmask\n");
                destroyWorkerpool(wpool,false);
                //notifico al processo figlio che deve terminare:
                int err=writen(s_pipe[1],"t", 2);
                if(err==-1){
                fprintf(stderr, "Impossibile comunicare con il Collector\n");
            }
            REMOVE_SOCKET();
            return EXIT_FAILURE;
            }
            
            //setto gli argomenti da passare al thread
            sigHarg*argSH =(sigHarg*)malloc(sizeof(sigHarg));
            argSH->set=&mask;
            printf("%p = indirizzo del set nel MT\n", argSH->set);fflush(stdout);
            argSH->signal_pipe=s_pipe[1];
            argSH->stop=&stop;

            //creo il thread signal handler in modalità detached
            pthread_t sigHandler;
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
            if (pthread_create(&sigHandler, NULL, &sigHandlerTask, (void *)&argSH) != 0){
                fprintf(stderr, "errore nella creazione del sighandler\n");
                destroyWorkerpool(wpool,false);
                int err = writen(s_pipe[1],"t", 2);
                if(err==-1){
                    fprintf(stderr, "Impossibile comunicare con il Collector\n");
                }
                REMOVE_SOCKET();
                free(argSH);
                return EXIT_FAILURE;
            }
            
            int index=0; //indice per scorrere files
            //itero fino a che non termina con segnale o fino a che non ho mandato tutti i file 
            while(!stop){
                nanosleep(&delay,NULL);
                int check = addTask(wpool, (void*)&files[index]);
                if(check==0){
                    //incremento l'indice solo se riesco ad assegnare correttamente la task alla threadpool
                    ++index;
                   //se non è stato passato alcun t dorme 0
                   if(index==numFilePassati){
                        goto chiusura;
                   }
                    continue;
                }else{
                    if(check==1){
                        //ritorna 1 qundo la coda è piena
                        continue;
                    }else{
                        //check==-1 c'è stato un errore grave nella addTask notifico al collector che deve termianre ed esco 
                        int err = writen(s_pipe[1],"t", 2);
                        if(err==-1){
                            fprintf(stderr, "Impossibile comunicare con il Collector\n");
                        }
                        destroyWorkerpool(wpool,false);
                        REMOVE_SOCKET();
                        return EXIT_FAILURE;
                    }
                }
            }

 chiusura:
            //distruggo la threadpool ma aspetto che siano completate le task pendenti
            if(!destroyWorkerpool(wpool,true)){
                REMOVE_SOCKET();
                return EXIT_FAILURE;
            }
            //aspetto che termini il collector
            waitpid(process_id,NULL,0);
            REMOVE_SOCKET();
            free(argSH);
            pthread_attr_destroy(&attr);
            puts("Master in uscita");
            stop=1; //termino anche il signal handler
            return EXIT_SUCCESS;
        }
    }
    //non ci dovrebbe arrivare mai
    return EXIT_FAILURE;
}