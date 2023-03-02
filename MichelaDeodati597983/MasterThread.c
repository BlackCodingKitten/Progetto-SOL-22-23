#define _POSIX_C_SOURCE 2001112L
#define _OPEN_SYS_ITOA_EXT
/**
 * @file MasterThread.c
 * @author Michela Deodati 597983
 * @brief implementazione dell'intefraccia MasterThread
 * @version 0.1
 * @date 15-03-1013
 * 
 * 
 */

#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <getopt.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <signal.h>

#include <MasterThread.h>
#include <WorkerPool.h>
#include <Util.h>




void runMasterThread(int argc,string argv[]){
    int n_nthread = NTHREAD_DEFAULT;
    int q_queueLen= QUEUE_SIZE_DEFAULT;
    int t_delay= DELAY_DEFAULT;
    string d_directoryName =(string)malloc(sizeof(char)*PATH_LEN);


    //getopt per controllare gli argomanti del main
    int fileIndex=1; //tiene traccia dell'indice di argv[] in cui si trovano i file

    int opt;
    opterr=0;

    int argvalue;

    //inizio la scan
    while((opt=getopt(argc, argv, "n:q:d:t:"))!=-1){
        //switch su opt:
        switch (opt)
        {
        case 'n':
        fileIndex+=2;
            //printf("NUMERO DI THREAD: %s\n", optarg);
            argvalue=StringToNumber(optarg);
            n_nthread=checkNthread(argvalue);
            break;
        case 'q':
        fileIndex+=2;
            //printf("LUNGHEZZA DELLA CODA: %s\n", optarg);
            argvalue=StringToNumber(optarg);
            q_queueLen=checkqSize(argvalue);
            break;
        case 't':
        fileIndex+=2;
            //printf("DELAY: %s\n", optarg);
            argvalue=StringToNumber(optarg);
            t_delay=checkDelay(argvalue);
            break;
        case 'd':
        fileIndex+=2;
            //printf("DIRECTORY: %s\n", optarg);        
            //dentro optarg ho il nome della cartella quindi:
            memset(d_directoryName, '\0', PATH_LEN);
            strcpy(d_directoryName,optarg);
        default:
            //printf("sono in default\n");
            //NON FACCIO NULLA 
            break;
        }
    }//end while optarg

    string*tmp=(string)malloc(sizeof(string)*20);
    for(int i=0; i<20; i++){
        tmp[i]=NULL;
    }
    findFileDir(d_directoryName,tmp,0);
    int x=0;
    for(;tmp[x]!=NULL;){
        ++x;
    }

    int dim=(argc-fileIndex)+x;
    string*argarray=malloc(sizeof(string)*dim);
    int i=0;
    for(i=0; fileIndex<argc; i++){
        argarray[i]=malloc(sizeof(char)*PATH_LEN);
        memset(argarray[i],'\0', PATH_LEN);
        strcpy(argarray[i], argv[fileIndex]);
        ++fileIndex;
    }
    for(int k=0;k<x;k++){
        argarray[i]=malloc(sizeof(char)*PATH_LEN);
        memset(argarray[i],'\0', PATH_LEN);
        strcpy(argarray[i], tmp[k]);
        free(tmp[i]);
        ++i;
    }
    free(tmp);

    workerpool_t *wpool=NULL;
    
    //controllo che nella creazione della threadpool vada tutto bene
    if((wpool =createWorkerpool(n_nthread,q_queueLen))==NULL){
        fprintf(stderr, "ERRORE FATALE NELLA CREAZIONE DELLA THREADPOOL\n");
        unlink(SOCKET_NAME);
        exit(EXIT_FAILURE);
    }

    //gestisco il signal handler


    
    
}

int checkNthread(const int nthread){
    //controllo che il valore di nthread passato al main sia >0;
    return ((nthread>0)? nthread : NTHREAD_DEFAULT);
}

int checkqSize (const int qsize){
    //controllo che il valore della lunghezza della coda condivisa sia >0
    return((qsize>0)? qsize:QUEUE_SIZE_DEFAULT);
}

int checkDelay (const int time){
    //controllo che il delay specificato sia >=0
    if((time>0)? time: DELAY_DEFAULT);
}

int isFile(const string filePath){
    struct stat path_stat;
    if(stat(filePath,&path_stat)!=0){
        perror("stat");
        return 0;
    }
    return S_ISREG(path_stat.st_mode) && strstr(filePath,".dat");
}


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

void findFileDir (const char* dirName, char** saveFile, int index){
    char** dirTree=malloc(sizeof(char*)*100);
    int treeI=0;
    struct stat buf;
    if(stat(dirName,&buf)==-1){
        perror("facendo la stat");
        fprintf(stderr, "Errore nella directory %s\n", dirName);
        exit(EXIT_FAILURE);
    }
    DIR*d;
    //fprintf(stdout, "SONO NELLA DIRECTORY: %s\n", dirName);

    if((d=opendir(dirName))==NULL){
        perror("opendir");
        fprintf(stderr, "errore nella directory %s\n", dirName);
        exit(EXIT_FAILURE);
    }else{
        struct dirent * f;
        while ((errno=0, f=readdir(d))!=NULL){
            struct stat buf;
            char * filename =malloc(sizeof(char*)*PATH_LEN);
            memset(filename,'\0',PATH_LEN);
            strncpy(filename,dirName, PATH_LEN);
            strncat(filename,"/", PATH_LEN);
            strncat(filename,f->d_name,PATH_LEN);
            if(stat(filename,&buf)==-1){
                perror("eseguendo la stat");
                fprintf(stderr, "erre in %s", filename);
                exit(EXIT_FAILURE);
            }
            if(S_ISDIR(buf.st_mode)){
                if(strlen(filename)>0 && (filename[strlen(filename)-1]!='.')){
                    dirTree[treeI]=malloc(sizeof(char)*PATH_LEN);
                    strncpy(dirTree[treeI], filename, PATH_LEN);
                    ++treeI;
                }
            }else{
                if(strstr(f->d_name,".dat")&& strstr(f->d_name,"file")){
                    saveFile[index]=malloc(sizeof(char)*PATH_LEN);
                    memset(saveFile[index],'\0',PATH_LEN);
                    strncpy(saveFile[index], filename, PATH_LEN);
                    ++index;   
                }
            }
        }
        if(errno!=0){
            perror("readdir");
            fprintf(stderr, "readdir della cartella %s fallito", dirName);
            closedir(d);
            exit(EXIT_FAILURE);
        }
        for(int k=0; k<treeI; k++){
            findFileDir(dirTree[k], saveFile,index);
            free(dirTree[k]);
        }                 
        free(dirTree);
        closedir(d);
    }
}