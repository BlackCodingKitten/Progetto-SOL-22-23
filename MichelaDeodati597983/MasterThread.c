#define _POSIX_C_SOURCE 2001112L
/**
 * @file MasterThread.c
 * @author Michela Deodati 597983
 * @brief implementazione dell'intefraccia MasterThread
 * @version 0.1
 * @date 15-03-1013
 * 
 * 
 */
#define _POSIX_C_SOURCE 200112L
#define _OPEN_SYS_ITOA_EXT
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
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <signal.h>

#include <MasterThread.h>
#include <Util.h>

int isFile(const string filePath){
    struct stat path_stat;
    if(stat(filePath,&path_stat.st_mode)!=0){
        return 0;
    }
    return S_ISREG(path_stat.st_mode);
}

string getProjectDirectory(void){
    string path=malloc(sizeof(char)*PATH_LEN);
    getcwd(path,PATH_LEN);
    int n =  strlen(path)-strlen(strrchr(path,'/'));
    string tmp = malloc(sizeof(char)*PATH_LEN);
    memset(tmp,'\0', PATH_LEN);
    strncpy(tmp,path,n);
    free(path);
    return tmp;
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

void runMasterThread(int argc,string argv[]){
    int n_nthread;
    int q_queueLen;
    int t_delay;
    string d_directoryName =(string)malloc(sizeof(char)*PATH_LEN);

    int totFile=argc; //parto dal considerare che tutti gli elementi passati siano file

    for(int i=0; i<argc; i++){
        if(strcmp(argv[i],"-n")==0){
            totFile=totFile-2; // tolgo 2 elementi che non sono file
            n_nthread=checkNthread(StringToNumber(argv[i+1]));
            ++i;
        }
        if(strcmp(argv[i],"-q")==0){
            totFile=totFile-2;
            q_queueLen=checkqSize(StringToNumber(argv[i+1]));
            ++i;
        }
        if(strcmp(argv[i],"-t")==0){
            totFile=totFile-2;
            n_nthread=checkDelay(StringToNumber(argv[i+1]));
            ++i;
        }
        if(strcmp(argv[i],"-d")==0){
            totFile=totFile-2;
            memset(d_directoryName,'\0',PATH_LEN);
            strncpy(d_directoryName, argv[i+1],strlen(argv[i+1]));
            ++i;
        }
    }
    //ho settato tutti i valori passati a riga di comando della threadpool
    //ora devo cerarmi un elenco di file (ricavando le loro path)

    //ricavo prima la path da cui inizialre la ricerca dei file
    string path= getProjectDirectory();

    string * appo = malloc(sizeof(string)*100);
    for(int i=0; i<100; i++){
        appo[i]=NULL;//inizializzo appo
    }
    findFileDir(d_directoryName,appo,0);
    int b=0;
    while(appo[b]!=NULL){
        ++b;
    }
    totFile=totFile+b; //adesso conosco il valore preciso di quanti file ci sono cioè [(argc - (opzioni+argomenti))+file nella directory]
    taskArgument * fileArray =(taskArgument*)malloc(sizeof(taskArgument)*40);
    int indexCounter=0;
    for(int i=0; i<argc; i++){
        if(strstr(argv[i],"file") && strstr(argv[i],".dat")){
            //ricerco il file e lo salvo nell'array
            strcpy(fileArray[i].name, argv[i]);
            strcpy(fileArray[i].path,searchFile(path,argv[i]));
            ++indexCounter;
        }
    }
    for(int i=0; i<b; i++){
        strcpy(fileArray[indexCounter].name, appo[i]);
        strcpy(fileArray[indexCounter].path, realpath(appo[i],NULL));
        free(appo[i]);
    }
    free(appo);
    
    
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

int isFile(const char* filePath){
    struct stat path_stat;
    if(stat(filePath,&path_stat)!=0){
        perror("stat");
        return 0;
    }
    return S_ISREG(path_stat.st_mode) && strstr(filePath,".dat");
}

char* searchFile (char* dirname, char* filename){
    //apro la cartella chiamata dirname 
    DIR* directory;
    if((directory=opendir(dirname))==NULL){
        perror("Impossibile trovare il percorso del file");
        exit(EXIT_FAILURE);
    }
    //esploro la cartella 
    struct dirent * dirptr;
    while(errno=0, (dirptr=readdir(directory))!=NULL){
        //escludo ./ e ../
        if(dirptr->d_name[0]=='.'){
            continue;
        }
        char* path =(char*)malloc(sizeof(char)*PATH_LEN);
        strcpy(path,dirname);
        strcat(path,"/");
        strcat(path, dirptr->d_name);

        struct stat infodir;
        if(stat(path,&infodir)==-1){
            perror(path);
            exit(EXIT_FAILURE);
        }
        if(strcmp(dirptr->d_name,filename)==0){
            return path;
        }
        if(S_ISDIR(infodir.st_mode)){
            char* ret=malloc(sizeof(char)*255);
            if((ret = searchFile(path,filename))!=NULL){

                    return ret;                
            }else{
                free(path);
                continue;
            }
        }
    }
    closedir(directory);
    return NULL;
}

int findFileDir (const char* dirName, char** saveFile, int index){
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