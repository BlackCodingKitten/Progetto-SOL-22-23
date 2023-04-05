/**
 * @file Masterthread.c
 * @author MICHELA DEODATI 597983
 * @brief 
 * @version 2.0
 * @date 2023-03-22
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#define _POSIX_C_SOURCE 2001112L
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <dirent.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include<sys/wait.h>


#include "./Util.h"
#include "./Masterthread.h"
#include "./Collector.h"
#include "./Workerpool.h"

typedef struct signal_handler_arg{
    int *stop;
    sigset_t * set;
    workerpool_t * wpool;
    pid_t childpid;
}signal_handler_arg;


/**
 * @brief task del pthread signal_hanlder che gestisce i segnali SIGUSR1,SIGTERM,SIGHUP,SIGINT
 * 
 * @param arg socket su cui comunicare col collector,puntatore Stop, workerpool,id processo child 
 * @return void* NULL 
 */
static void* signal_handler_task (void* arg){
    //per prima cosa casto arg:
    signal_handler_arg ARG = *((signal_handler_arg*)arg);
    int fd_conn = ARG.wpool->fd_socket;
    sigset_t* mask = ARG.set;

    int recived_signal=0;
    while(1){
        //attendo che arrivi un segnale 
        if(sigwait(mask,&recived_signal)==-1){
            errno=EINVAL;
            perror("signal_handler: sigwait()");
            close(fd_conn);
            exit(EXIT_FAILURE);
        }else{
            //ho ricevuto correttamente un segnale
            switch(recived_signal){
                case SIGTERM:
                case SIGHUP:
                case SIGINT:
                    if(*(ARG.stop)==0){
                        if(pthread_mutex_lock(&(ARG.wpool->conn_mutex))!=0){
                            fprintf(stderr, "Impossibile acquisire la lockdella socket\n");
                            close(fd_conn);
                            exit(EXIT_FAILURE);
                        }else{
                            while(ARG.wpool->can_write==0){
                                pthread_cond_wait(&(ARG.wpool->conn_cond), &(ARG.wpool->conn_mutex));
                            }
                            ARG.wpool->can_write=0;
                            if(writen(fd_conn, "t", 2)<0){
                                fprintf(stderr, "Il signal_handler non riesce a comunicare con il collector\n");
                                close(fd_conn);
                                exit(EXIT_FAILURE);
                            }
                            ARG.wpool->can_write=1;
                            pthread_cond_signal(&(ARG.wpool->conn_cond));
                            if(pthread_mutex_unlock(&(ARG.wpool->conn_mutex))!=0){
                                fprintf(stderr, "Impossibile esguire l'unlock della socket, errore fatale\n");
                                close(fd_conn);
                                exit(EXIT_FAILURE);
                            }
                        }
                        ARG.wpool->exiting=true;
                        destroyWorkerPool(ARG.wpool,true);
                        waitpid(ARG.childpid,NULL,0);
                        (*(ARG.stop)=1);
                    }
                    //se stop != 0 significa che sono state mandate tutte le task in coda e si aspetta a terminazione della pool
                    return NULL;
                    break;
                case SIGUSR1:
                    if(pthread_mutex_lock(&(ARG.wpool->conn_mutex))!=0){
                        fprintf(stderr, "Impossibile acquisire la lockdella socket\n");
                        close(fd_conn);
                        exit(EXIT_FAILURE);
                    }else{
                        while(ARG.wpool->can_write==0){
                            pthread_cond_wait(&(ARG.wpool->conn_cond), &(ARG.wpool->conn_mutex));
                        }
                        ARG.wpool->can_write=0;
                        if(writen(fd_conn, "s", 2)<0){
                            fprintf(stderr, "Il signal_handler non riesce a comunicare con il collector\n");
                            close(fd_conn);
                            exit(EXIT_FAILURE);
                        }
                        ARG.wpool->can_write=1;
                        pthread_cond_signal(&(ARG.wpool->conn_cond));
                        if(pthread_mutex_unlock(&(ARG.wpool->conn_mutex))!=0){
                            fprintf(stderr, "Impossibile esguire l'unlock della socket, errore fatale\n");
                            close(fd_conn);
                            exit(EXIT_FAILURE);
                        }
                    }
                    break;
                default: 
                    fprintf(stderr, "ERRORE segnale non valido catturato del signal handler\n");
                    close(fd_conn);
                    exit(EXIT_FAILURE);
            }//fine switch
        }
    }
    return NULL;
}

/**
 * @brief salva nell'array di stringhe tutti i file che trova
 *  esplorando la cartella, l'esplorazione consiste in una BFS sull'albero delle sottocartelle della cartella passata
 * 
 * @param dirName nome della cartella da esplorare
 * @param saveFile string array
 * @param index posizione dello string array in cui scrivere
 */
