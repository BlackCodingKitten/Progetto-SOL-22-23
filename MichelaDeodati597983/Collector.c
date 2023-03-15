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
        //senza il flush non passa i test
        fflush(stdout);
    }
    
}

void runCollector (int numFile){


    string dataArray[numFile];
    //inizializzo l'array di file da stampare
    for(int i=0; i<numFile; i++){
        //alloco la stringa che contiene la path e il risultato del calcolo sul file
        dataArray[i]=(string)malloc(sizeof(char)*(FILE_BUFFER_SIZE+PATH_LEN));
        memset(dataArray[i],'\0',(FILE_BUFFER_SIZE+PATH_LEN));
    }
   
    //creo socket
    struct sockaddr_un addr;
    strncpy(addr.sun_path,SOCKET_NAME,UNIX_PATH_MAX);
    addr.sun_family = AF_UNIX;
    int sck=0;
    if((sck=socket(AF_UNIX,SOCK_STREAM,0))==-1){
        perror("sck=socket(AF_UNIX,SOCK_STREAM,0)");
        for(int l=0; l<numFile; l++){
            free(dataArray[l]);
        }
        REMOVE_SOCKET();
        _exit(EXIT_FAILURE);
    }

    while(connect(sck,(struct sockaddr*)&addr, sizeof(addr))==-1){
        if(errno==ENOENT){
            sleep(1);
        }else{
            fprintf(stderr, "Fallito tentativo di connessione con MasterThread\n");
            for(int i=0;i<numFile;i++){
                free(dataArray[i]);
            }
            close(sck);
            _exit(EXIT_FAILURE);
        }
    }
   
    int index=0;//tiene traccia di quanti file sono stati scritti nel dataArray
    int check =0;
    bool readSignal=false;
    while(true){
       
        string buffer = (string)malloc(sizeof(char)*(FILE_BUFFER_SIZE+PATH_LEN));
        memset(buffer, '\0', (FILE_BUFFER_SIZE+PATH_LEN));

        while((check=read(sck,buffer,(FILE_BUFFER_SIZE+PATH_LEN)))==-1){
            if(errno=EINTR){
                free(buffer);
                string buffer = (string)malloc(sizeof(char)*(FILE_BUFFER_SIZE+PATH_LEN));
                memset(buffer, '\0', (FILE_BUFFER_SIZE+PATH_LEN));
            }else{
                //errore insapettato nella read
                fprintf(stderr, "Errore inaspettato nella lettura\n");
                free(buffer);
                for(int i=0; i<numFile; i++){
                    free(dataArray[i]);
                }
                close(sck);
                _exit(EXIT_FAILURE);

            }
        }
        
        if(check==0 && strlen(buffer)==0){
            if(readSignal){
                //è stato letto un segnale di terminazione e ora è stata chiusa la socket del Masterthread
                close(sck);
                for(int i=0; i<numFile; i++){
                    if(i<index){
                        puts(dataArray[i]);

                    }
                    //free(dataArray[i]);
                }
                _exit(EXIT_SUCCESS);
            }else{
                //è stata chiusa la Socket del masterthread significa che c'è stato un errore
                free(buffer);
                for(int i=0; i<numFile; i++){
                    free(dataArray[i]);
                }
                close(sck);
                _exit(EXIT_FAILURE);
            }
        }
        if(strlen(buffer)>0){
            //la lettura è avvenuta correttamente
            if(strncmp(buffer, "stampa", strlen("stampa"))==0){
                free(buffer);
                if(writen(sck,"k",2)==-1){
                    fprintf(stderr, "Impossibile raggiungere l'handler");
                }
                //è stato inviato sigusr1
                string tmp[index];
                if(index>0){
                    for(int i=0; i<index;i++){
                        tmp[i]=malloc(sizeof(char)*(1+strlen(dataArray[i])));
                        strcpy(tmp[i],dataArray[i]);
                    }
                    stampaRisultati(tmp,index);
                }
                for(int i=0; i<index; i++){
                    free(tmp[i]);
                }
                continue;
            }
            if(strncmp(buffer, "exit", strlen("exit"))==0){
                free(buffer);
                //è stato letto un segnale di terminazione
                readSignal=true;
                if(writen(sck,"k",2)==-1){
                    fprintf(stderr, "Impossibile raggiungere l'handler");
                }
                continue;
            }
            //inserisco il risultato della lettura nel dataArray e rispondo 
            strcpy(dataArray[index], buffer);
            free(buffer);
            if(writen(sck,"OK",3)!=0){
                fprintf(stderr, "impossibile comunicare al worker che ho ricevuto il segnale\n");
                free(buffer);
                for(int i=0; i<numFile; i++){
                    free(dataArray[i]);
                }
                close(sck);
                _exit(EXIT_FAILURE);
            }
            index++;
            if(index==numFile){
                puts("stampa");
                stampaRisultati(dataArray,numFile);
                for(int i=0; i<numFile; i++){
                    free(dataArray[i]);
                }
                close(sck);
                _exit(EXIT_SUCCESS);
            }else{
                continue;
            }
        }
    }
    //non ci arriva mai
    _exit(EXIT_FAILURE);
   
}//end Collector






