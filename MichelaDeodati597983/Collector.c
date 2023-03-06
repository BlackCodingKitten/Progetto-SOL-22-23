#define _POSIX_C_SOURCE 2001112L
/**
 * @file Collector.c
 * @author Michela Deodati 597983
 * @brief impplementazione dell'interfaccia Collector
 * @date 15-03-2023
 * 
 */
#include <Collector.h>
#include <Util.h>
#include<WorkerPool.h>
#include <MasterThread.h>
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
#include <sys/types.h>
#include <sys/un.h>
#include <assert.h>
#include <signal.h>

/**
 * @brief stampa i risultati ottenuti dai file 
 * 
 * @param a array di filevalue e indirizzi 
 * @param dim dimensione dell'array
 */
void stampaRisultati (Data * a, int dim){
    qsort(a,dim,sizeof(Data), compare);
    for(int i=0; i<dim; i++){
        if(a[i].fileValue==0){
            continue;
        }
        fprintf(stdout, "%ld %s\n", a[i].fileValue,a[i].filePath);
    }
    
}

void runCollector (int numFile, int signal_pipe){
    int currentDataNumber=0;
    Data dataArray[numFile];
    //inizializzo l'array di file da stampare
    for(int i=0; i<numFile; i++){
        //inizializzo il valore di tutti i filevalue
        dataArray[i].fileValue=0;
        //alloco la stringa che contiene la path
        dataArray[i].filePath=malloc(sizeof(char)*PATH_LEN);
    }
    //creo la listensocket;
    struct sockaddr_un addr;
    strncpy(addr.sun_path,SOCKET_NAME,UNIX_PATH_MAX);
    addr.sun_family = AF_UNIX;
    int listenSocket;
    
    if((listenSocket=socket(AF_UNIX,SOCK_STREAM,0))==-1){
        perror("listenSocket=socket(AF_UNIX,SOCK_STREAM,0)");
        REMOVE_SOCKET();
        exit(EXIT_FAILURE);
    }
    //bind della listen socket
    if(bind(listenSocket,(struct sockaddr*)&addr, sizeof(addr))!=0){
        perror("bind(farm.sck)");
            CLOSE_SOCKET(listenSocket);
            REMOVE_SOCKET();
            exit(EXIT_FAILURE);
    }
    //listen socket in ascolto
    if(listen(listenSocket, SOMAXCONN)==-1){
        perror("listen()");
            CLOSE_SOCKET(listenSocket);
            REMOVE_SOCKET();
            exit(EXIT_FAILURE);
    }

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
        if(select(fdmax+1, &tmp_set,NULL,NULL,NULL)==-1){
            perror("Collector Select");
            close(listenSocket);
            close(signal_pipe);
            REMOVE_SOCKET();
            exit(EXIT_FAILURE);
        }

        //adesso iteriamo per capire da quale file descriptor abbiamo ricevuto un messaggio
        for(int fd=0; fd<(fdmax+1);fd++){
            if(FD_ISSET(fd,&tmp_set)){
                int fd_conn;
                if(fd==listenSocket){
                    //nuova richiesta di connessione
                    fd_conn=accept(listenSocket,NULL,NULL);
                    if(fd_conn == -1){
                        perror("collector accept()");
                        close(listenSocket);
                        REMOVE_SOCKET();
                        exit(EXIT_FAILURE);
                    }
                    string buffer=malloc(sizeof(char)*FILE_BUFFER_SIZE);
                    readn(fd_conn, buffer,FILE_BUFFER_SIZE);
                    dataArray[index].fileValue=StringToNumber(buffer);
                    readn(fd_conn,dataArray[index].filePath,PATH_LEN);
                    index++;
                    free(buffer);
                    close(fd_conn);
                    if(index==numFile){
                        stampaRisultati(dataArray,numFile);
                        CLOSE_SOCKET(listenSocket);
                        close(signal_pipe);
                        //chiudo il processo così si blocca 
                        //la wait del masterthread che è il processo padre del collector
                        exit(EXIT_SUCCESS);                        
                    }
                }
                if(fd_conn==signal_pipe){
                    char a[2];
                    readn(signal_pipe,a,2);
                    if(strcmp(a,"t")==0){
                        close(signal_pipe);
                        CLOSE_SOCKET(listenSocket);
                        exit(EXIT_SUCCESS);
                    }
                    if(strcmp(a,"s")==0){
                        int currentDim=index+1;
                        stampaRisultati(dataArray,currentDim);
                    }
                }//end if gestione segnali
            }
        }
    }//end while select
    exit(EXIT_FAILURE); //qui nin ci arriva mai
}

/**
 * @brief funzione compare per il qsort per ordinare l'array prima della stampa
 * 
 * @param a 
 * @param b 
 * @return int 
 */
int compare (const void* a, const void *b){
    Data A = *(Data*)a;
    Data B =*(Data*)b;
    if(A.fileValue-B.fileValue!=0){
        return A.fileValue-B.fileValue;
    }
    //se a e B hanno lo stesso valore i file vengono ordinati in ordine alfabetico
    return strcmp(A.filePath,B.filePath);
}





