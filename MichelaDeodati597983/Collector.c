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
 * @param a elemnto di dataarray
 * @param b elemento di dataarray
 * @return int contronto tra a e b
 */
int compare (const void* a, const void *b){
    string A= *( string*)a;
    string B= *( string*)b;

    string e=NULL;
    long nA=strtol(A,&e,0);

    string e2=NULL;
    long nB=strtol(B,&e2,0);

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
    //ordino gli elementi dell'array prima di stampare
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
    int check =0;//avlore di ritorno della read
    string buffer = (string)malloc(sizeof(char)*PATH_LEN); //buffer di scritturea e lettura

    while(true){
        //controllo che index sia minore di numfile, altrimenti ho finito di lavorare, stampo ed esco
        if(index == numFile || index>numFile){
            stampaRisultati(dataArray, numFile);
            for (int i=0; i<numFile; i++){
                if(dataArray[i]!=NULL){
                    free(dataArray[i]);
                }
            }
            if(buffer!=NULL){
                free(buffer);
            }
            close(sck);
            _exit(EXIT_SUCCESS);
        }
        //resetto il buffer
        memset(buffer, '\0', PATH_LEN); 
        //controllo il vaore di ritorno della read per assicurarmi che la pipe non sia stata chiusa e che non ci siano errori
        if((check=read(sck, buffer, PATH_LEN))==0){
            //la pipe è chiusa
            fprintf(stdout, "pipe chiusa");
            for (int i=0; i<numFile; i++){
                if(dataArray[i]!=NULL){
                    free(dataArray[i]);
                }
            }
            if(buffer!=NULL){
                free(buffer);
            }
            close(sck);
            _exit(EXIT_SUCCESS);
        }
        //read ritona un valore negativo errore 
        if(check<0){
            _exit(EXIT_FAILURE);
        }
        //controllo il valore di int * signal_flag 
        if(strlen(buffer)==STAMPA){
            //segnale di stampa
            stampaRisultati(dataArray,index);
            continue;
        }
        if(strlen(buffer)==TERMINA){
            //segnale di terminazione
            stampaRisultati(dataArray,index);
            for (int i=0; i<numFile; i++){
                if(dataArray[i]!=NULL){
                    free(dataArray[i]);
                }
            }
            if(buffer!=NULL){
                free(buffer);
            }
            close(sck);
            _exit(EXIT_SUCCESS);
        }
        //se la lunghezza del buffer è maggiore di 0 caratteri significa che ho ricevuto correttamente la stringa
        if(strlen(buffer)>0){
            strcpy(dataArray[index],buffer);
            ++index;
            //rispondo al worker che ha inviato il file per comunicargli la corretta lettura, se fallisce esco
            if(writen(sck, "ok", 3) !=0){
                fprintf(stderr,  "il collector non riesce a comunicare l'avvenuta ricezine del messaggio alla threadpool\n");
                for (int i=0; i<numFile; i++){
                    if(dataArray[i]!=NULL){
                        free(dataArray[i]);
                    }
                }
                if(buffer!=NULL){
                    free(buffer);
                }
                close(sck);
                _exit(EXIT_FAILURE);
            }
        } 
    }
    fprintf(stderr, "Unkwnon Error\n");
    //non ci arriva mai
    for (int i=0; i<numFile; i++){
        if(dataArray[i]!=NULL){
            free(dataArray[i]);
        }
    }
    if(buffer!=NULL){
        free(buffer);
    }
    close(sck);
    _exit(EXIT_FAILURE);
   
}//end Collector