void findFileDir(const string dirName, string* saveFile, int index)
{
    //alloco l'array di stringhe che conterrà poi tutti i nodi dell'abero delle directory
    string * dirTree = malloc(sizeof(string) * 100);
    int treeI = 0; //indice dell'array di stringhe
    struct stat buf;
    if (stat(dirName, &buf) == -1){
        perror("MAIN: facendo la stat() nella funzione findFileDir");
        fprintf(stderr, "Errore nella directory %s\n", dirName);
        exit(EXIT_FAILURE);
    }
    DIR *d;
    if ((d = opendir(dirName)) == NULL)
    {   perror("MAIN: facendo la opendir() nella funzione findFileDir");
        fprintf(stderr, "errore nella directory %s\n", dirName);
        exit(EXIT_FAILURE);
    }
    else
    {
        struct dirent *f;
        while ((errno = 0, f = readdir(d)) != NULL)
        {
            struct stat buf;
            string filename = malloc(sizeof(string) * BUFFER_SIZE);
            memset(filename, '\0', BUFFER_SIZE);
            strcpy(filename, dirName);
            strcat(filename, "/");
            strcat(filename, f->d_name);
            if (stat(filename, &buf) == -1){
                perror("MAIN: facendo la stat() nella funzione findFileDir");
                fprintf(stderr, "errore in %s", filename);
                exit(EXIT_FAILURE);
            }
            if (S_ISDIR(buf.st_mode))
            {
                if (strlen(filename) > 0 && (filename[strlen(filename) - 1] != '.'))
                {   
                    //se è una cartella e non è '.' o '..'
                    dirTree[treeI] = malloc(sizeof(char) * BUFFER_SIZE);
                    strcpy(dirTree[treeI], filename);
                    ++treeI;
                }
            }
            else
            {
                if (isFile(filename))
                {
                    saveFile[index] = malloc(sizeof(char) * BUFFER_SIZE);
                    memset(saveFile[index], '\0', BUFFER_SIZE);
                    //salvo il file nell'array solo se è un file regolare e del tipo che i miei thread possono leggere
                    if(isFile(filename)){
                        strncpy(saveFile[index], filename, BUFFER_SIZE);
                        ++index;
                    }else{
                        free(saveFile[index]);
                    }

                }
            }
            free(filename);
        }
        if (errno != 0)
        {
            perror("facendo la readdir() nella funzione findFileDir");
            fprintf(stderr, "readdir della cartella %s fallito\n", dirName);
            closedir(d);
            exit(EXIT_FAILURE);
        }
        for (int k = 0; k < treeI; k++)
        {
            //eseguo la funzione anche sulle altre sottocartelle trovate passando però l'indice corretto a cui scrivere
            findFileDir(dirTree[k], saveFile, index);
            free(dirTree[k]);
        }
        free(dirTree);
        closedir(d);
    }
}

/**
 * @brief controlla che il file che gli viene passato sia un regular file 
 * e presenti le caratteristiche dei file che possono lggere i miei thread
 * 
 * @param filePath path del file
 * @return int 1 se è un file conforme 0 altrimenti
 */
int isFile(const string filePath){
    struct stat path_stat;
    string tmp = malloc(sizeof(char)*strlen(filePath)+3);
    memset(tmp, '\0', strlen(filePath)+3);
    strcpy(tmp, "./");
    strcat(tmp, filePath);
    if(stat(tmp,&path_stat)!=0){
        perror("stat");
        return 0;
    }
    //controllo che sia effettivamente un file
    if(S_ISREG(path_stat.st_mode)){
        free(tmp);
        return 1;
    }
    free(tmp);
    return 0;
}













/****RUN******MASTERTHREAD***/

