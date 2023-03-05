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

void runCollector (int numFile){
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
    
    //il collector si mette in ascolto dei client che gestisce con una select in modo che quando 
    //arriva un segnale di terminazione o il segnale di 
    //stampa lui sa quello che deve fare leggeno il file descriptor della pipe che gli viene passata come argomento dal masthreThread
    

    CLOSE_SOCKET(listenSocket);
    REMOVE_SOCKET();
    fprintf(stdout, "Collector end.");
    exit(EXIT_SUCCESS); //chiudo il processo così si blocca la wait del masterthread che è il processo padre del collector
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
        fprintf(stdout, "%ld\t%s\n", a[i].fileValue,a[i].filePath);
    }
    
}



