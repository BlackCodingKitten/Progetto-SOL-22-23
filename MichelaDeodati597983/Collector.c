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
        fprintf(stdout, "COLLECTOR:%s\n",a[i]);
        //senza il flush non passa i test
        fflush(stdout);
    }
    
}

void runCollector (int numFile){


    string dataArray[numFile];
    //inizializzo l'array di file da stampare
    for(int i=0; i<numFile; i++){
        //alloco la stringa che contiene la path e il risultato del calcolo sul file
        dataArray[i]=(string)malloc(sizeof(char)*PATH_LEN);
        memset(dataArray[i],'\0',PATH_LEN);
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
            _exit(EXIT_FAILURE);
        }
    }
   
    int index=0;//tiene traccia di quanti file sono stati scritti nel dataArray
    int check =0;
    string buffer =(string)malloc(sizeof(char)*PATH_LEN);
    
    while(true){
        memset(buffer, '\0', PATH_LEN); 
        if((check=read(sck, buffer, PATH_LEN))==0){
            //la pipe è chiusa
            fprintf(stdout, "pipe chiusa");
            free(buffer);
            _exit(EXIT_SUCCESS);
        }
        fprintf(stdout, "Ho letto %d byte :%s\n", check,buffer);
        if(strlen(buffer)==1){
            if(strcmp(buffer,  "s")==0){
                stampaRisultati(dataArray,index);
            }else{
                if(strcmp(buffer,"t")==0){
                    stampaRisultati(dataArray,index);
                    for(int i=0; i<index; i++){
                        free(dataArray[index]);
                    }
                    free(buffer);
                    _exit(EXIT_SUCCESS);
                }else{
                    fprintf(stderr, "Letto un valore inaspettato ERRORE\n");
                    free(buffer);
                    _exit(EXIT_FAILURE);
                }
            }
        }else{
            printf("COLLECTOR:Copio nel data array %s\n", buffer);
            strcpy(dataArray[index],buffer);
            index++;
            if(writen(sck, "ok", 3) !=0){
                fprintf(stderr,  "il collector non riesce a comunicare l'avvenuta ricezine del messaggio alla threadpool\n");
                free(buffer);
                _exit(EXIT_FAILURE);
            }
            if(index>=numFile){
                stampaRisultati(dataArray, index);
                free(buffer);
                _exit(EXIT_SUCCESS);
            }
        }

        

    }
    //non ci arriva mai
    free(buffer);
    _exit(EXIT_FAILURE);
   
}//end Collector