int runMasterthread(int nthread, int qsize, int tdelay, int fileindex, string * argv, int argc, string dir_name,  sigset_t * mask, int fd_conn){
    //la fd_conn creata nel main si mette in ascolto 
    if(listen(fd_conn,SOMAXCONN)==-1){
        perror("listen()");
        close(fd_conn);
        REMOVE_SOCKET();
        return EXIT_FAILURE;
    }

    //creo la condizione di loop del masterthread
    int*stop=(int*)malloc(sizeof(int));
    *stop=0;

    //creo il processo filgio che farà partire il collector
    pid_t process_id=0;
    process_id=fork();
    if(process_id<0){
        //la fork ha ritornato un valore negativo quindi non è andta a buon fine
        fprintf(stderr, "Mastrthread.c Errore nell'esecuzione della fork, return valore negativo\n");
        free(stop);
        close(fd_conn);
        REMOVE_SOCKET();
        return EXIT_FAILURE;
    }
    //se la fork è andata a buon fine 
    if(process_id==0){
        //sono nel processo figlio quindi avvio il Collector 
    }else{
        //sono nel processo padre 
        //il collector è stato avviato accetto la connessione 
        int collector_socket= accept(fd_conn, NULL,0);
        if(collector_socket==-1){
            close(fd_conn);
            REMOVE_SOCKET();
            return EXIT_FAILURE;
        }
        //creo la threadpool passandogli la collector_socket per permettere ai thread di comunicare
        workerpool_t* wpool=NULL;
        wpool=createWorkerPool(nthread, qsize, collector_socket);
        if(wpool==NULL){
            //creazione della threadool fallita
            fprintf(stderr, "Errore nella creazione della threadpool\n");
            close(fd_conn);
            REMOVE_SOCKET();
            free(stop);
            return EXIT_FAILURE;
        }
        //creo il signal handler 
        pthread_t signal_handler;
        //argomenti da passare al sig_handler
        signal_handler_arg SHA = {stop, mask, wpool, process_id};
        if(pthread_create(&signal_handler,NULL,&signal_handler_task, (void*)&SHA)!=0){
            fprintf(stderr, "Errore nella creazione del signal handler");
            close(fd_conn);
            REMOVE_SOCKET();
            destroyWorkerPool(wpool,false);
            free(stop);
            return EXIT_FAILURE;
        }

        //itero fino a che stop!=1, fino a che non ho mandato tutti i file 
        //o se il Collector è terminato per un qualunque motivo inaspettato
        int closed_child=0;
        while(!(*stop) && (closed_child=waitpid(process_id, NULL,WNOHANG))!=process_id){
            if(fileindex >= argc){
                //ho letto tutti i file passati come argomento è tempo di leggere la cartella 
                if(dir_name != NULL){
                    //cerco nella cartella
                }else{
                    //non ho passato alcuna cartella, quindi posso uscire
                    *stop==1;
                    continue;
                }
            }
            worker_arg w_arg;
            //controllo che il file sia regolare prima di inserirlo in coda 
            if(isFile(argv[fileindex])){
                w_arg.file=argv[fileindex];
                w_arg.wpool=wpool;
            }else{
                fprintf(stderr, "Il file %s non è conforme per le operazioni\n");
                fileindex++;
                continue;
            }
            //aggiungo la task in coda e valuto il valore di ritorno
            int check_add = addTask(wpool, (void*)&w_arg);

            if(check_add==0){
                //l'aggiunta alla coda è andta a buon fine, incremento l'inice di scorrimento dell'array di files
                ++fileindex;
                //controllo se deve dormire il masterthread
                int dormi=tdelay;
                while(dormi>0 && *stop==0){
                    //faccio dormire il thread un secondo e poi ricontrollo se devo eseguire cose
                    sleep(1);
                    --dormi;                   
                }
            }else{
                //il file non è stato inserito nella coda
                if(check_add==1){
                    //ritona 1 quando la coda è piena, o qunaod si è in fase di uscita, quindi non si accettano più task
                    continue;
                }else{
                    //check_add==-1, vuol dire che c'è stato un errore grave nella addTask
                    //chiudo tutto
                    *stop=1;
                    pthread_kill(signal_handler,SIGTERM);
                    pthread_join(signal_handler,NULL);
                    close(fd_conn);
                    REMOVE_SOCKET();
                    return EXIT_FAILURE;
                }
            }
        }

        //sono uscito dal while controllo perchè 
        if (closed_child == process_id){
            //il collector è terminato prima che il masterthread finisse di mandargli le task 
            return EXIT_FAILURE;
        }else{
            //l'uscita dal while è dovuta al cambiamento del valore di stop quindi eseguo la procedeura di chiusura della pool regolare
            destroyWorkerPool(wpool, true);
            //segnalo al collector che ho terminato
            pthread_mutex_lock(&(wpool->conn_mutex));
            while(wpool->can_write==0){
                pthread_cond_wait(&(wpool->conn_cond), &(wpool->conn_mutex));
            }
            wpool->can_write=0;//impedisco la scrittura
            if(writen(fd_conn, "stop", 5)<0){
                //scrittura non adata a buon fine 
                perror("write()");
                return EXIT_FAILURE;
            }
            wpool->can_write=1;//ripermetto la scrittura
            pthread_cond_signal(&(wpool->conn_cond));
            pthread_mutex_unlock(&(wpool->conn_mutex));
            //aspetto che termini il collector er chiudere tutto
            waitpid(process_id,NULL,0);
            *stop=1;
            pthread_kill(signal_handler,SIGTERM);
            pthread_join(signal_handler,NULL);
            free(stop);
            REMOVE_SOCKET();
            return EXIT_SUCCESS;
        }

        
    }//fine istruzioni processo padre

    //qui non ci dovrebbe arrivare mai
    return EXIT_FAILURE;
}


