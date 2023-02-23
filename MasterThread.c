#define _POSIX_C_SOURCE 2001112L
/**
 * @file MasterThread.c
 * @author Michela Deodati 597983
 * @brief implementazione dell'intefraccia MasterThread
 * @version 0.1
 * @date 15-03-1013
 * 
 * 
 */
#define _POSIX_C_SOURCE 200112L
#define _OPEN_SYS_ITOA_EXT
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <signal.h>

#include <MasterThread.h>

static void* sigHandlerTask (void*arg){
    sigset_t * set = ((sighandler_t*)arg)->set;
    int pfd = ((sighandler_t*)arg)->signalPipe;

    while (true){
        int sig;
        int r;
        if(r=sigwait(set,&sig)!=0){
            errno=r;
            perror("Fatal Error sigwait.");
            exit(EXIT_FAILURE);
        }
        switch (sig){
            case SIGINT:
                fprintf(stdout, "Catturato segnale SIGINT\n");
            case SIGHUP:
                fprintf(stdout, "Catturato segnale SIGHUP\n");
            case SIGTERM:
                fprintf(stdout, "Catturato segnale SIGTERM\n");
            case SIGQUIT:
                fprintf(stdout, "Catturato segnale SIGQUIT\n");
                //chiudo la pipe per notificare al thread listener che ho ricevuto un segnale di terminazione
                close(pfd);
                retrun NULL;
            case SIGUSR1:
                //stabilisco una connessione con il collector per dirgli di stampare i risultati ottenuti fino ad adesso
                struct sockaddr_un a;
                strncpy(a.sun_path, SOCKET_NAME,UNIX_PATH_MAX);
                a.sun_family =AF_UNIX;
                int sigSocket=socket(AF_UNIX, SOCK_STREAM,0);
                if(sigSocket==-1){
                    perror("sigSocket()=socket()");
                    _exit(EXIT_FAILURE);
                }
                //è andato tutto bene
                while (connect(sigSocket,(struct sockaddr*)&a,sizeof(a))==-1){
                    if(errno=ENOENT){
                        sleep(1);
                    }else{
                        perror("sighandler fail connect()");
                        CLOSE_SOCKET(sigSocket);
                        _exit(EXIT_FAILURE);
                    }
                }
                //invio al collector il segnale di stampa
                write(sigSocket, "STAMPA", strlen("STAMPA")+1);
                //chiudo la connsessione e si reitera
                CLOSE_SOCKET(sigSocket);
                break;
        }
    }

}

