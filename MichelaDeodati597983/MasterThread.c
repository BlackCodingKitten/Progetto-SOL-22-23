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

    for(int i=0; i<argc; i++){
        if(strstr(argv[i], "-n")){
            n_nthread=checkNthread(StringToNumber(argv[i+1]));
            ++i;
        }
        if(strstr(argv[i], "-q")){
            q_queueLen=checkqSize(StringToNumber(argv[i+1]));
            ++i;
        }
        if(strstr(argv[i], "-t")){
            n_nthread=checkDelay(StringToNumber(argv[i+1]));
            ++i;
        }
    }
    //ho settato tutti i valori passati a riga di comando della threadpool
    //ora devo cerarmi un elenco di file (ricavando le loro path)

    //ricavo prima la path da cui inizialre la ricerca dei file
    string path= getProjectDirectory();
    
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
