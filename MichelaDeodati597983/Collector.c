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
    Data dataArray[numFile];
    for(int i=0; i<numFile; i++){
        dataArray[i].fileValue=0;
        //alloco la stringa che contiene la path
        dataArray[i].filePath=malloc(sizeof(char)*UNIX_PATH_MAX);
    }
    struct sockaddr_un addr;
    strncpy(addr.sun_path,SOCKET_NAME,UNIX_PATH_MAX);
    addr.sun_family = AF_UNIX;
    
    if((listenSocket=socket(AF_UNIX,SOCK_STREAM,0))==-1){
        perror("listenSocket=socket(AF_UNIX,SOCK_STREAM,0)");
        REMOVE_SOCKET();
        exit(EXIT_FAILURE);
    }

    if(bind(listenSocket,(struct sockaddr*)&addr, sizeof(addr))!=0){
        perror("bind(farm.sck)");
        goto _uscita_errore;
    }

    if(listen(listenSocket, SOMAXCONN)==-1){
        perror("listen()");
        goto _uscita_errore;
    }
    //inizializzo l'array di file da stampare
    
    //collector in ascolto
    for(int i=0; i<numFile; i++){
        int connfd;
        if((connfd=accept(listenSocket,NULL,NULL))== -1){
            perror("accept()");
            goto _uscita_errore;
        }
        string buffer =malloc(sizeof(char)*FILE_BUFFER_SIZE);
        if(readn(connfd,buffer,FILE_BUFFER_SIZE)==-1){
            perror("read(fileValue)");
            goto _uscita_errore;
        }
        if(strstr(buffer,"STAMPA")){
            //se ho ricevuto il segnale di stampa 
            i--; // nonsto leggendo un file quindi decremento il contatore
            stampaRisultati(dataArray,numFile);
            memset(buffer, '\0', FILE_BUFFER_SIZE);
            continue;
        }
        //la prima cosa che legge è il valore del calcolo quindi devo rifare la cnversione da stringa a long
        string e=NULL;
        long v = strtol(buffer,&e,0);
        if(e==(char)0 && e!=NULL){
            dataArray[i].fileValue=v;
        }
        //adesso leggo la path del file completa
        if(readn(connfd,dataArray[i].filePath,UNIX_PATH_MAX)==-1){
            perror("read(filepath)");
            goto _uscita_errore;
        }
    }
    //ho inserito tutti i file ed è andato tutto liscio quindi ordino l'array di elementi per stampare
    qsort(dataArray,numFile,sizeof(Data),compare);
    //stampo e chiudo la connessione
    for(int i=0; i<numFile; i++){
        fprintf(stdout,"%ld\t%s\n", dataArray[i].fileValue,dataArray[i].filePath);
    }
    CLOSE_SOCKET(listenSocket);
    REMOVE_SOCKET();
    fprintf(stdout, "Collector end.");

    _uscita_errore:
        CLOSE_SOCKET(listenSocket);
        REMOVE_SOCKET();
        exit(EXIT_FAILURE);
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

void stampaRisultati (Data * a, int dim){
    qsort(a,dim,sizeof(Data), compare);
    for(int i=0; i<dim; i++){
        if(a[i].fileValue==0){
            continue;
        }
        fprintf(stdout, "%ld\t%s\n", a[i].fileValue,a[i].filePath);
    }
    
}



