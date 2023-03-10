
/**
 * @file MasterThread.c
 * @author Michela Deodati 597983
 * @brief implementazione dell'intefraccia MasterThread
 * @version 0.1
 * @date 15-03-2023
 * 
 * 
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 2001112L
#endif
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
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
 * @brief @struct che rappresenta gli argomenti passare al thread signal handler.
 * 
 */
typedef struct s{
    int * stop;         //condizione di terminazione del while del masterthread;
    sigset_t set;       // set dei segnali da gestire mascherati
    int signal_pipe;    //descrittore di scrittura di una pipe senza nome usato per comunicare al collector che è terminata l'esecuzione
}sigHarg;

/**
 * @brief task del thread signal handler che gestisce i segnali di terminazione e i segnali di stampa 
 * invia un messaggio sulla pipe passata come argomento per comunicare al collector la terminazione o 
 * che deve stampare
 * 
 * @param arg 
 * @return void* 
 */
static void* sigHandlerTask (void*arg){
    sigHarg sArg = *(sigHarg*)arg;
    printf("DEBUG Masterthread-signal_handler:AVVIO DEL THREAD\n"); fflush(stdout);
    while (true){
        int sig;
        if(sigwait(&(sArg.set),&sig)==-1){
            errno=EINVAL;
            perror("SIGNAL_HANDLER-Masterthread.c-123: errore sigwait");
            exit(EXIT_FAILURE);
        }
        switch (sig){
            case SIGINT:
            case SIGHUP:
            case SIGTERM:
            if(writen(sArg.signal_pipe,"t", 2)==-1){
                fprintf(stderr, "SIGNAL_HANDLER-Masterthread.c-131: errore writen sulla pipe");
            }
            close(sArg.signal_pipe); //notifico la ricezione del segnale al Collector;
            //cambio il vlore della guardia del while in masterthread
            *(sArg.stop)=1;
            return NULL;          
            case SIGUSR1:
                //invio al collector il segnale di stampa
                if(writen(sArg.signal_pipe, "s", 2)==-1){
                    fprintf(stderr, "SIGNAL_HANDLER-Masterthread.c-137: errore writen sulla pipe");
                }
        }
    }
    return NULL;

}

/**
 * @brief core del masterthread che esegue tutte le funzioni richieste: controllo dell'input, creazione della workerpool
 * gestione dei segnali, fork per creare il processo figlio collector, terminazione della threadpool
 */
void runMasterThread(int n, int q, int t, int numFilePassati, string * files){
    //creo la condizione del while per i segnali di stop che poi verrà passato agli argomenti del signal hanler
    int stop=0;
    //creo la threadpool con la funzione createWorkerpool
    workerpool_t *wpool=NULL;    
    //controllo che la creazione della threadpool si concluda con successo
    if((wpool =createWorkerpool(n, q))==NULL){
        fprintf(stderr, "MASTERTHREAD: Errore fatale nella creazione della workerpool\n");
        exit(EXIT_FAILURE);
    }
    printf("DEBUG Masterthread: Pool allocata Correttamente\n"); fflush(stdout);
    printf("DEBUG Masterthread: creo la pipe per comunicare con il collector tramite il signal handler\n");
    //creo la pipe che mi permette di segnalare al collector che sto terminando o per dirgli di stamapare
    int s_pipe[2];
    if(pipe(s_pipe)==-1){
        perror("Masterthread: errore esecuzione pipe()");
        exit(EXIT_FAILURE);
    }
    //faccio partire il processo collector facendo la fork perchè adesso conosco quanti file ho il totale da calcolare
    pid_t process_id=fork();
    if(process_id==0){
        printf("DEBUG MasterThread: Eseguita la fork e sono nel processo Collector\n");
        //avvio il processo collector 
        runCollector(numFilePassati,s_pipe[0]);
    }else{
        if(process_id<0){
            fprintf(stderr, "MASTERTHREAD: Errore la fork ha ritornato un id negativo process_id=%d\n", process_id);
            REMOVE_SOCKET();
            exit(EXIT_FAILURE);
        }else{
            printf("DEBUG Masterthread: Dopo la fork sono nel processo Masterthread\n");
            // gestisco il signal handler aggiungendo i segnali da gestire al set
            sigset_t mask;
            sigemptyset(&mask);
            sigaddset(&mask, SIGINT);
            sigaddset(&mask, SIGHUP);
            sigaddset(&mask, SIGTERM);
            sigaddset(&mask, SIGUSR1);
            printf("DEBUG Masterthread: ho eseguito tutti i sigaddset\n");
            if (pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0)
            {   fprintf(stderr, "fatal error pthread_sigmask\n");
                REMOVE_SOCKET();
                exit(EXIT_FAILURE);
            }
            pthread_t sigHandler; // creo il pthread_t del signal_handler 
            sigHarg argument = {&stop, mask, s_pipe[1]};
            if (pthread_create(&sigHandler, NULL, &sigHandlerTask, (void *)&argument)!=0){
                fprintf(stderr, "errore nella creazione del sighandler\n");
                REMOVE_SOCKET()
                exit(EXIT_FAILURE);
            }
            printf("DEBUG Masterthread: creato e avviato signal handler\n");fflush(stdout);
            //ignoro il segnale sigpipe per evitare di essere terminato da una scrittura su socket
            struct sigaction s;
            memset(&s,0,sizeof(s));
            s.sa_handler=SIG_IGN;
            if((sigaction(SIGPIPE,&s,NULL))==-1){
                perror("sigaction()");
                REMOVE_SOCKET();
                exit(EXIT_FAILURE);
            }
            printf("DEBUG Masterthread: inizio ad aggiungere task\n");fflush(stdout);
            int index=0;
            //itero fino a che non termina con segnale o fino a che non ho mandato tutti i file 
            while(!stop && index<numFilePassati){
                printf("DEBUG Masterthread: aggiungo la task %d\n", index);fflush(stdout);
                int check = addTask(wpool, (void*)&files[index]);

                if(check==0){
                    //incremento l'indice solo se riesco ad assegnare correttamente la task alla threadpool
                    ++index;
                    printf("DEBUG Masterthread: Task aggiunta con successo\n");fflush(stdout);
                    sleep(t);//se non è stato passato alcun t dorme 0
                    printf("DEBUG Masterthread: aggiungo la nuova task\n");fflush(stdout);
                    continue;
                }else{
                    if(check==1){
                        printf("DEBUG Masterthread: coda piena\n");fflush(stdout);
                        //ritorna 1 qundo la coda è piena
                        continue;
                    }else{
                        fprintf(stderr, "Masterthread: fatal error in threadpool uscita in corso");
                        break;
                    }
                }
            }
            //distruggo la threadpool ma aspetto che siano completate le task pendenti
            printf("DEBUG Masterthread: distruggo la threadpool\n");fflush(stdout);
            destroyWorkerpool(wpool);
            //aspetto che termini il collector
            printf("DEBUG Masterthread: Attendo il collector\n");fflush(stdout);
            waitpid(process_id,NULL,0);
            printf("DEBUG Masterthread: Rimuovo la socket\n");fflush(stdout);
            REMOVE_SOCKET()
        }
    }//END Else della fork
    
}
