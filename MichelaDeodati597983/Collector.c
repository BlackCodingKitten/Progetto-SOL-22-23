#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 2001112L
#endif
/**
 * @file Collector.c
 * @author Michela Deodati 597983
 * @brief impplementazione dell'interfaccia Collector
 * @date 15-03-2023
 * 
 */

#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/un.h>
#include <assert.h>
#include <signal.h>


#include "./Util.h"
#include "./Collector.h"

/**
 * @brief funzione compare per il qsort per ordinare l'array prima della stampa
 * 
 * @param a 
 * @param b 
 * @return int 
 */
int compare (const void* a, const void *b){
    string A= *( string*)a;
    string B= *( string*)b;

    string e=NULL;
    long nA=strtol(A,&e,0);

    e=NULL;
    long nB=strtol(B,&e,0);

    return nA>nB;
}

/**
 * @brief stampa i risultati ottenuti dai file 
 * 
 * @param a array di filevalue e indirizzi 
 * @param dim dimensione dell'array
 */
void stampaRisultati (string * a, int dim){
    fflush(stdout);
    qsort(a, dim, sizeof(string), compare);
    for(int i=0; i<dim; i++){
        fprintf(stdout, "%s\n",a[i]);
        fflush(stdout);
    }
    
}

void runCollector (int numFile, int signal_pipe){
    string dataArray[numFile];
    //inizializzo l'array di file da stampare
    for(int i=0; i<numFile; i++){
        //alloco la stringa che contiene la path e il risultato del calcolo sul file
        dataArray[i]=(string)malloc(sizeof(char)*(FILE_BUFFER_SIZE+PATH_LEN));
    }
   
    //creo la listensocket;
    struct sockaddr_un addr;
    strncpy(addr.sun_path,SOCKET_NAME,UNIX_PATH_MAX);
    addr.sun_family = AF_UNIX;
    int listenSocket;
    if((listenSocket=socket(AF_UNIX,SOCK_STREAM,0))==-1){
        perror("listenSocket=socket(AF_UNIX,SOCK_STREAM,0)");
        for(int l=0; l<numFile; l++){
            free(dataArray[l]);
        }
        REMOVE_SOCKET();
        exit(EXIT_FAILURE);
    }
    //bind della listen socket
    if(bind(listenSocket,(struct sockaddr*)&addr, sizeof(addr))!=0){
        perror("bind(farm.sck)");
            CLOSE_SOCKET(listenSocket);
            for(int l=0; l<numFile; l++){
                free(dataArray[l]);
            }
            REMOVE_SOCKET();
            exit(EXIT_FAILURE);
    }
    //listen socket in ascolto
    if(listen(listenSocket, SOMAXCONN)==-1){
        perror("listen()");
            CLOSE_SOCKET(listenSocket);
            for(int l=0; l<numFile; l++){
                free(dataArray[l]);
            }
            REMOVE_SOCKET();
            exit(EXIT_FAILURE);
    }
    //indice per scorrere il dataArray
    int index=0;
    //utilizzo la select perchè così posso mettere nel master set la pipe passata come argomento
    fd_set set;
    fd_set tmp_set;
    FD_ZERO(&set);
    FD_ZERO(&tmp_set);
    FD_SET(listenSocket,&set);
    FD_SET(signal_pipe, &set);//aggiungo la pipe al master set per aver 

    //tengo traccia del file descriptor più grande
    int fdmax=(listenSocket>signal_pipe)?listenSocket:signal_pipe;
    while (true){
        
        tmp_set=set;
        if(select(fdmax+1,&tmp_set,NULL,NULL,NULL)==-1){
            perror("Collector Select");
            close(listenSocket);
            close(signal_pipe);
            for(int l=0; l<numFile; l++){
                free(dataArray[l]);
            }
            free(dataArray);
            REMOVE_SOCKET();
            exit(EXIT_FAILURE);
        }

        //adesso iteriamo per capire da quale file descriptor abbiamo ricevuto un messaggio
        for(int fd=0; fd<(fdmax+1);fd++){
            if(FD_ISSET(fd,&tmp_set)){
                int fd_conn=0;
                if(fd==listenSocket){

                    //nuova richiesta di connessione
                    fd_conn=accept(listenSocket,NULL,NULL);
                    if(fd_conn == -1){
                        perror("collector accept()");
                        close(listenSocket);
                        for(int l=0; l<numFile; l++){
                            free(dataArray[l]);
                        }
                        free(dataArray);
                        REMOVE_SOCKET();
                        exit(EXIT_FAILURE);
                    }                    
                    if(readn(fd_conn, dataArray[index],(FILE_BUFFER_SIZE+PATH_LEN))==-1){
                        fprintf(stderr, "COLLECTOR: ho ricevuto: %s", dataArray[index]);
                    }
                    index++;
                    close(fd_conn);
                    if(index==numFile){
                        //ho letto tutti i file posso stampare e uscire
                        stampaRisultati(dataArray,numFile);
                        CLOSE_SOCKET(listenSocket);
                        close(signal_pipe);
                        for(int l=0; l<numFile; l++){
                            free(dataArray[l]);
                        }
                        //chiudo il processo 
                        exit(EXIT_SUCCESS);                        
                    }
                }
                /**
                 * manca la parte dell'else in cui tramite la select mi connetto ad una socket che è già registrata nel set,
                 * questo perchè non ho necessità ne di registrare i client ne di rimuoverli in quanto ogni socket client viene 
                 * gestita interamente dai workers, dall'apertura alla chiusura, quindi in questo caso non ho necessità di registrare tramite la FD_SET(fd_conn,&set)
                 * 
                 */
                if(fd_conn==signal_pipe){
                    char a[2];
                    if(readn(signal_pipe,a,2)==-1){
                        fprintf(stderr, "COLLECTOR-Collector.c Errore nella lettura del segnale ricevuto dal sig handler");
                    }
                    if(strcmp(a,"t")==0){
                        close(signal_pipe);
                        CLOSE_SOCKET(listenSocket);
                        for(int l=0; l<numFile; l++){
                            free(dataArray[l]);
                        }
                        exit(EXIT_SUCCESS);
                    }
                    if(strcmp(a,"s")==0){
                        stampaRisultati(dataArray,index);
                    }
                }//end if gestione segnali
            }
        }
    }//end while select
    exit(EXIT_FAILURE); //qui non ci arriva mai
}






