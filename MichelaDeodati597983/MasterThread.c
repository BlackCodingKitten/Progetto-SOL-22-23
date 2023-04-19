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
    workerpool_t *wpool;            //threadpool
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
    workerpool_t*wpool=sArg.wpool;
   
    int sig;
    while (true){
        //controllo che sigwait non ritorni un errore
        if(sigwait((*((sigHarg*)arg)).set, &sig)==-1){
            errno=EINVAL;
            perror("SIGNAL_HANDLER-Masterthread.c-123: errore sigwait");
            REMOVE_SOCKET();
            exit(EXIT_FAILURE);
        }else{
            puts("ho ricevuto un segnale");
            switch (sig){
                case SIGINT:
                case SIGHUP:
                case SIGTERM:                  
                    if(*(sArg.stop)==0){
                        if(pthread_mutex_lock(&(wpool->conn_lock))!=0){
                            fprintf(stderr, "impossibile acquisire la lock della connessione\n");
                            exit(EXIT_FAILURE);
                        }else{
                            while(wpool->can_write==0){
                                pthread_cond_wait(&(wpool->conn_cond), &(wpool->conn_lock));
                            }
                            wpool->can_write=0;
                            if(writen(wpool->fd_socket, "t", 2)<0){
                                fprintf(stderr, "impossibile notificare al collector la terminazione\n");
                                exit(EXIT_FAILURE);
                            }
                            //attendo la risposta dal collector 
                            char ok[3];
                            if(readn(wpool->fd_socket, ok, 3)<0){
                                fprintf(stderr, "risposta dal collector non ricevuta\n");
                                exit(EXIT_FAILURE);
                            }
                            wpool->can_write=1;
                            if(pthread_cond_signal(&(wpool->conn_cond))!=0){
                                fprintf(stderr, "Errore, impossibile svegliare nuovi thread\n");
                                exit(EXIT_FAILURE);
                            }
                            wpool->exiting=true;
                            if(pthread_mutex_unlock(&(wpool->conn_lock))!=0){
                                fprintf(stderr,  "impossibile eseguire la unlock della connessione\n");
                                exit(EXIT_FAILURE);
                            }
                            *(sArg.stop)=1;
                            pthread_exit(NULL);
                        }
                    }else {
                        pthread_exit(NULL);
                    }   
                case SIGUSR1:
                    if(pthread_mutex_lock(&(wpool->conn_lock))!=0){
                        fprintf(stderr, "impossibile acquisire la lock della connessione\n");
                        exit(EXIT_FAILURE);
                    }else{
                        while(wpool->can_write==0){
                            pthread_cond_wait(&(wpool->conn_cond), &(wpool->conn_lock));
                        }
                        wpool->can_write=0;
                        if(writen(wpool->fd_socket, "s", 2)<0){
                            fprintf(stderr, "impossibile notificare al collector la terminazione\n");
                            exit(EXIT_FAILURE);
                        }
                        //attendo la risposta dal collector 
                        char ok[3];
                        if(readn(wpool->fd_socket, ok, 3)<0){
                            fprintf(stderr, "risposta dal collector non ricevuta\n");
                            exit(EXIT_FAILURE);
                        }
                        wpool->can_write=1;
                        if(pthread_cond_signal(&(wpool->conn_cond))!=0){
                            fprintf(stderr, "Errore, impossibile svegliare nuovi thread\n");
                            exit(EXIT_FAILURE);
                        }
                        wpool->exiting=true;
                        if(pthread_mutex_unlock(&(wpool->conn_lock))!=0){
                            fprintf(stderr,  "impossibile eseguire la unlock della connessione\n");
                            exit(EXIT_FAILURE);
                        }
                        *(sArg.stop)=1;
                        pthread_exit(NULL);
                    }
                    break;
                default:
                    fprintf(stderr, "Catturato segnale non riconosciuto, errore\n");
                    exit(EXIT_FAILURE);
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
int runMasterThread(int n, int q, int t, int numFilePassati, sigset_t mask, string * files){
    //creo la socket che mi permetterà di comunicare tra i worker e il collector
    struct sockaddr_un addr;
    strncpy(addr.sun_path,SOCKET_NAME,UNIX_PATH_MAX);
    addr.sun_family=AF_UNIX;
    int MasterSocket = socket(AF_UNIX,SOCK_STREAM,0);
    if(MasterSocket == -1){
        perror("socket() MasterThread");
        return EXIT_FAILURE;
    }

    //bind della MasterSocket che si metterà in ascolto del collector
    if(bind(MasterSocket,(struct sockaddr*)&addr, sizeof(addr))!=0){
            REMOVE_SOCKET();
            if(bind(MasterSocket,(struct sockaddr*)&addr, sizeof(addr))!=0){
                perror("bind()");
                return (EXIT_FAILURE);
            }
    }

    //mi metto in ascolto
    if(listen(MasterSocket,SOMAXCONN)==-1){
        perror("listen()");
        REMOVE_SOCKET();
        return EXIT_FAILURE;
    }

    //creo la condizione del while per terminare in caso di ricezione dei segnali SIGHUP,SIGINT,SIGTERM
    int* stop=(int*)malloc(sizeof(int));
    *stop=0;
   
    //Eseguo la fork, il processo padre  continuerà ad essere il MasterThread,  mentre il processo figlio invocherà la funzione che gestisce il collector
    pid_t process_id=fork();

    if(process_id<0){
        //id <0 la fork ha dato errore
        fprintf(stderr, "MASTERTHREAD: Errore la fork ha ritornato un id negativo process_id=%d\n", process_id);
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
                fprintf(stderr,  "Errore accept() collector Socket\n");
                REMOVE_SOCKET();
                free(stop);
                return EXIT_FAILURE;
            }

            //creo la threadpool con la funzione createWorkerpool e controllo che la creazione della threadpool si concluda con successo
            workerpool_t * wpool=NULL;    
            if((wpool = createWorkerpool(n, q, collectorSocket))==NULL){
                fprintf(stderr, "MASTERTHREAD: Errore fatale nella creazione della workerpool\n");
                free(stop);
                REMOVE_SOCKET();
                return EXIT_FAILURE;
            }

            //setto gli argomenti da passare al thread signal handler
            sigHarg argSH ={stop, &mask, wpool};
           
            //creo il thread signal handler e gli passo gli argomenti appena settati
            pthread_t sigHandler;
            if(pthread_create(&sigHandler, NULL, &sigHandlerTask, (void *)&argSH) != 0){
                fprintf(stderr, "errore nella creazione del sighandler\n");
                destroyWorkerpool(wpool,false);
                REMOVE_SOCKET();
                free(stop);
                return EXIT_FAILURE;
            }

            int index=0; //indice per scorrere files
            pid_t collectorTerminato;

            //itero fino a che stop!=1, fino a che non ho mandato tutti i file o se il Collector è terminato per un qualunque motivo inaspettato
            while(!(*stop) && ((collectorTerminato=waitpid(process_id,NULL,WNOHANG))!=process_id)){
                //se non è stato passato alcun -t dorme 0
                //creo setto gli argomenti da mettere in coda
                wArg workerArg = {files[index], wpool};
                //printf("\nMASTERTHREAD:");puts(files[index]);puts("\n");
                if(t!=0){
                    //devo aspettare tot secondi prima di poter aggiungere una task
                    for(int time_delay=1; time_delay<=t; time_delay++){
                        //dormo un secondo alla volta e mano a mano controllo che non siano stati passati segnali di stop
                        sleep(1);
                        if(*stop==1){
                            destroyWorkerpool(wpool,false);
                            pthread_kill(sigHandler,SIGTERM);
                            pthread_join(sigHandler,NULL);
                            free(stop);
                            REMOVE_SOCKET();
                            return  EXIT_SUCCESS;
                        }
                        if(((collectorTerminato=waitpid(process_id,NULL,WNOHANG))==process_id)){
                            destroyWorkerpool(wpool,false);
                            pthread_kill(sigHandler,SIGTERM);
                            pthread_join(sigHandler,NULL);
                            free(stop);
                            REMOVE_SOCKET();
                            return  EXIT_FAILURE;
                        }

                    }
                }
                int check = addTask(wpool, (void*)&workerArg);
                
                if(check==0){
                    //incremento l'indice solo se riesco ad assegnare correttamente la task alla threadpool
                    ++index;
                    if(index==numFilePassati){
                        *stop=1;
                        continue;
                    }
                    
                    continue;
                }else{
                    if(check==1){
                        //ritorna 1 qundo la coda è piena
                        continue;
                    }else{
                        //check==-1, c'è stato un errore grave nella addTask notifico al collector che deve termianre sulla socket del signal handler ed esco 
                        pthread_kill(sigHandler,SIGTERM);
                        pthread_join(sigHandler,NULL);
                        destroyWorkerpool(wpool,false);
                        free(stop);                       
                        REMOVE_SOCKET();
                        return EXIT_FAILURE;
                    }
                }
            }
           
            //il collector è terminato prima del masterthread, c'è un errore, libero le risorse e ritorno EXIT_FAILURE
            if(collectorTerminato==process_id){
                destroyWorkerpool(wpool,false);
                *(stop)=1;
                pthread_kill(sigHandler,SIGTERM);
                pthread_join(sigHandler,NULL);
                free(stop);
                REMOVE_SOCKET();
                return  EXIT_FAILURE;
            }else{
                //se il while non è terminato perchè si è chiuso il collector deve essere terminato perchè  *stop==1
                //distruggo la threadpool ma aspetto che siano completate le task pendenti
                if(!destroyWorkerpool(wpool,true)){
                    //se la threadpool non si chiude correttamente
                    *(stop)=1;
                    pthread_kill(sigHandler, SIGTERM);
                    pthread_join(sigHandler,NULL);
                    free(stop);
                    REMOVE_SOCKET();
                    return EXIT_FAILURE;
                }else{
                    //chiudo la MasterSocket e aspetto che termini il collector
                    waitpid(process_id,NULL,0);
                    
                    pthread_kill(sigHandler, SIGTERM);
                    pthread_join(sigHandler,NULL);
                    free(stop);           
                    REMOVE_SOCKET();
                    return EXIT_SUCCESS;
                }
            }//fine dell'else che controlla la terminazione del ciclo while

        }//fine del processo padre
    }//fine del controllo correttezza della fork()
    //non ci dovrebbe arrivare mai
    return EXIT_FAILURE;
}