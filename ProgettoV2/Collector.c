
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

void runCollector(int* stop){
    string dataArray[100];
    for(int i=0; i<100; i++){
        dataArray[i]=NULL;
    }
    //creo socket
    struct sockaddr_un addr;
    strncpy(addr.sun_path,SOCKET_NAME,UNIX_PATH_MAX);
    addr.sun_family = AF_UNIX;
    int sck=0;
    if((sck=socket(AF_UNIX,SOCK_STREAM,0))==-1){
        perror("sck=socket(AF_UNIX,SOCK_STREAM,0)");
        _exit(EXIT_FAILURE);
    }
    while(connect(sck,(struct sockaddr*)&addr, sizeof(addr))==-1){
        if(errno==ENOENT){
            sleep(1);
        }else{
            fprintf(stderr, "Fallito tentativo di connessione con MasterThread\n");
            close(sck);
            _exit(EXIT_FAILURE);
        }
    }
    //DEBUG
    puts("collector connesso correttamente");
    int index=0;
    int check=0; 
    string buffer = (string)malloc(sizeof(char)*BUFFER_SIZE);
    while(*stop == 0){
        memset(buffer, '\0', BUFFER_SIZE);
        if((check =readn(sck,buffer,BUFFER_SIZE))<0){
              fprintf(stderr, "Errore inaspettato nella lettura\n");
              free(buffer);
              close(sck);
              _exit(EXIT_FAILURE);
        }

        if(strlen(buffer)==1){
            if(strcmp(buffer, "t")==0){
                //è stato letto un segnale di terminazione
                stampaRisultati(dataArray, index);
                free(buffer);
                for(int i=0; i<index; i++){
                    free(dataArray[i]);
                }
                free(buffer);
                _exit(EXIT_SUCCESS);
            }
            if(strcmp(buffer, "s")==0){//è stato letto un segnale di stampa
                stampaRisultati(dataArray, index);
                continue;
            }
        }

        if(strlen(buffer)==4){
            //ho ricevuto stop, sono finiti i file da mandare
           stampaRisultati(dataArray, index);
            free(buffer);
            for(int i=0; i<index; i++){
                free(dataArray[i]);
            }
            free(buffer);
            _exit(EXIT_SUCCESS);
        }

        if(strlen(buffer)>1){
            //un thread mi ha inviato un file 
            dataArray[index]=malloc(sizeof(char)*(1+strlen(buffer)));
            strcpy(dataArray[index], buffer);
            ++index;
            continue;
        }

        if(strlen(buffer)==0 && check==0){
            //la socket è stata chiusa 
            for(int i=0; i<index; i++){
                free(dataArray[i]);
            }
            free(buffer);
            _exit(EXIT_SUCCESS);
        }
    }
    _exit(EXIT_SUCCESS);

}